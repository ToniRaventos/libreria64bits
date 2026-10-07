//---------------------------------------------------------------------------
// TDataBase.h — acceso a ODBC portable (16/32/64 bits, Windows/POSIX)
//
// - ODBC 3.x (sql.h + sqlext.h), portable.
// - Sin vcl.h, sin HWND, sin WM_USER, sin SendMessage.
// - Drivers enumerados en runtime con SQLDriversA().
// - Enum de drivers con nombres significativos.
// - Estado consolidado en CTDbState.
// - Debug vía callback std::function.
// - Pool de conexiones robusto y thread-safe.
//---------------------------------------------------------------------------
#ifndef TDATABASE_H
#define TDATABASE_H

#include "FCTLib.h"
#include "FCTString.h"

//---------------------------------------------------------------------------
// ODBC: en Windows, sql.h y sqlext.h dependen de tipos y macros que define
// <windows.h>. Sin él, sqltypes.h falla con "unknown type name 'INT64'",
// "unknown type name 'DWORD'", "unknown type name '_Out_'" y similares.
//
// En POSIX, unixODBC ya trae todo lo necesario.
//---------------------------------------------------------------------------
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <cstdint>
#include <cstddef>

class CTSqlTable;

//---------------------------------------------------------------------------
// Enum de drivers. Coincide con el índice de Txt_Driver[].
//---------------------------------------------------------------------------
enum class CTDriver : int
{
    SqlServer18     = 0,   // "ODBC Driver 18 for SQL Server"
    SqlServer17     = 1,   // "ODBC Driver 17 for SQL Server"
    SqlServerLegacy = 2,   // "SQL Server" (obsoleto pero a veces necesario)
    SQLite          = 3,   // "SQLite3 ODBC Driver"
    PostgreSQL      = 4,   // "PostgreSQL Unicode(x64)"
    MySQL           = 5,   // "MySQL ODBC 8.0 Unicode Driver"
    MariaDB         = 6,   // "MariaDB ODBC 3.1 Driver"
    Access          = 7,   // "Microsoft Access Driver (*.mdb, *.accdb)"
    Excel           = 8,   // "Microsoft Excel Driver (*.xls, *.xlsx)"
    Pervasive       = 9,   // "Pervasive ODBC Client Interface"
    Unknown         = -1
};

// Lista textual de drivers (definida en el .cpp).
extern const char* const Txt_Driver[];

// Número de entradas válidas de Txt_Driver.
constexpr int Txt_Driver_Count = 10;

// Conversión nombre <-> enum
CTDriver    DriverFromName(const std::string& name);
const char* DriverEnumName(CTDriver d);

//---------------------------------------------------------------------------
// Estado consolidado de la conexión.
//---------------------------------------------------------------------------
enum class CTDbState : int
{
    Disconnected = 0,   // Nunca abierta o cerrada
    Connected    = 1,   // Abierta y usable
    InUse        = 2,   // Abierta y en uso (por el pool)
    Closed       = 3,   // Cerrada explícitamente
    Error        = 4    // Última operación falló
};

//---------------------------------------------------------------------------
// CTDataBase
//---------------------------------------------------------------------------
class CTDataBase : public CTLib
{
private:
    std::chrono::system_clock::time_point lastUsedTime_;
    std::function<void(const std::string& sql, bool isWide)> debugCallback_;

    // Estado consolidado. isOpen, isClosed_, isInUse_ se derivan de aquí.
    CTDbState state_;

    // Cadena de conexión efectiva usada en la última Open() exitosa.
    // El pool la reutiliza para reconectar.
    std::string effectiveConnectionString_;

    int OpenConect();
    int OpenInternal();

    // Constructores específicos de cadena de conexión por driver.
    std::string BuildConnectionStringSqlServer() const;
    std::string BuildConnectionStringOther() const;
    std::string BuildConnectionStringFromParams() const;

public:
    // --- Estado público (compatibilidad) ---
    bool    borrar;
    RETCODE ERR;
    bool    isOpen;

    SQLHENV henv;
    SQLHDBC hdbc;

    // --- Parámetros de conexión ---
    std::string Mandt;
    CTDriver    Driver;
    std::string ALIAS;
    std::string Server;
    std::string UID;
    std::string PWD;
    std::string DBQ;
    std::string PORT;
    std::string APP;
    bool        AutoTrans;
    std::string Extern;
    std::string PathCache;
    std::string FileNameReturn;

    CTString    Params;
    bool        isUnicConex;
    std::string DriverLiteral;

    // --- Ciclo de vida ---
    explicit CTDataBase(void* owner = nullptr);
    ~CTDataBase() override;

    CTDataBase(const CTDataBase&)            = delete;
    CTDataBase& operator=(const CTDataBase&) = delete;

     // Asigna el driver por nombre literal. Cualquier nombre que el
    // driver manager de ODBC reconozca.
    void SetDriverLiteral(const std::string& name) { DriverLiteral = name; }
    // --- Configuración ---
    void SetUnicConex(bool vl);
    CTDataBase* NewConex(CTDataBase* db);

    // Init con enum (preferida)
    int Init(CTDriver driver,
             std::string alias,
             std::string server,
             std::string user,
             std::string pas,
             std::string db,
             std::string port,
             std::string externo = "");

    // Init con índice entero (compatibilidad)
    int Init(int driver,
             std::string alias,
             std::string server,
             std::string user,
             std::string pas,
             std::string db,
             std::string port,
             std::string externo = "");

    // Init con nombre de driver (para configuraciones JSON)
    int Init(const std::string& driverName,
             std::string alias,
             std::string server,
             std::string user,
             std::string pas,
             std::string db,
             std::string port,
             std::string externo = "");

    // --- Conexión ---
    int  Open();
    int  Open(CTString param);
    void Close();

    bool Reconnect();

    // --- Ejecución ---
    int  Cursor(const char* sql, SQLHSTMT* hstmt);
    int  Cursor(const std::string& sql, SQLHSTMT* hstmt);
    void CloseCursor(SQLHSTMT* hstmt);

    bool IsConnectionAlive();

    // --- Utilidades ---
    CTDataBase* FindDB(std::string name);
    void        SetParameters(std::string mandt, std::string path, std::string ext);

    // --- Debug ---
    void SetDebugCallback(std::function<void(const std::string&, bool)> cb);
    void SendMsgDebuger(CTSqlTable* tb);

    // --- Estado (para pool y compatibilidad) ---
    void SetInUse(bool value);
    bool IsInUse() const;

    void MarkAsClosed();
    bool IsClosed() const;

    void UpdateLastUsedTime();
    std::chrono::system_clock::time_point GetLastUsedTime() const;

    CTDbState GetState() const { return state_; }

    // --- Cadena de conexión efectiva ---
    std::string EffectiveConnectionString() const { return effectiveConnectionString_; }

    // --- Diagnóstico ---
    RETCODE     LastError() const    { return ERR; }
    std::string LastErrorMessage() const;

    // --- Metadatos ---
    static bool                     IsValidDriver(CTDriver d);
    static std::string              DriverName(CTDriver d);
    static std::vector<std::string> ListOdbcDrivers();
};

#endif // TDATABASE_H


