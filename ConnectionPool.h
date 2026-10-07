//---------------------------------------------------------------------------
// ConnectionPool.h — pool de conexiones ODBC profesional
//
// Características:
//   - Límite máximo de conexiones (maxConnections_).
//   - Timeout de adquisición (acquireTimeout_).
//   - Validación de conexión antes de entregar.
//   - Cola FIFO de conexiones libres (O(1) en GetConnection).
//   - Hilo de limpieza de conexiones ociosas (configurable).
//   - Warm-up inicial.
//   - Estadísticas de uso (adquisiciones, esperas, timeouts, etc.).
//   - Cierre de conexiones fuera del lock.
//   - Descarte de conexiones corruptas.
//
// Thread-safety: sí, todas las operaciones públicas son thread-safe.
//---------------------------------------------------------------------------
#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include "FCTString.h"

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>

class CTDataBase;

//---------------------------------------------------------------------------
// Configuración del pool
//---------------------------------------------------------------------------
struct ConnectionPoolConfig
{
    // Número máximo de conexiones. 0 = sin límite (no recomendado).
    std::size_t maxConnections = 20;

    // Número mínimo de conexiones a mantener (warm-up).
    std::size_t minConnections = 2;

    // Timeout por defecto para GetConnection.
    std::chrono::milliseconds defaultAcquireTimeout{30000};

    // Tiempo máximo que una conexión puede estar ociosa antes de cerrarse.
    std::chrono::minutes idleTimeout{60};

    // Intervalo del hilo de limpieza.
    std::chrono::minutes cleanupInterval{5};

    // Si true, valida la conexión con "SELECT 1" antes de entregarla.
    bool validateOnAcquire = true;
};

//---------------------------------------------------------------------------
// Estadísticas del pool
//---------------------------------------------------------------------------
struct ConnectionPoolStats
{
    std::size_t total;          // Conexiones actualmente en el pool
    std::size_t inUse;          // Conexiones en uso
    std::size_t available;      // Conexiones disponibles
    std::size_t closed;         // Conexiones cerradas (pero en el pool)

    std::uint64_t totalAcquires;    // Adquisiciones totales
    std::uint64_t totalWaits;       // Veces que un hilo tuvo que esperar
    std::uint64_t totalTimeouts;    // Veces que un hilo agotó el timeout
    std::uint64_t totalCreated;     // Conexiones creadas en total
    std::uint64_t totalDestroyed;   // Conexiones cerradas/descartadas
};

//---------------------------------------------------------------------------
// ConnectionPool
//---------------------------------------------------------------------------
class ConnectionPool
{
public:
    using AcquireCallback = std::function<void(CTDataBase*)>;

    explicit ConnectionPool(const ConnectionPoolConfig& config = {});
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    //--- Configuración ---
    void SetConfig(const ConnectionPoolConfig& config);
    ConnectionPoolConfig GetConfig() const;

    //--- Warm-up: pre-abre minConnections conexiones ---
    void WarmUp(const CTString& param);

    //--- Adquisición ---
    // Devuelve una conexión lista para usar, o nullptr si:
    //   - Se agotó el timeout, o
    //   - No se pudo abrir una nueva.
    CTDataBase* GetConnection(const CTString& param);

    // Versión con timeout explícito.
    CTDataBase* GetConnection(const CTString& param,
                              std::chrono::milliseconds timeout);

    //--- Devolución ---
    void Disconnected(CTDataBase* connection);

    //--- Limpieza ---
    void CleanupConnections();

    //--- Estadísticas ---
    ConnectionPoolStats GetStats() const;

    //--- Cierre global ---
    void CloseAll();

private:
    //--- Configuración ---
    ConnectionPoolConfig config_;

    //--- Estado ---
    mutable std::mutex      mutex_;
    std::condition_variable cv_;

    std::vector<CTDataBase*> all_;        // todas las conexiones (para cierre global)
    std::queue<CTDataBase*>  available_;  // conexiones libres (FIFO)

    std::thread              cleanupThread_;
    std::atomic<bool>        cleanupThreadActive_;
    std::condition_variable  cleanupCV_;

    //--- Contadores ---
    mutable std::atomic<std::uint64_t> statAcquires_{0};
    mutable std::atomic<std::uint64_t> statWaits_{0};
    mutable std::atomic<std::uint64_t> statTimeouts_{0};
    mutable std::atomic<std::uint64_t> statCreated_{0};
    mutable std::atomic<std::uint64_t> statDestroyed_{0};

    //--- Helpers internos ---
    void CleanupLoop();

    // Crea una conexión nueva con el parámetro indicado. Devuelve nullptr si falla.
    // No toma el mutex; el llamante debe gestionar la sincronización.
    CTDataBase* CreateConnectionLocked(const CTString& param);

    // Cierra y descarta una conexión del pool.
    // Requiere mutex tomado. La conexión NO se cierra bajo el lock: se encola
    // para cierre diferido. Devuelve la conexión para que el llamante la cierre
    // fuera del lock si lo desea.
    CTDataBase* DetachConnectionLocked(CTDataBase* connection);

    // Valida una conexión (SELECT 1). No toma el mutex.
    bool ValidateConnection(CTDataBase* c) const;
};

#endif // CONNECTIONPOOL_H
