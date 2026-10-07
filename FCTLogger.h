//---------------------------------------------------------------------------
// FCTLogger.h — logger portable (16/32/64 bits, sin Win32 obligatorio)
//
// - El fichero se rota por día: log_YYYY-MM-DD.txt
// - Thread-safe: un único mutex protege fichero + fecha.
// - jsoncpp es opcional: define CTLIB_WITH_JSONCPP para habilitar logToJson.
//
// Uso típico:
//     Logger_Start("C:/logs");
//     Logger("hola", 0);
//     Logger_Stop();
//---------------------------------------------------------------------------
#ifndef FCTLOGGER_H
#define FCTLOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

#ifdef CTLIB_WITH_JSONCPP
    #include <json/json.h>
#endif

//---------------------------------------------------------------------------
// Tipos de registro
//---------------------------------------------------------------------------
enum class LogType : int
{
    INFO    = 0,
    IOT     = 1,
    WARNING = 2,
    JSON    = 3,
    SNAP    = 4,
    SYSTEM  = 5,
    SQL     = 6,
    GET     = 7,
    POST    = 8,
    PILA    = 9,
    XGET    = 10,
    XPOST   = 11,
    HTTP    = 12
};

//---------------------------------------------------------------------------
// Clase logger
//---------------------------------------------------------------------------
class CTLogger
{
private:
    std::ofstream  file_;
    std::mutex     mutex_;              // protege fichero + fecha + Path
    std::string    current_date_;

    // Requiere que el mutex esté tomado por el llamante.
    void        UpdateLogFileLocked();
    static std::string LogTypeToString(int type);
    static std::tm     LocalTime(std::time_t t);

public:
    bool        Cmd;    // true = además de fichero, escribe por stdout
    std::string Path;   // carpeta destino (sin separador final obligatorio)

    explicit CTLogger(const std::string& path = "");
    virtual ~CTLogger() noexcept;

    // No copiable ni movible: contiene un mutex y un ofstream.
    CTLogger(const CTLogger&)            = delete;
    CTLogger& operator=(const CTLogger&) = delete;
    CTLogger(CTLogger&&)                 = delete;
    CTLogger& operator=(CTLogger&&)      = delete;

    void Log(int type, const std::string& message);

#ifdef CTLIB_WITH_JSONCPP
    Json::Value logToJson(const std::string& filter);
#endif
};

//---------------------------------------------------------------------------
// API global (por compatibilidad con el resto del proyecto)
//---------------------------------------------------------------------------
void Logger_Start(const std::string& path);
void Logger_Stop();
void Logger_Path (const std::string& path);
void Logger_cmd  (const std::string& cmd);
void Logger      (const std::string& msg, int type = 0);

#ifdef CTLIB_WITH_JSONCPP
Json::Value Logger_json(const std::string& filter);
#endif

#endif // FCTLOGGER_H
