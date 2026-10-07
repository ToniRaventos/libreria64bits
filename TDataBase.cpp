//---------------------------------------------------------------------------
// TDataBase.cpp — implementación portable
//---------------------------------------------------------------------------
#include "TDataBase.h"
#include "TSqlTable.h"
#include "General.h"

#include <cstring>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

//===========================================================================
// Conversión nombre <-> enum
//===========================================================================
CTDriver DriverFromName(const std::string& name)
{
    if (name == "SqlServer18")     return CTDriver::SqlServer18;
    if (name == "SqlServer17")     return CTDriver::SqlServer17;
    if (name == "SqlServerLegacy") return CTDriver::SqlServerLegacy;
    if (name == "SQLite")          return CTDriver::SQLite;
    if (name == "PostgreSQL")      return CTDriver::PostgreSQL;
    if (name == "MySQL")           return CTDriver::MySQL;
    if (name == "MariaDB")         return CTDriver::MariaDB;
    if (name == "Access")          return CTDriver::Access;
    if (name == "Excel")           return CTDriver::Excel;
    if (name == "Pervasive")       return CTDriver::Pervasive;
    return CTDriver::Unknown;
}

const char* DriverEnumName(CTDriver d)
{
    switch (d)
    {
        case CTDriver::SqlServer18:     return "SqlServer18";
        case CTDriver::SqlServer17:     return "SqlServer17";
        case CTDriver::SqlServerLegacy: return "SqlServerLegacy";
        case CTDriver::SQLite:          return "SQLite";
        case CTDriver::PostgreSQL:      return "PostgreSQL";
        case CTDriver::MySQL:           return "MySQL";
        case CTDriver::MariaDB:         return "MariaDB";
        case CTDriver::Access:          return "Access";
        case CTDriver::Excel:           return "Excel";
        case CTDriver::Pervasive:       return "Pervasive";
        case CTDriver::Unknown:         return "Unknown";
    }
    return "Unknown";
}

//---------------------------------------------------------------------------
// Tabla de drivers (los nombres deben coincidir con los instalados en el
// sistema; usa CTDataBase::ListOdbcDrivers() para verlos).
//---------------------------------------------------------------------------
const char* const Txt_Driver[] =
{
    "ODBC Driver 18 for SQL Server",
    "ODBC Driver 17 for SQL Server",
    "SQL Server",
    "SQLite3 ODBC Driver",
    "PostgreSQL Unicode(x64)",
    "MySQL ODBC 8.0 Unicode Driver",
    "MariaDB ODBC 3.1 Driver",
    "Microsoft Access Driver (*.mdb, *.accdb)",
    "Microsoft Excel Driver (*.xls, *.xlsx)",
    "Pervasive ODBC Client Interface"
};

//---------------------------------------------------------------------------
// Helpers internos
//---------------------------------------------------------------------------
namespace {

std::string odbc_diag(SQLSMALLINT handleType, SQLHANDLE handle)
{
    std::string result;
    SQLCHAR     state[6]   = {0};
    SQLCHAR     msg[1024]  = {0};
    SQLINTEGER  nativeErr  = 0;
    SQLSMALLINT msgLen     = 0;

    SQLSMALLINT rec = 1;
    while (SQLGetDiagRecA(handleType, handle, rec,
                          state, &nativeErr,
                          msg, sizeof(msg),
                          &msgLen) == SQL_SUCCESS)
    {
        std::ostringstream oss;
        oss << "[" << reinterpret_cast<const char*>(state) << "] "
            << reinterpret_cast<const char*>(msg)
            << " (native " << nativeErr << ")";
        if (!result.empty()) result += "\n";
        result += oss.str();
        ++rec;
    }
    return result;
}

} // namespace

//===========================================================================
// CTDataBase — metadatos
//===========================================================================
bool CTDataBase::IsValidDriver(CTDriver d)
{
    const int v = static_cast<int>(d);
    return v >= 0 && v < Txt_Driver_Count;
}

std::string CTDataBase::DriverName(CTDriver d)
{
    if (!IsValidDriver(d)) return "";
    return Txt_Driver[static_cast<int>(d)];
}

std::vector<std::string> CTDataBase::ListOdbcDrivers()
{
    std::vector<std::string> result;

    SQLHENV env = nullptr;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env) != SQL_SUCCESS)
        return result;

    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
                  reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);

    SQLCHAR desc[256] = {0};
    SQLCHAR attr[256] = {0};
    SQLSMALLINT descLen = 0;
    SQLSMALLINT attrLen = 0;
    SQLUSMALLINT direction = SQL_FETCH_FIRST;

    while (SQLDriversA(env, direction,
                       desc, sizeof(desc), &descLen,
                       attr, sizeof(attr), &attrLen) == SQL_SUCCESS)
    {
        result.emplace_back(reinterpret_cast<const char*>(desc));
        direction = SQL_FETCH_NEXT;
    }

    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return result;
}

//===========================================================================
// Ciclo de vida
//===========================================================================
CTDataBase::CTDataBase(void* owner)
    : CTLib(owner),
      lastUsedTime_(std::chrono::system_clock::now()),
      state_(CTDbState::Disconnected),
      borrar(false),
      ERR(SQL_SUCCESS),
      isOpen(false),
      henv(nullptr),
      hdbc(nullptr),
      Mandt(""),
      Driver(CTDriver::SqlServer18),
      AutoTrans(false),
      PathCache(fs::temp_directory_path().string()),
      isUnicConex(false)
{
}

CTDataBase::~CTDataBase()
{
    borrar = false;
    Close();
}

//===========================================================================
// Configuración
//===========================================================================
void CTDataBase::SetUnicConex(bool vl)
{
    isUnicConex = vl;
}

CTDataBase* CTDataBase::NewConex(CTDataBase* db)
{
    if (!db) return nullptr;

    CTDataBase* BB = new CTDataBase(nullptr);
    if (BB->Open(db->Params) == 0)
    {
        delete BB;
        return nullptr;
    }
    return BB;
}

int CTDataBase::Init(CTDriver driver,
                     std::string alias,
                     std::string server,
                     std::string user,
                     std::string pas,
                     std::string db,
                     std::string port,
                     std::string externo)
{
    borrar = true;
    Driver = driver;
    ALIAS  = std::move(alias);
    Server = std::move(server);
    UID    = std::move(user);
    PWD    = std::move(pas);
    DBQ    = std::move(db);
    PORT   = std::move(port);
    Extern = std::move(externo);
    Params = "";
    return 1;
}

int CTDataBase::Init(int driver,
                     std::string alias,
                     std::string server,
                     std::string user,
                     std::string pas,
                     std::string db,
                     std::string port,
                     std::string externo)
{
    // Compatibilidad: convertir el índice a enum si es válido.
    CTDriver d = IsValidDriver(static_cast<CTDriver>(driver))
                     ? static_cast<CTDriver>(driver)
                     : CTDriver::Unknown;
    return Init(d, std::move(alias), std::move(server), std::move(user),
                std::move(pas), std::move(db), std::move(port),
                std::move(externo));
}

int CTDataBase::Init(const std::string& driverName,
                     std::string alias,
                     std::string server,
                     std::string user,
                     std::string pas,
                     std::string db,
                     std::string port,
                     std::string externo)
{
    const CTDriver d = DriverFromName(driverName);
    if (d == CTDriver::Unknown)
        return 0;

    return Init(d, std::move(alias), std::move(server), std::move(user),
                std::move(pas), std::move(db), std::move(port),
                std::move(externo));
}

//===========================================================================
// Constructores de cadena de conexión por driver
//===========================================================================
std::string CTDataBase::BuildConnectionStringSqlServer() const
{
    std::string s;
    s  = "DRIVER=";

    const std::string driverReal =
        (!DriverLiteral.empty()) ? DriverLiteral : DriverName(Driver);
    s += driverReal;

    s += ";SERVER=";
    s += Server;

    if (!PORT.empty())
    {
        s += ",";
        s += PORT;
    }

    s += ";UID=";
    s += UID;
    s += ";PWD=";
    s += PWD;

    if (!DBQ.empty())
    {
        s += ";DATABASE=";
        s += DBQ;
    }

    // TrustServerCertificate para el driver 18 (por enum o por literal).
    const bool esDriver18 = (Driver == CTDriver::SqlServer18) ||
                            (driverReal.find("ODBC Driver 18") != std::string::npos);
    if (esDriver18)
        s += ";TrustServerCertificate=Yes";

    return s;
}

std::string CTDataBase::BuildConnectionStringOther() const
{
    std::string s;
    s  = "DRIVER=";
       // Si hay nombre literal, usarlo. Si no, resolver por enum.
    s += (!DriverLiteral.empty()) ? DriverLiteral : DriverName(Driver);


    s += ";UID=";
    s += UID;
    s += ";PWD=";
    s += PWD;

    if (!DBQ.empty())
    {
        s += ";DBQ=";
        s += DBQ;
    }




    return s;
}

std::string CTDataBase::BuildConnectionStringFromParams() const
{
    // Params ya viene como cadena completa. Convertir CTString a std::string.
    const std::wstring wp = Params.c_str();
    std::string s(wp.begin(), wp.end());
    return s;
}

//===========================================================================
// OpenConect
//===========================================================================
int CTDataBase::OpenConect()
{
    if (!hdbc) return SQL_ERROR;

    SQLSetConnectOption(hdbc, SQL_LOGIN_TIMEOUT, 5);
    SQLSetConnectOption(hdbc, SQL_CURSOR_TYPE, SQL_CURSOR_STATIC);

    std::string connectString;

    if (isNull(Params))
    {
        switch (Driver)
        {
            case CTDriver::SqlServer18:
            case CTDriver::SqlServer17:
            case CTDriver::SqlServerLegacy:
                connectString = BuildConnectionStringSqlServer();
                break;

            default:
                connectString = BuildConnectionStringOther();
                break;
        }

        connectString += ";";

        if (!APP.empty())
        {
            connectString += "APP=";
            connectString += APP;
            connectString += ";";
        }

        connectString += AutoTrans ? "Autotranslate=YES;" : "Autotranslate=NO;";
    }
    else
    {
        connectString  = BuildConnectionStringFromParams();
        connectString += AutoTrans ? "Autotranslate=YES;" : "Autotranslate=NO;";
    }

    std::vector<char> outString(2048, 0);
    SQLSMALLINT outLen = 0;

    ERR = SQLDriverConnectA(
              hdbc,
              nullptr,
              reinterpret_cast<SQLCHAR*>(const_cast<char*>(connectString.c_str())),
              static_cast<SQLSMALLINT>(connectString.size()),
              reinterpret_cast<SQLCHAR*>(outString.data()),
              static_cast<SQLSMALLINT>(outString.size()),
              &outLen,
              SQL_DRIVER_NOPROMPT);

    // Guardar la cadena de conexión efectiva para que el pool (u otros
    // consumidores) la puedan reutilizar.
    if (ERR == SQL_SUCCESS || ERR == SQL_SUCCESS_WITH_INFO)
        effectiveConnectionString_ = connectString;

    return ERR;
}

//===========================================================================
// OpenInternal
//===========================================================================
int CTDataBase::OpenInternal()
{
    isOpen = false;
    state_ = CTDbState::Disconnected;

    ERR = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (ERR != SQL_SUCCESS && ERR != SQL_SUCCESS_WITH_INFO)
        return 0;

    ERR = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION,
                        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    if (ERR != SQL_SUCCESS && ERR != SQL_SUCCESS_WITH_INFO)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        henv = nullptr;
        return 0;
    }

    ERR = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
    if (ERR != SQL_SUCCESS && ERR != SQL_SUCCESS_WITH_INFO)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        henv = nullptr;
        hdbc = nullptr;
        return 0;
    }

    ERR = OpenConect();
    if (ERR == SQL_SUCCESS || ERR == SQL_SUCCESS_WITH_INFO)
    {
        isOpen = true;
        state_ = CTDbState::Connected;
        return 1;
    }

    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    hdbc = nullptr;
    SQLFreeHandle(SQL_HANDLE_ENV, henv);
    henv = nullptr;

    isOpen = false;
    state_ = CTDbState::Error;
    return 0;
}

//===========================================================================
// Open / Close
//===========================================================================
int CTDataBase::Open()
{
    return OpenInternal();
}

int CTDataBase::Open(CTString param)
{
    Params = param;
    return OpenInternal();
}

void CTDataBase::Close()
{
    if (isOpen && hdbc)
    {
        SQLDisconnect(hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        hdbc = nullptr;
    }
    if (henv)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        henv = nullptr;
    }
    isOpen = false;
    state_ = CTDbState::Closed;
}

//===========================================================================
// Reconnect
//===========================================================================
bool CTDataBase::Reconnect()
{
    if (isOpen || hdbc != nullptr)
        Close();

    if (!isNull(Params))
        return Open(Params) == 1;

    return Open() == 1;
}

//===========================================================================
// Cursor
//===========================================================================
int CTDataBase::Cursor(const char* sql, SQLHSTMT* hstmt)
{
    if (!hdbc || !hstmt || !sql) return 0;

    RETCODE retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, hstmt);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
        return 0;

    retcode = SQLExecDirectA(*hstmt,
                             reinterpret_cast<SQLCHAR*>(
                                 const_cast<char*>(sql)),
                             SQL_NTS);

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
        return 0;

    SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
    return 1;
}

int CTDataBase::Cursor(const std::string& sql, SQLHSTMT* hstmt)
{
    return Cursor(sql.c_str(), hstmt);
}

void CTDataBase::CloseCursor(SQLHSTMT* hstmt)
{
    if (hstmt && *hstmt)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, *hstmt);
        *hstmt = nullptr;
    }
}

//===========================================================================
// Diagnóstico
//===========================================================================
std::string CTDataBase::LastErrorMessage() const
{
    if (hdbc) return odbc_diag(SQL_HANDLE_DBC, hdbc);
    if (henv) return odbc_diag(SQL_HANDLE_ENV, henv);
    return "";
}

bool CTDataBase::IsConnectionAlive()
{
    SQLHSTMT hstmt = nullptr;
    if (Cursor("SELECT 1", &hstmt) == 1)
    {
        CloseCursor(&hstmt);
        return true;
    }
    return false;
}

//===========================================================================
// Utilidades
//===========================================================================
CTDataBase* CTDataBase::FindDB(std::string name)
{
    const std::string wanted = Upper(name);
    for (int a = 0; a < Count; ++a)
    {
        CTDataBase* BB = static_cast<CTDataBase*>(Item(a));
        if (BB && Upper(BB->ALIAS) == wanted)
            return BB;
    }
    return nullptr;
}

void CTDataBase::SetParameters(std::string mandt, std::string path, std::string ext)
{
    Mandt     = std::move(mandt);
    PathCache = std::move(path);
    Extern    = std::move(ext);
}

//===========================================================================
// Debug (callback)
//===========================================================================
void CTDataBase::SetDebugCallback(std::function<void(const std::string&, bool)> cb)
{
    debugCallback_ = std::move(cb);
}

void CTDataBase::SendMsgDebuger(CTSqlTable* tb)
{
    (void)tb;
    if (!debugCallback_ || !tb) return;

    // TODO: cuando CTSqlTable exponga el SQL como std::string, descomentar:
    //   std::string sql = tb->SqlAsString();
    //   debugCallback_(sql, false);
}

//===========================================================================
// Estado de uso (para el pool)
//===========================================================================
void CTDataBase::UpdateLastUsedTime()
{
    lastUsedTime_ = std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point CTDataBase::GetLastUsedTime() const
{
    return lastUsedTime_;
}

bool CTDataBase::IsInUse() const { return state_ == CTDbState::InUse; }

void CTDataBase::SetInUse(bool value)
{
    if (value)
    {
        state_ = CTDbState::InUse;
    }
    else
    {
        // Sale de "InUse" y vuelve a Connected o Disconnected.
        state_ = isOpen ? CTDbState::Connected : CTDbState::Disconnected;
    }
}

bool CTDataBase::IsClosed() const
{
    return state_ == CTDbState::Closed;
}

void CTDataBase::MarkAsClosed()
{
    state_ = CTDbState::Closed;
}


