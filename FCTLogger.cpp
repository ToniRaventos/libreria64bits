//---------------------------------------------------------------------------
// FCTLogger.cpp — implementación portable
//---------------------------------------------------------------------------
#include "FCTLogger.h"
#include "General.h"    // toLower

#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

//---------------------------------------------------------------------------
// Estado global (encapsulado en unique_ptr)
//---------------------------------------------------------------------------
namespace {

std::unique_ptr<CTLogger> g_logger;
std::mutex                g_logger_mutex;

CTLogger* GetLogger()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    return g_logger.get();
}

} // namespace

//===========================================================================
// API global
//===========================================================================
void Logger_Start(const std::string& path)
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    g_logger.reset(new CTLogger(path));
}

void Logger_Stop()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    g_logger.reset();
}

void Logger_Path(const std::string& path)
{
    CTLogger* lg = GetLogger();
    if (!lg) return;

    // Normalizar: aseguramos separador final si no lo hay.
    fs::path p(path);
    lg->Path = p.string();
    if (!lg->Path.empty() && lg->Path.back() != '/' && lg->Path.back() != '\\')
        lg->Path += '/';
}

void Logger_cmd(const std::string& cmd)
{
    CTLogger* lg = GetLogger();
    if (!lg) return;
    lg->Cmd = (cmd == "reg_on");
}

void Logger(const std::string& msg, int type)
{
    CTLogger* lg = GetLogger();
    if (!lg) return;   // sin logger configurado: no hacemos nada
    lg->Log(type, msg);
}

#ifdef CTLIB_WITH_JSONCPP
Json::Value Logger_json(const std::string& filter)
{
    CTLogger* lg = GetLogger();
    if (!lg) return Json::Value(Json::arrayValue);
    return lg->logToJson(filter);
}
#endif

//===========================================================================
// CTLogger
//===========================================================================
CTLogger::CTLogger(const std::string& path)
    : Cmd(false), Path(path)
{
    // Normalizar: separador final.
    if (!Path.empty() && Path.back() != '/' && Path.back() != '\\')
        Path += '/';
}

CTLogger::~CTLogger() noexcept
{
    try
    {
        if (file_.is_open())
            file_.close();
    }
    catch (...)
    {
        // Silenciar errores en el destructor.
    }
}

//---------------------------------------------------------------------------
// Hora local thread-safe
//---------------------------------------------------------------------------
std::tm CTLogger::LocalTime(std::time_t t)
{
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

//---------------------------------------------------------------------------
// Log
//---------------------------------------------------------------------------
void CTLogger::Log(int type, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    UpdateLogFileLocked();

    const auto now = std::chrono::system_clock::now();
    const std::time_t ts = std::chrono::system_clock::to_time_t(now);
    const std::tm tm = LocalTime(ts);

    std::ostringstream oss;
    oss << "[" << LogTypeToString(type) << "]"
        << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
        << message;

    const std::string line = oss.str();

    if (file_.is_open())
    {
        file_ << line << std::endl;
    }

    if (Cmd)
    {
        std::cout << line << std::endl;
    }
}

//---------------------------------------------------------------------------
// Rotación por día
//---------------------------------------------------------------------------
void CTLogger::UpdateLogFileLocked()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t ts = std::chrono::system_clock::to_time_t(now);
    const std::tm tm = LocalTime(ts);

    std::ostringstream date_oss;
    date_oss << std::put_time(&tm, "%Y-%m-%d");
    const std::string new_date = date_oss.str();

    if (new_date == current_date_ && file_.is_open())
        return;   // ya estamos en el fichero correcto

    if (file_.is_open())
        file_.close();

    std::ostringstream name_oss;
    name_oss << Path << "log_" << new_date << ".txt";

    file_.open(name_oss.str(), std::ios::app);
    current_date_ = new_date;
}

//---------------------------------------------------------------------------
// Tipos
//---------------------------------------------------------------------------
std::string CTLogger::LogTypeToString(int type)
{
    switch (type)
    {
        case 0:  return "INFO";
        case 1:  return "IOT";
        case 2:  return "WARNING";
        case 3:  return "JSON";
        case 4:  return "SNAP";
        case 5:  return "SYSTEM";
        case 6:  return "SQL";
        case 7:  return "GET";
        case 8:  return "POST";
        case 9:  return "PILA";
        case 10: return "XGET";
        case 11: return "XPOST";
        case 12: return "HTTP";
        default: return "UNKNOWN";
    }
}

//---------------------------------------------------------------------------
// JSON (opcional)
//---------------------------------------------------------------------------
#ifdef CTLIB_WITH_JSONCPP
Json::Value CTLogger::logToJson(const std::string& filter)
{
    Json::Value logArray(Json::arrayValue);

    std::lock_guard<std::mutex> lock(mutex_);

    // Asegurar que tenemos una fecha actual válida.
    if (current_date_.empty())
        UpdateLogFileLocked();

    // Si el fichero está abierto por el propio logger, cerrarlo temporalmente
    // no es necesario: sólo leemos, así que abrimos otro ifstream aparte.
    std::ostringstream name_oss;
    name_oss << Path << "log_" << current_date_ << ".txt";

    std::ifstream logFile(name_oss.str());
    if (!logFile.is_open())
    {
        std::cerr << "CTLogger: no se pudo abrir " << name_oss.str() << std::endl;
        return logArray;
    }

    const std::string lowerFilter = toLower(filter);

    std::string line;
    while (std::getline(logFile, line))
    {
        Json::Value logEntry(Json::objectValue);
        std::istringstream iss(line);
        std::string type, date, message;
        char discard = 0;

        if (!line.empty() && line[0] == '[')
        {
            iss >> discard;                       // '['
            std::getline(iss, type, ']');
            iss >> discard;                       // '['
            std::getline(iss, date, ']');
            std::getline(iss, message);           // resto (con espacio inicial)
        }
        else
        {
            message = line;
        }

        const std::string lowerType    = toLower(type);
        const std::string lowerDate    = toLower(date);
        const std::string lowerMessage = toLower(message);

        const bool match =
            lowerFilter.empty() ||
            lowerType.find(lowerFilter)    != std::string::npos ||
            lowerDate.find(lowerFilter)    != std::string::npos ||
            lowerMessage.find(lowerFilter) != std::string::npos;

        if (match)
        {
            logEntry["type"]    = type;
            logEntry["date"]    = date;
            logEntry["message"] = message;
            logArray.append(logEntry);
        }
    }
    return logArray;
}
#endif // CTLIB_WITH_JSONCPP
