//---------------------------------------------------------------------------
// ConnectionPool.cpp — implementación
//---------------------------------------------------------------------------
#include "ConnectionPool.h"
#include "TDataBase.h"

#include <algorithm>
#include <iostream>

//===========================================================================
// Ciclo de vida
//===========================================================================
ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : config_(config),
      cleanupThreadActive_(true)
{
    cleanupThread_ = std::thread(&ConnectionPool::CleanupLoop, this);
}

ConnectionPool::~ConnectionPool()
{
    // Parar el hilo de limpieza.
    cleanupThreadActive_ = false;
    cleanupCV_.notify_all();
    if (cleanupThread_.joinable())
        cleanupThread_.join();

    // Cerrar todas las conexiones.
    CloseAll();
}

//===========================================================================
// Configuración
//===========================================================================
void ConnectionPool::SetConfig(const ConnectionPoolConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    cleanupCV_.notify_all();   // puede que cambie el intervalo
}

ConnectionPoolConfig ConnectionPool::GetConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

//===========================================================================
// Warm-up
//===========================================================================
void ConnectionPool::WarmUp(const CTString& param)
{
    std::vector<CTDataBase*> created;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        const std::size_t target = std::min(config_.minConnections,
                                            config_.maxConnections);
        while (all_.size() < target)
        {
            CTDataBase* c = CreateConnectionLocked(param);
            if (!c) break;
            all_.push_back(c);
            available_.push(c);
            created.push_back(c);
        }
    }

    // Nada que hacer fuera del lock en este caso; CreateConnectionLocked
    // ya abre las conexiones antes de devolverlas.
}

//===========================================================================
// GetConnection (sin timeout explícito)
//===========================================================================
CTDataBase* ConnectionPool::GetConnection(const CTString& param)
{
    return GetConnection(param, config_.defaultAcquireTimeout);
}

//===========================================================================
// GetConnection (con timeout)
//===========================================================================
CTDataBase* ConnectionPool::GetConnection(const CTString& param,
                                          std::chrono::milliseconds timeout)
{
    statAcquires_.fetch_add(1, std::memory_order_relaxed);

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool waited = false;

    std::unique_lock<std::mutex> lock(mutex_);

    while (true)
    {
        // 1) ¿Hay alguna conexión disponible?
        while (!available_.empty())
        {
            CTDataBase* c = available_.front();
            available_.pop();

            // Validación: si está muerta, se descarta y se prueba la siguiente.
            if (config_.validateOnAcquire && !ValidateConnection(c))
            {
                // Detach (fuera del lock en cuanto salgamos).
                auto it = std::find(all_.begin(), all_.end(), c);
                if (it != all_.end()) all_.erase(it);
                statDestroyed_.fetch_add(1, std::memory_order_relaxed);

                // Cerrar fuera del lock sería ideal, pero aquí estamos dentro.
                // Como la conexión ya está rota, cerrarla rápido es aceptable.
                c->Close();
                delete c;
                continue;
            }

            // Marcar en uso y devolver.
            c->SetInUse(true);
            c->UpdateLastUsedTime();

            if (waited)
                statWaits_.fetch_add(1, std::memory_order_relaxed);

            return c;
        }

        // 2) No hay disponibles. ¿Podemos crear una nueva?
        if (config_.maxConnections == 0 || all_.size() < config_.maxConnections)
        {
            CTDataBase* c = CreateConnectionLocked(param);
            if (c)
            {
                all_.push_back(c);
                c->SetInUse(true);
                c->UpdateLastUsedTime();

                if (waited)
                    statWaits_.fetch_add(1, std::memory_order_relaxed);

                return c;
            }
            // Si CreateConnectionLocked falla, caemos al wait abajo.
        }

        // 3) Límite alcanzado. Esperar hasta que alguien devuelva.
        waited = true;

        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout)
        {
            statTimeouts_.fetch_add(1, std::memory_order_relaxed);
            return nullptr;
        }
        // Si despertamos por notify, volvemos al principio del while.
    }
}

//===========================================================================
// Disconnected
//===========================================================================
void ConnectionPool::Disconnected(CTDataBase* connection)
{
    if (!connection) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Solo devolvemos al pool si sigue perteneciendo a él.
    if (std::find(all_.begin(), all_.end(), connection) == all_.end())
        return;   // ya fue descartada

    connection->SetInUse(false);
    available_.push(connection);

    cv_.notify_one();   // despierta a un hilo esperando
}

//===========================================================================
// CleanupConnections
//===========================================================================
void ConnectionPool::CleanupConnections()
{
    std::vector<CTDataBase*> toClose;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto now = std::chrono::system_clock::now();
        auto it = all_.begin();
        while (it != all_.end())
        {
            CTDataBase* c = *it;
            if (!c)
            {
                it = all_.erase(it);
                continue;
            }

            // No cerrar las que están en uso.
            if (c->IsInUse())
            {
                ++it;
                continue;
            }

            // ¿Lleva demasiado tiempo ociosa?
            const auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                                     now - c->GetLastUsedTime()).count();
            if (elapsed >= config_.idleTimeout.count())
            {
                // Quitar de la cola de disponibles (puede que esté ahí o no).
                std::queue<CTDataBase*> tmp;
                while (!available_.empty())
                {
                    CTDataBase* front = available_.front();
                    available_.pop();
                    if (front != c) tmp.push(front);
                }
                available_.swap(tmp);

                toClose.push_back(c);
                it = all_.erase(it);
                statDestroyed_.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                ++it;
            }
        }
    }

    // Cerrar FUERA del lock.
    for (CTDataBase* c : toClose)
    {
        c->Close();
        delete c;
    }
}

//===========================================================================
// Estadísticas
//===========================================================================
ConnectionPoolStats ConnectionPool::GetStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    ConnectionPoolStats s{};
    s.total     = all_.size();
    s.available = available_.size();

    std::size_t inUse = 0;
    std::size_t closed = 0;
    for (CTDataBase* c : all_)
    {
        if (!c) continue;
        if (c->IsInUse()) inUse++;
        else if (c->IsClosed()) closed++;
    }
    s.inUse  = inUse;
    s.closed = closed;

    s.totalAcquires  = statAcquires_.load(std::memory_order_relaxed);
    s.totalWaits     = statWaits_.load(std::memory_order_relaxed);
    s.totalTimeouts  = statTimeouts_.load(std::memory_order_relaxed);
    s.totalCreated   = statCreated_.load(std::memory_order_relaxed);
    s.totalDestroyed = statDestroyed_.load(std::memory_order_relaxed);

    return s;
}

//===========================================================================
// CloseAll
//===========================================================================
void ConnectionPool::CloseAll()
{
    std::vector<CTDataBase*> toClose;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        toClose.swap(all_);
        std::queue<CTDataBase*> empty;
        available_.swap(empty);
    }

    for (CTDataBase* c : toClose)
    {
        if (!c) continue;
        c->Close();
        delete c;
        statDestroyed_.fetch_add(1, std::memory_order_relaxed);
    }
}

//===========================================================================
// Hilo de limpieza
//===========================================================================
void ConnectionPool::CleanupLoop()
{
    while (cleanupThreadActive_)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool stopping = cleanupCV_.wait_for(
                                  lock,
                                  config_.cleanupInterval,
                                  [this] { return !cleanupThreadActive_.load(); });

        if (!cleanupThreadActive_)
            break;

        // Si no fue notificado por parada, es timeout: limpiar.
        lock.unlock();
        if (!stopping)
            CleanupConnections();
    }
}

//===========================================================================
// Helpers internos
//===========================================================================
CTDataBase* ConnectionPool::CreateConnectionLocked(const CTString& param)
{
    CTDataBase* c = new CTDataBase(nullptr);

    // Nota: c->Open(param) puede tardar (ODBC). Como estamos dentro del lock,
    // el pool queda bloqueado durante la apertura. Si quieres evitarlo, se
    // podría abrir fuera del lock, pero entonces habría que reservar el hueco
    // antes y podría quedar una conexión huérfana si falla. Por simplicidad
    // y porque abrir una conexión ODBC no suele tardar más de ~200 ms, se
    // asume esta pequeña contención.
    if (c->Open(param) != 1)
    {
        delete c;
        return nullptr;
    }

    statCreated_.fetch_add(1, std::memory_order_relaxed);
    return c;
}

bool ConnectionPool::ValidateConnection(CTDataBase* c) const
{
    if (!c) return false;
    if (c->IsClosed()) return false;
    return c->IsConnectionAlive();
}
