//---------------------------------------------------------------------------
// TSqlTable.cpp — implementación portable (ODBC 3.x, sin Win32)
//---------------------------------------------------------------------------
#include "TSqlTable.h"
#include "General.h"
#include "FCTLogger.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef CTLIB_WITH_LOGGER
    #define CTLOG(msg, type) ::Logger(msg, type)
#else
    #define CTLOG(msg, type)                 \
        do {                                 \
            (void)(type);                    \
            std::cerr << (msg) << std::endl; \
        } while (0)
#endif

//===========================================================================
// Helpers internos de conversión UTF-8 <-> UTF-16
//===========================================================================
namespace
{

    std::string wstring_to_utf8(const std::wstring &w)
    {
        std::string out;
        out.reserve(w.size() * 2);

        for (std::size_t i = 0; i < w.size(); ++i) {
            char32_t cp = 0;

            if constexpr (sizeof(wchar_t) == 2) {
                cp = static_cast<char32_t>(static_cast<std::uint16_t>(w[i]));
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < w.size()) {
                    char32_t lo = static_cast<char32_t>(
                        static_cast<std::uint16_t>(w[i + 1]));
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        ++i;
                    }
                }
            } else {
                cp = static_cast<char32_t>(w[i]);
            }

            if (cp < 0x80)
                out.push_back(static_cast<char>(cp));
            else if (cp < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        return out;
    }

    std::wstring utf8_to_wstring(const std::string &s)
    {
        std::wstring out;
        out.reserve(s.size());

        const unsigned char* p =
            reinterpret_cast<const unsigned char*>(s.data());
        const unsigned char* end = p + s.size();

        while (p < end) {
            char32_t cp = 0;
            int extra = 0;

            if (*p < 0x80)
                cp = *p++;
            else if ((*p & 0xE0) == 0xC0) {
                cp = *p++ & 0x1F;
                extra = 1;
            } else if ((*p & 0xF0) == 0xE0) {
                cp = *p++ & 0x0F;
                extra = 2;
            } else if ((*p & 0xF8) == 0xF0) {
                cp = *p++ & 0x07;
                extra = 3;
            } else {
                ++p;
                continue;
            }

            bool ok = true;
            for (int i = 0; i < extra; ++i) {
                if (p >= end || (*p & 0xC0) != 0x80) {
                    ok = false;
                    break;
                }
                cp = (cp << 6) | (*p++ & 0x3F);
            }
            if (!ok)
                continue;

            if constexpr (sizeof(wchar_t) == 2) {
                if (cp <= 0xFFFF)
                    out.push_back(static_cast<wchar_t>(cp));
                else {
                    cp -= 0x10000;
                    out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
                    out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
                }
            } else {
                out.push_back(static_cast<wchar_t>(cp));
            }
        }
        return out;
    }

} // namespace

//===========================================================================
// Ciclo de vida
//===========================================================================
CTSqlTable::CTSqlTable(CTDataBase* db, bool iswchar_t) : CTSTD()
{
    DB = db;
    isWchar_t = iswchar_t;
    ROW_SIZE = 1;
    m_bSupressErrors = true;
    fMaximBlob = static_cast<int>(MAX_BLOB);
    lazyThreshold_ = TSQLTABLE_DEFAULT_LAZY_THRESHOLD;
    RowCountPtr = 0;
    fRowCount = 0;
    fSelected = -1;
    retcode = SQL_SUCCESS;
    hihaError = false;
    ColumnCountPtr = 0;
    hstmt = nullptr;
    isOpen = false;
    std::memset(SqlState, 0, sizeof(SqlState));
}

CTSqlTable::~CTSqlTable()
{
    Close(true);
}

int CTSqlTable::Open(int /*tipus*/)
{
    ColumnCountPtr = 0;
    Close();

    if (DB == nullptr)
        return 0;

    retcode = SQLAllocHandle(SQL_HANDLE_STMT, DB->hdbc, &hstmt);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        return 1;

    DisplayError();
    return 0;
}

void CTSqlTable::Close(bool /*borra*/)
{
    if (hstmt != nullptr) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        hstmt = nullptr;
    }

    rowProceed.clear();
    RowStat.clear();
    Limpiar();
    isOpen = false;
}

void CTSqlTable::Limpiar()
{
    Fields.clear();
    Index.clear();
    fRowCount = 0;
    ColumnCountPtr = 0;
}

int CTSqlTable::ColCount()
{
    if (!hstmt)
        return 0;
    SQLSMALLINT n = 0;
    SQLNumResultCols(hstmt, &n);
    ColumnCountPtr = n;
    return ColumnCountPtr;
}

int CTSqlTable::RowCount()
{
    return fRowCount;
}
void CTSqlTable::InitTabla()
{
    fSelected = -1;
    fRowCount = 1;
}
void CTSqlTable::SetMaxRow(int num)
{
    ROW_SIZE = num;
}
int CTSqlTable::GetMaxRow(int /*num*/)
{
    return ROW_SIZE;
}

//===========================================================================
// CreateFields
//===========================================================================
void CTSqlTable::CreateFields(bool binds)
{
    Limpiar();
    if (!hstmt)
        return;

    SQLSMALLINT nCols = 0;
    SQLNumResultCols(hstmt, &nCols);
    if (nCols <= 0)
        return;

    ColumnCountPtr = nCols;
    Fields.resize(static_cast<std::size_t>(ColumnCountPtr));
    Index.resize(static_cast<std::size_t>(ColumnCountPtr));
    RowStat.assign(static_cast<std::size_t>(ROW_SIZE), 0);

    for (int i = 0; i < ColumnCountPtr; ++i) {
        ColSql &f = Fields[static_cast<std::size_t>(i)];

        SQLWCHAR nameBuf[256] = { 0 };
        SQLSMALLINT nameLen = 0;
        SQLSMALLINT dataType = 0;
        SQLULEN colSize = 0;
        SQLSMALLINT decDigits = 0;
        SQLSMALLINT nullable = 0;

        const RETCODE rc =
            SQLDescribeColW(hstmt, static_cast<SQLUSMALLINT>(i + 1), nameBuf,
                static_cast<SQLSMALLINT>(std::size(nameBuf)), &nameLen,
                &dataType, &colSize, &decDigits, &nullable);

        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
            continue;

        f.NombreCampo =
            std::wstring(nameBuf, static_cast<std::size_t>(nameLen));
        f.NameLengthPtr = nameLen;
        f.DataType = dataType;
        f.ColumnSizePtr = static_cast<std::size_t>(colSize);
        f.DecimalDigitsPtr = decDigits;
        f.NullablePtr = nullable;
        f.ColumnIndex = static_cast<short int>(i + 1);

        short int sqlCType = 0;
        std::size_t size = f.ColumnSizePtr;
        VerificaTipus(dataType, &sqlCType, &size);
        f.Type = sqlCType;
        f.ColumnSizePtr = size;

        const bool tipoGrande =
            (dataType == SQL_LONGVARCHAR || dataType == SQL_WLONGVARCHAR ||
                dataType == SQL_LONGVARBINARY);

        const bool tamanoGrande =
            f.ColumnSizePtr > lazyThreshold_ &&
            (dataType == SQL_CHAR || dataType == SQL_VARCHAR ||
                dataType == SQL_WCHAR || dataType == SQL_WVARCHAR ||
                dataType == SQL_BINARY || dataType == SQL_VARBINARY);

        if (ROW_SIZE == 1 && (tipoGrande || tamanoGrande)) {
            f.Lazy = true;
            f.ColumnSizePtr = 0;
            f.Buffer.clear();
            f.Longitud.assign(1, 0);
        } else {
            if (ROW_SIZE > 1 && f.ColumnSizePtr > lazyThreshold_)
                f.ColumnSizePtr = lazyThreshold_;

            const std::size_t bufBytes =
                f.ColumnSizePtr * static_cast<std::size_t>(ROW_SIZE);

            f.Buffer.assign(bufBytes, 0);
            f.Longitud.assign(static_cast<std::size_t>(ROW_SIZE), 0);

            if (binds) {
                SQLBindCol(hstmt, static_cast<SQLUSMALLINT>(i + 1), f.Type,
                    f.Buffer.data(), static_cast<SQLLEN>(f.ColumnSizePtr),
                    f.Longitud.data());
            }
        }

        Index[static_cast<std::size_t>(i)].Campo = f.NombreCampo;
        Index[static_cast<std::size_t>(i)].Fl = &f;
    }

    if (!Index.empty())
        Sort(Index.data(), 0, static_cast<int>(Index.size()) - 1);
}

//===========================================================================
// Traducción de tipos SQL → tipos C
//===========================================================================
void CTSqlTable::VerificaTipus(
    int tipus, short int* SqlTipus, std::size_t* size)
{
    switch (tipus) {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
            *SqlTipus = SQL_C_CHAR;
            if (*size > static_cast<std::size_t>(MAX_LEN_CHAR))
                *size = static_cast<std::size_t>(MAX_LEN_CHAR);
            *size += 1;
            return;

        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            *SqlTipus = SQL_C_WCHAR;
            if (*size > static_cast<std::size_t>(MAX_LEN_CHAR))
                *size = static_cast<std::size_t>(MAX_LEN_CHAR);
            *size = (*size * sizeof(wchar_t)) + sizeof(wchar_t);
            return;

        case SQL_DECIMAL:
        case SQL_NUMERIC:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
            *SqlTipus = SQL_C_DOUBLE;
            *size = sizeof(double);
            return;

        case SQL_SMALLINT:
            *SqlTipus = SQL_C_SHORT;
            *size = sizeof(short int);
            return;

        case SQL_BIGINT:
        case SQL_INTEGER:
            *SqlTipus = SQL_C_SLONG;
            *size = sizeof(std::int32_t);
            return;

        case SQL_BIT:
        case SQL_TINYINT:
            *SqlTipus = SQL_C_TINYINT;
            *size = sizeof(char);
            return;

        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
            *SqlTipus = SQL_C_BINARY;
            return;

        case SQL_TYPE_DATE:
            *SqlTipus = SQL_C_TYPE_DATE;
            *size = sizeof(DATE_STRUCT);
            return;

        case SQL_TYPE_TIME:
            *SqlTipus = SQL_C_TYPE_TIME;
            *size = sizeof(TIME_STRUCT);
            return;

        case SQL_TYPE_TIMESTAMP:
            *SqlTipus = SQL_C_TYPE_TIMESTAMP;
            *size = sizeof(TIMESTAMP_STRUCT);
            return;

        default:
            *SqlTipus = SQL_C_DEFAULT;
            return;
    }
}

//===========================================================================
// Modo lazy
//===========================================================================
bool CTSqlTable::ReadLazyField(ColSql &f, int row)
{
    if (!f.Lazy || !hstmt)
        return false;
    if (row != 0)
        return false;

    if (f.LazyRow == row && !f.LazyCache.empty())
        return true;

    f.LazyCache.clear();
    f.LazyLen = 0;

    constexpr std::size_t CHUNK = 64 * 1024;
    std::vector<char> buf(CHUNK);

    SQLLEN indicator = 0;
    RETCODE rc = SQLGetData(hstmt, static_cast<SQLUSMALLINT>(f.ColumnIndex),
        f.Type, buf.data(), static_cast<SQLLEN>(CHUNK), &indicator);

    while (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        if (indicator == SQL_NULL_DATA) {
            f.LazyLen = 0;
            break;
        }

        std::size_t n = CHUNK;
        if (indicator != SQL_NO_TOTAL && indicator >= 0 &&
            static_cast<std::size_t>(indicator) < CHUNK)
        {
            n = static_cast<std::size_t>(indicator);
        }

        if (f.Type == SQL_C_CHAR && n > 0 && buf[n - 1] == '\0')
            --n;
        else if (f.Type == SQL_C_WCHAR && n >= sizeof(wchar_t) &&
                 *reinterpret_cast<wchar_t*>(
                     buf.data() + n - sizeof(wchar_t)) == L'\0')
            n -= sizeof(wchar_t);

        if (n > 0)
            f.LazyCache.insert(f.LazyCache.end(), buf.begin(), buf.begin() + n);
        f.LazyLen = static_cast<SQLLEN>(f.LazyCache.size());

        if (rc == SQL_SUCCESS)
            break;

        rc = SQLGetData(hstmt, static_cast<SQLUSMALLINT>(f.ColumnIndex), f.Type,
            buf.data(), static_cast<SQLLEN>(CHUNK), &indicator);
    }

    if (f.Type == SQL_C_CHAR)
        f.LazyCache.push_back('\0');
    else if (f.Type == SQL_C_WCHAR) {
        f.LazyCache.push_back(0);
        f.LazyCache.push_back(0);
    }

    f.LazyRow = row;
    return true;
}

void CTSqlTable::InvalidarLazyCache()
{
    for (ColSql &f : Fields) {
        if (f.Lazy) {
            f.LazyRow = -1;
            f.LazyLen = 0;
            f.LazyCache.clear();
        }
    }
}

//===========================================================================
// Execute
//===========================================================================
int CTSqlTable::Execute(int maxrow)
{
    if (!hstmt) {
        Open();
        if (!hstmt)
            return 0;
    }
    if (maxrow > 0)
        ROW_SIZE = maxrow;

    SQLCancel(hstmt);
    rowProceed.assign(static_cast<std::size_t>(ROW_SIZE), 0);
    RowStat.assign(static_cast<std::size_t>(ROW_SIZE), 0);
    hihaError = false;

    SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
        reinterpret_cast<SQLPOINTER>(
            static_cast<std::intptr_t>(SQL_CONCUR_READ_ONLY)),
        0);
    SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
        reinterpret_cast<SQLPOINTER>(
            static_cast<std::intptr_t>(SQL_CURSOR_KEYSET_DRIVEN)),
        0);
    SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
        reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(ROW_SIZE)), 0);
    SQLSetStmtAttr(
        hstmt, SQL_ATTR_ROW_STATUS_PTR, RowStat.data(), SQL_IS_POINTER);
    SQLSetStmtAttr(
        hstmt, SQL_ATTR_ROW_OPERATION_PTR, rowProceed.data(), SQL_IS_POINTER);

    if (DB != nullptr)
        DB->SendMsgDebuger(this);

    retcode = SQLExecDirectA(hstmt,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(Sql.c_str())),
        static_cast<SQLINTEGER>(Sql.size()));

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO &&
        retcode != SQL_NO_DATA)
    {
        hihaError = true;
        DisplayError();
        return 0;
    }

    isOpen = true;
    if (maxrow > 0)
        Limpiar();
    CreateFields();
    return 1;
}

//===========================================================================
// ExecuteSql
//===========================================================================
int CTSqlTable::ExecuteSql()
{
    if (!hstmt) {
        Open();
        if (!hstmt)
            return 0;
    }

    SQLCancel(hstmt);

    SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
        reinterpret_cast<SQLPOINTER>(
            static_cast<std::intptr_t>(SQL_CONCUR_READ_ONLY)),
        0);
    SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
        reinterpret_cast<SQLPOINTER>(
            static_cast<std::intptr_t>(SQL_CURSOR_KEYSET_DRIVEN)),
        0);

    if (DB != nullptr)
        DB->SendMsgDebuger(this);

    retcode = SQLExecDirectA(hstmt,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(Sql.c_str())),
        static_cast<SQLINTEGER>(Sql.size()));

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO &&
        retcode != SQL_NO_DATA)
    {
        DisplayError();
        return 0;
    }

    isOpen = true;
    return 1;
}

//===========================================================================
// Call
//===========================================================================
int CTSqlTable::Call(int maxrow)
{
    if (!hstmt) {
        Open();
        if (!hstmt)
            return 0;
    }
    if (maxrow > 0)
        ROW_SIZE = maxrow;

    hihaError = false;
    SQLCancel(hstmt);

    rowProceed.assign(static_cast<std::size_t>(ROW_SIZE), 0);
    RowStat.assign(static_cast<std::size_t>(ROW_SIZE), 0);

    SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
        reinterpret_cast<SQLPOINTER>(
            static_cast<std::intptr_t>(SQL_CONCUR_READ_ONLY)),
        0);
    SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
        reinterpret_cast<SQLPOINTER>(
            static_cast<std::intptr_t>(SQL_CURSOR_KEYSET_DRIVEN)),
        0);
    SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
        reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(ROW_SIZE)), 0);
    SQLSetStmtAttr(
        hstmt, SQL_ATTR_ROW_STATUS_PTR, RowStat.data(), SQL_IS_POINTER);
    SQLSetStmtAttr(
        hstmt, SQL_ATTR_ROW_OPERATION_PTR, rowProceed.data(), SQL_IS_POINTER);

    if (DB != nullptr)
        DB->SendMsgDebuger(this);

    retcode = SQLExecDirectA(hstmt,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(Sql.c_str())),
        static_cast<SQLINTEGER>(Sql.size()));

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO &&
        retcode != SQL_NO_DATA)
    {
        hihaError = true;
        DisplayError();
        return 0;
    }

    isOpen = true;
    CallGetMatrix();
    return 1;
}

int CTSqlTable::CallGetMatrix()
{
    if (!isOpen) {
        Limpiar();
        return 0;
    }

    RETCODE lastRc = SQL_SUCCESS;
    do {
        CreateFields();
        InvalidarLazyCache();
        fRowCount = 0;
        fSelected = -1;

        lastRc = SQLExtendedFetch(hstmt, SQL_FETCH_NEXT, 1,
            reinterpret_cast<SQLULEN*>(&fRowCount), RowStat.data());

        if (lastRc == SQL_SUCCESS || lastRc == SQL_SUCCESS_WITH_INFO)
            fSelected = 0;
        else if (lastRc != SQL_NO_DATA && !m_bSupressErrors)
            DisplayError();

    } while (SQLMoreResults(hstmt) != SQL_NO_DATA);

    return lastRc;
}

//===========================================================================
// GetMatriz
//===========================================================================
int CTSqlTable::GetMatriz()
{
    if (!isOpen || !hstmt)
        return 0;

    InvalidarLazyCache();
    fRowCount = 0;
    fSelected = 0;

    const RETCODE rc = SQLExtendedFetch(hstmt, SQL_FETCH_NEXT, 1,
        reinterpret_cast<SQLULEN*>(&fRowCount), RowStat.data());

    return (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) ? 1 : 0;
}

int CTSqlTable::Fetch()
{
    if (!hstmt)
        return SQL_ERROR;
    InvalidarLazyCache();
    retcode = SQLFetch(hstmt);
    return retcode;
}

int CTSqlTable::Stop()
{
    if (!hstmt)
        return 0;
    return SQLCancel(hstmt);
}

//===========================================================================
// Update / Commit / Rollback
//===========================================================================
int CTSqlTable::Update(int _eof)
{
    for (int a = 0; a < ColumnCountPtr; ++a)
        PosaLongitud(&Fields[static_cast<std::size_t>(a)]);

    if (_eof != EOF_ERR && fSelected < fRowCount && fSelected < ROW_SIZE) {
        rowProceed[static_cast<std::size_t>(fSelected)] = SQL_ROW_PROCEED;
        SQLSetPos(hstmt, static_cast<SQLUSMALLINT>(fSelected + 1), SQL_UPDATE,
            SQL_LOCK_NO_CHANGE);
    } else {
        if (fSelected >= ROW_SIZE)
            return 1;
        if (fSelected >= fRowCount)
            fRowCount = fSelected + 1;
        rowProceed[static_cast<std::size_t>(fSelected)] = SQL_ROW_PROCEED;
        SQLSetPos(hstmt, static_cast<SQLUSMALLINT>(fSelected + 1), SQL_ADD,
            SQL_LOCK_NO_CHANGE);
    }

    DisplayError();
    Commit();
    return 0;
}

bool CTSqlTable::Commit()
{
    if (!DB || !DB->hdbc)
        return false;
    retcode = SQLEndTran(SQL_HANDLE_DBC, DB->hdbc, SQL_COMMIT);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        if (!m_bSupressErrors)
            DisplayError();
        return false;
    }
    return true;
}

bool CTSqlTable::Rollback()
{
    if (!DB || !DB->hdbc)
        return false;
    retcode = SQLEndTran(SQL_HANDLE_DBC, DB->hdbc, SQL_ROLLBACK);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        if (!m_bSupressErrors)
            DisplayError();
        return false;
    }
    return true;
}

int CTSqlTable::GetError()
{
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        return 0;
    return 1;
}

//===========================================================================
// Cursor
//===========================================================================
int CTSqlTable::Recno()
{
    return fSelected;
}

int CTSqlTable::Goto(int reg)
{
    if (reg >= ROW_SIZE)
        return 0;
    if (reg == ADD_REG) {
        fSelected = fRowCount;
        return EOF_ERR;
    }
    if (reg < 0 || reg >= fRowCount)
        return EOF_ERR;

    if (fSelected != reg) {
        fSelected = reg;
        InvalidarLazyCache();
    }
    return 0;
}

//===========================================================================
// Errores
//===========================================================================
void CTSqlTable::ActiveErrors(bool act)
{
    m_bSupressErrors = !act;
}

void CTSqlTable::DisplayError()
{
    if (!m_bSupressErrors)
        return;
    const std::string msg = GetErrorSQL();
    if (!msg.empty())
        CTLOG("CTSqlTable: " + msg, 6);
}

std::string CTSqlTable::GetErrorSQL()
{
    if (!hstmt)
        return std::string();

    std::string result;
    SQLWCHAR msg[SQL_MAX_MESSAGE_LENGTH] = { 0 };
    SQLINTEGER nativeError = 0;
    SQLSMALLINT msgLen = 0;
    SQLSMALLINT rec = 1;
    SQLWCHAR state[6] = { 0 };

    while (true) {
        const RETCODE rc =
            SQLGetDiagRecW(SQL_HANDLE_STMT, hstmt, rec, state, &nativeError,
                msg, static_cast<SQLSMALLINT>(std::size(msg)), &msgLen);
        if (rc == SQL_NO_DATA || rc == SQL_ERROR || rc == SQL_INVALID_HANDLE)
            break;

        result += "[";
        result += wstring_to_utf8(state);
        result += "] ";
        result += wstring_to_utf8(msg);
        result += "\n";
        ++rec;
    }
    return result;
}

//===========================================================================
// Ordenación
//===========================================================================
int CTSqlTable::Sort(Index_Cursor* values, int start, int end_list)
{
    if (values == nullptr)
        return 0;

    int low = start;
    int high = end_list;
    const std::wstring pivot = values[(start + end_list) / 2].Campo;

    do {
        while (low < ColumnCountPtr && values[low].Campo < pivot)
            ++low;
        while (high >= 0 && values[high].Campo > pivot)
            --high;
        if (low <= high) {
            std::swap(values[low], values[high]);
            ++low;
            --high;
        }
    } while (low <= high);

    if (start < high)
        Sort(values, start, high);
    if (low < end_list)
        Sort(values, low, end_list);
    return 0;
}

//===========================================================================
// Find
//===========================================================================
ColSql* CTSqlTable::Find(const CTString &sub1)
{
    if (Index.empty() || ColumnCountPtr <= 0)
        return nullptr;

    int low = 0;
    int high = ColumnCountPtr - 1;
    const std::wstring key = sub1.c_str();

    while (low <= high) {
        const int mid = low + (high - low) / 2;
        const std::wstring &campo = Index[static_cast<std::size_t>(mid)].Campo;
        if (key == campo)
            return Index[static_cast<std::size_t>(mid)].Fl;
        else if (key < campo)
            high = mid - 1;
        else
            low = mid + 1;
    }
    return nullptr;
}

//===========================================================================
// Conexión
//===========================================================================
bool CTSqlTable::IsConnectionAlive()
{
    if (!DB || !DB->hdbc)
        return false;
    SQLUINTEGER connDead = SQL_CD_TRUE;
    const SQLRETURN r = SQLGetConnectAttr(
        DB->hdbc, SQL_ATTR_CONNECTION_DEAD, &connDead, 0, nullptr);
    return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) &&
           connDead == SQL_CD_FALSE;
}

bool CTSqlTable::IsConnectionLost()
{
    return SqlState[0] == L'0' && SqlState[1] == L'8';
}

bool CTSqlTable::EnsureConnected()
{
    if (IsConnectionAlive())
        return true;
    Close();
    return DB && DB->Reconnect();
}

//===========================================================================
// StoreLlarg
//===========================================================================
int CTSqlTable::StoreLlarg(wchar_t* mj, int tope)
{
    int a = 0, b = 0;
    for (a = 0; a < tope && mj[a]; ++a)
        ;
    for (b = a; b > 0 && (mj[b] == L' ' || mj[b] == 0); --b)
        ;
    return b;
}

int CTSqlTable::StoreLlarg(char* mj, int tope)
{
    int a = 0, b = 0;
    for (a = 0; a < tope && mj[a]; ++a)
        ;
    for (b = a; b > 0 && (mj[b] == ' ' || mj[b] == 0); --b)
        ;
    return b;
}

//===========================================================================
// Store (genérico)
//===========================================================================
void CTSqlTable::Store(ColSql* field, void* vl, int llarg)
{
    if (!field || !vl)
        return;

    const char* src = nullptr;
    std::size_t len = 0;

    if (field->Lazy) {
        if (!ReadLazyField(*field, fSelected))
            return;
        src = field->LazyCache.data();
        len = static_cast<std::size_t>(field->LazyLen);
    } else {
        src = field->DataAt(fSelected);
        len = (field->Longitud[fSelected] > 0)
                  ? static_cast<std::size_t>(field->Longitud[fSelected])
                  : field->ColumnSizePtr;
    }

    switch (field->Type) {
        case SQL_C_WCHAR: {
            wchar_t* dst = static_cast<wchar_t*>(vl);
            const wchar_t* wsrc = reinterpret_cast<const wchar_t*>(src);
            const std::size_t tope = len / sizeof(wchar_t);

            std::size_t i = 0;
            for (; i < tope && wsrc[i] != L'\0'; ++i)
                dst[i] = wsrc[i];
            dst[i] = L'\0';
            break;
        }
        case SQL_C_CHAR: {
            char* sub2 = static_cast<char*>(vl);
            const int tope = static_cast<int>(len);
            int i = 0;
            for (; i < tope && src[i]; ++i)
                sub2[i] = src[i];
            sub2[i] = '\0';
            break;
        }
        case SQL_C_DOUBLE: {
            double* p = static_cast<double*>(vl);
            if (!field->Lazy && field->Longitud[fSelected] < 0) {
                *p = 0;
                return;
            }
            std::memcpy(p, src, sizeof(double));
            break;
        }
        case SQL_C_SHORT: {
            short int* p = static_cast<short int*>(vl);
            if (!field->Lazy && field->Longitud[fSelected] < 0) {
                *p = 0;
                return;
            }
            std::memcpy(p, src, sizeof(short int));
            break;
        }
        case SQL_C_SLONG: {
            int* p = static_cast<int*>(vl);
            if (!field->Lazy && field->Longitud[fSelected] < 0) {
                *p = 0;
                return;
            }
            std::memcpy(p, src, sizeof(int));
            break;
        }
        case SQL_C_TINYINT: {
            unsigned char* p = static_cast<unsigned char*>(vl);
            if (!field->Lazy && field->Longitud[fSelected] < 0) {
                *p = 0;
                return;
            }
            std::memcpy(p, src, sizeof(unsigned char));
            break;
        }
        default: {
            char* p = static_cast<char*>(vl);
            const std::size_t copia =
                (llarg < 0 || static_cast<std::size_t>(llarg) > len)
                    ? len
                    : static_cast<std::size_t>(llarg);
            std::memcpy(p, src, copia);
            break;
        }
    }
}

//===========================================================================
// Store públicos
//===========================================================================
void CTSqlTable::Store(CTString nom, void* vl, int llarg)
{
    ColSql* BB = Find(nom);
    if (BB)
        Store(BB, vl, llarg);
}

void CTSqlTable::Store(CTString nom, wchar_t* vl)
{
    ColSql* field = Find(nom);
    if (!field || !vl)
        return;

    const char* src = nullptr;
    std::size_t len = 0;

    if (field->Lazy) {
        if (!ReadLazyField(*field, fSelected))
            return;
        src = field->LazyCache.data();
        len = static_cast<std::size_t>(field->LazyLen);
    } else {
        src = field->DataAt(fSelected);
        len = field->ColumnSizePtr;
    }

    std::wstring tmp;

    if (field->Type == SQL_C_CHAR) {
        const int ll =
            StoreLlarg(const_cast<char*>(src), static_cast<int>(len));
        tmp = utf8_to_wstring(std::string(src, static_cast<std::size_t>(ll)));
    } else if (field->Type == SQL_C_WCHAR) {
        const wchar_t* wsrc = reinterpret_cast<const wchar_t*>(src);
        const int tope = static_cast<int>(len / sizeof(wchar_t));
        const int ll = StoreLlarg(const_cast<wchar_t*>(wsrc), tope);
        tmp.assign(wsrc, static_cast<std::size_t>(ll));
    }

    std::memcpy(vl, tmp.c_str(), (tmp.size() + 1) * sizeof(wchar_t));
}

void CTSqlTable::Store(CTString nom, CTString &vl)
{
    ColSql* field = Find(nom);
    if (!field)
        return;

    const char* src = nullptr;
    std::size_t len = 0;

    if (field->Lazy) {
        if (!ReadLazyField(*field, fSelected))
            return;
        src = field->LazyCache.data();
        len = static_cast<std::size_t>(field->LazyLen);
    } else {
        src = field->DataAt(fSelected);
        len = field->ColumnSizePtr;
    }

    if (field->Type == SQL_C_CHAR) {
        const int ll =
            StoreLlarg(const_cast<char*>(src), static_cast<int>(len));
        const std::wstring tmp =
            utf8_to_wstring(std::string(src, static_cast<std::size_t>(ll)));
        vl.Copy(
            const_cast<wchar_t*>(tmp.c_str()), static_cast<int>(tmp.size()));
    } else if (field->Type == SQL_C_WCHAR) {
        const wchar_t* wsrc = reinterpret_cast<const wchar_t*>(src);
        const int tope = static_cast<int>(len / sizeof(wchar_t));
        const int ll = StoreLlarg(const_cast<wchar_t*>(wsrc), tope);
        vl.Copy(const_cast<wchar_t*>(wsrc), ll);
    }
}

void CTSqlTable::Store(CTString nom, std::string &vl)
{
    ColSql* field = Find(nom);
    if (!field)
        return;

    const char* src = nullptr;
    std::size_t len = 0;

    if (field->Lazy) {
        if (!ReadLazyField(*field, fSelected))
            return;
        src = field->LazyCache.data();
        len = static_cast<std::size_t>(field->LazyLen);
    } else {
        src = field->DataAt(fSelected);
        len = field->ColumnSizePtr;
    }

    if (field->Type == SQL_C_CHAR) {
        const int ll =
            StoreLlarg(const_cast<char*>(src), static_cast<int>(len));
        vl.assign(src, static_cast<std::size_t>(ll));
    } else if (field->Type == SQL_C_WCHAR) {
        const wchar_t* wsrc = reinterpret_cast<const wchar_t*>(src);
        const int tope = static_cast<int>(len / sizeof(wchar_t));
        const int ll = StoreLlarg(const_cast<wchar_t*>(wsrc), tope);
        vl = wstring_to_utf8(std::wstring(wsrc, static_cast<std::size_t>(ll)));
    }
}

//===========================================================================
// Say / Text / Int / Double
//===========================================================================
CTString CTSqlTable::Say(int p, int x)
{
    if (x < 0 || x >= ColumnCountPtr)
        return "";

    if (fSelected != p)
        Goto(p);

    ColSql &fl = Fields[static_cast<std::size_t>(x)];
    const char* src = nullptr;
    std::size_t len = 0;

    if (fl.Lazy) {
        if (!ReadLazyField(fl, fSelected))
            return "";
        src = fl.LazyCache.data();
        len = static_cast<std::size_t>(fl.LazyLen);
    } else {
        src = fl.DataAt(fSelected);
        len = fl.ColumnSizePtr;
    }

    if (fl.Type == SQL_C_CHAR) {
        const std::string tmp(src, len);
        const std::wstring w = utf8_to_wstring(tmp);
        return CTString(w);
    }
    if (fl.Type == SQL_C_WCHAR) {
        const wchar_t* w = reinterpret_cast<const wchar_t*>(src);
        return CTString(std::wstring(w, len / sizeof(wchar_t)));
    }
    return "";
}

CTString CTSqlTable::Text(ColSql* fl)
{
    if (!fl)
        return "";

    const char* src = nullptr;
    std::size_t len = 0;

    if (fl->Lazy) {
        if (!ReadLazyField(*fl, fSelected))
            return "";
        src = fl->LazyCache.data();
        len = static_cast<std::size_t>(fl->LazyLen);
    } else {
        src = fl->DataAt(fSelected);
        len = fl->ColumnSizePtr;
    }

    if (fl->Type == SQL_C_CHAR) {
        const std::string tmp(src, len);
        return CTString(utf8_to_wstring(tmp));
    }
    if (fl->Type == SQL_C_WCHAR) {
        const wchar_t* w = reinterpret_cast<const wchar_t*>(src);
        return CTString(std::wstring(w, len / sizeof(wchar_t)));
    }
    return "";
}

CTString CTSqlTable::Text(CTString nom)
{
    ColSql* BB = Find(nom);
    if (!BB)
        return "";
    return Text(BB);
}

double CTSqlTable::Double(CTString nom)
{
    ColSql* fl = Find(nom);
    if (!fl)
        return 0;

    const char* src = fl->Lazy ? fl->LazyCache.data() : fl->DataAt(fSelected);

    switch (fl->Type) {
        case SQL_C_DOUBLE: {
            double v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        case SQL_C_SLONG: {
            std::int32_t v;
            std::memcpy(&v, src, sizeof(v));
            return static_cast<double>(v);
        }
        case SQL_C_SHORT: {
            short v;
            std::memcpy(&v, src, sizeof(v));
            return static_cast<double>(v);
        }
        case SQL_C_TINYINT: {
            unsigned char v;
            std::memcpy(&v, src, sizeof(v));
            return static_cast<double>(v);
        }
        default:
            return 0;
    }
}

int CTSqlTable::Int(CTString nom)
{
    ColSql* fl = Find(nom);
    if (!fl)
        return 0;

    const char* src = fl->Lazy ? fl->LazyCache.data() : fl->DataAt(fSelected);

    switch (fl->Type) {
        case SQL_C_DOUBLE: {
            double v;
            std::memcpy(&v, src, sizeof(v));
            return static_cast<int>(v);
        }
        case SQL_C_SLONG: {
            std::int32_t v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        case SQL_C_SHORT: {
            short v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        case SQL_C_TINYINT: {
            unsigned char v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        default:
            return 0;
    }
}

//===========================================================================
// toInt / toDouble / ISO8601
//===========================================================================
int CTSqlTable::toInt(ColSql* fl)
{
    if (!fl)
        return 0;
    const char* src = fl->Lazy ? fl->LazyCache.data() : fl->DataAt(fSelected);
    switch (fl->Type) {
        case SQL_C_SLONG: {
            std::int32_t v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        case SQL_C_SHORT: {
            short v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        case SQL_C_TINYINT: {
            unsigned char v;
            std::memcpy(&v, src, sizeof(v));
            return v;
        }
        default:
            return 0;
    }
}

double CTSqlTable::toDouble(ColSql* fl)
{
    if (!fl)
        return 0;
    const char* src = fl->Lazy ? fl->LazyCache.data() : fl->DataAt(fSelected);
    double v = 0;
    std::memcpy(&v, src, sizeof(v));
    return v;
}

std::string CTSqlTable::ISO8601(ColSql* fl)
{
    if (!fl)
        return "";
    const char* src = fl->Lazy ? fl->LazyCache.data() : fl->DataAt(fSelected);
    TIMESTAMP_STRUCT_TRC ts;
    std::memcpy(&ts, src, sizeof(ts));
    return TimestampToISO8601(ts);
}

ColSql* CTSqlTable::GetField(int pos)
{
    if (pos < 0 || pos >= ColumnCountPtr)
        return nullptr;
    return &Fields[static_cast<std::size_t>(pos)];
}

//===========================================================================
// Get / Set
//===========================================================================
void* CTSqlTable::Get(wchar_t* name)
{
    ColSql* fl = Find(CTString(name));
    if (!fl)
        return const_cast<char*>("");
    if (fl->Lazy) {
        if (!ReadLazyField(*fl, fSelected))
            return const_cast<char*>("");
        return fl->LazyCache.data();
    }
    return fl->DataAt(fSelected);
}

void* CTSqlTable::Get(int pos)
{
    if (pos < 0 || pos >= ColumnCountPtr)
        return const_cast<char*>("");
    ColSql &fl = Fields[static_cast<std::size_t>(pos)];
    if (fl.Lazy) {
        if (!ReadLazyField(fl, fSelected))
            return const_cast<char*>("");
        return fl.LazyCache.data();
    }
    return fl.DataAt(fSelected);
}

void CTSqlTable::Set(CTString /*name*/, void* /*bf*/, int /*x*/) {}

//===========================================================================
// Metadatos
//===========================================================================
int CTSqlTable::GetColAling(int col)
{
    if (col < 0 || col >= ColCount())
        return 0;
    return Aling(col);
}

int CTSqlTable::Aling(int col)
{
    if (col < 0 || col >= ColumnCountPtr)
        return 0;
    const short int t = Fields[static_cast<std::size_t>(col)].Type;
    if (t == SQL_C_DOUBLE || t == SQL_C_SHORT || t == SQL_C_SLONG)
        return 1;
    return 0;
}

CTString CTSqlTable::GetColCaption(int col)
{
    if (col < 0 || col >= ColumnCountPtr)
        return L"";
    return CTString(Fields[static_cast<std::size_t>(col)].NombreCampo);
}

int CTSqlTable::GetColType(int col)
{
    if (col < 0 || col >= ColumnCountPtr)
        return 0;
    return Fields[static_cast<std::size_t>(col)].Type;
}

int CTSqlTable::TypeField(CTString nom)
{
    ColSql* BB = Find(nom);
    if (!BB)
        return 0;
    if (BB->Type == SQL_C_CHAR)
        return 0;
    return 1;
}

int CTSqlTable::GetColWidth(void* /*hwnd*/, int col, int max)
{
    if (col < 0 || col >= ColumnCountPtr)
        return 0;
    const ColSql &f = Fields[static_cast<std::size_t>(col)];
    int chars = static_cast<int>(f.NombreCampo.size());
    if (chars < 4)
        chars = 4;
    int width = chars * 8 + 16;
    if (width > max)
        width = max;
    return width;
}

int CTSqlTable::GetColLen(int col)
{
    if (col < 0 || col >= ColumnCountPtr)
        return 0;
    return static_cast<int>(
        Fields[static_cast<std::size_t>(col)].ColumnSizePtr);
}

//===========================================================================
// PosaLongitud
//===========================================================================
void CTSqlTable::PosaLongitud(ColSql* field)
{
    if (!field || field->Lazy)
        return;

    char* bf = field->DataAt(fSelected);

    if (field->Longitud[fSelected] > 0)
        return;

    if (field->Type == SQL_C_CHAR) {
        bf[0] = ' ';
        field->Longitud[fSelected] = 1;
        return;
    }
    if (field->Type == SQL_C_WCHAR) {
        reinterpret_cast<wchar_t*>(bf)[0] = L' ';
        reinterpret_cast<wchar_t*>(bf)[1] = L'\0';
        field->Longitud[fSelected] = static_cast<SQLLEN>(2 * sizeof(wchar_t));
        return;
    }
    if (field->Type == SQL_C_DOUBLE)
        field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(double));
    else if (field->Type == SQL_C_SHORT)
        field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(short int));
    else if (field->Type == SQL_C_SLONG)
        field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(int));
    else if (field->Type == SQL_C_TINYINT)
        field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(unsigned char));
    else
        field->Longitud[fSelected] = static_cast<SQLLEN>(field->ColumnSizePtr);

    std::memset(bf, 0, static_cast<std::size_t>(field->Longitud[fSelected]));
}

//===========================================================================
// Replace
//===========================================================================
void CTSqlTable::Replace(CTString nom, void* vl)
{
    ColSql* BB = Find(nom);
    if (BB)
        Replace(BB, vl);
}

void CTSqlTable::Replace(ColSql* field, void* vl)
{
    if (!field || !vl)
        return;

    if (field->Lazy) {
        const std::size_t n = field->ColumnSizePtr > 0
                                  ? field->ColumnSizePtr
                                  : static_cast<std::size_t>(MAX_BLOB);
        field->LazyCache.assign(
            static_cast<const char*>(vl), static_cast<const char*>(vl) + n);
        field->LazyLen = static_cast<SQLLEN>(n);
        field->LazyRow = fSelected;
        return;
    }

    char* bf = field->DataAt(fSelected);

    switch (field->Type) {
        case SQL_C_CHAR: {
            const char* src = static_cast<const char*>(vl);
            const std::size_t n = std::strlen(src);
            const std::size_t cap =
                (field->ColumnSizePtr > 0) ? field->ColumnSizePtr - 1 : 0;
            const std::size_t ll = (n < cap) ? n : cap;
            std::memcpy(bf, src, ll);
            bf[ll] = '\0';
            field->Longitud[fSelected] = static_cast<SQLLEN>(ll);
            break;
        }
        case SQL_C_DOUBLE: {
            std::memcpy(bf, vl, sizeof(double));
            field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(double));
            break;
        }
        case SQL_C_SHORT: {
            std::memcpy(bf, vl, sizeof(short int));
            field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(short int));
            break;
        }
        case SQL_C_SLONG: {
            std::memcpy(bf, vl, sizeof(int));
            field->Longitud[fSelected] = static_cast<SQLLEN>(sizeof(int));
            break;
        }
        case SQL_C_TINYINT: {
            std::memcpy(bf, vl, sizeof(unsigned char));
            field->Longitud[fSelected] =
                static_cast<SQLLEN>(sizeof(unsigned char));
            break;
        }
        default: {
            const std::size_t n = field->ColumnSizePtr;
            std::memcpy(bf, vl, n);
            field->Longitud[fSelected] = static_cast<SQLLEN>(n);
            break;
        }
    }
}

void CTSqlTable::Replace(CTString nom, wchar_t* vl)
{
    ColSql* field = Find(nom);
    if (!field || !vl)
        return;

    if (field->Lazy) {
        const std::string tmp = wstring_to_utf8(vl);
        field->LazyCache.assign(tmp.begin(), tmp.end());
        field->LazyCache.push_back('\0');
        field->LazyLen = static_cast<SQLLEN>(tmp.size());
        field->LazyRow = fSelected;
        return;
    }

    char* bf = field->DataAt(fSelected);
    const std::wstring w = vl;

    switch (field->Type) {
        case SQL_C_WCHAR: {
            const std::size_t cap =
                (field->ColumnSizePtr / sizeof(wchar_t) > 0)
                    ? field->ColumnSizePtr / sizeof(wchar_t) - 1
                    : 0;
            const std::size_t ll = (w.size() < cap) ? w.size() : cap;
            std::memcpy(bf, w.data(), ll * sizeof(wchar_t));
            reinterpret_cast<wchar_t*>(bf)[ll] = 0;
            field->Longitud[fSelected] =
                static_cast<SQLLEN>((ll + 1) * sizeof(wchar_t));
            break;
        }
        case SQL_C_CHAR: {
            const std::string tmp = wstring_to_utf8(w);
            const std::size_t cap =
                (field->ColumnSizePtr > 0) ? field->ColumnSizePtr - 1 : 0;
            const std::size_t ll = (tmp.size() < cap) ? tmp.size() : cap;
            std::memcpy(bf, tmp.data(), ll);
            bf[ll] = '\0';
            field->Longitud[fSelected] = static_cast<SQLLEN>(ll);
            break;
        }
        default: {
            const std::size_t n = field->ColumnSizePtr;
            std::memcpy(bf, w.data(),
                (n < w.size() * sizeof(wchar_t)) ? n
                                                 : w.size() * sizeof(wchar_t));
            field->Longitud[fSelected] = static_cast<SQLLEN>(n);
            break;
        }
    }
}

void CTSqlTable::Replace(CTString nom, CTString vl)
{
    Replace(nom, const_cast<wchar_t*>(vl.c_str()));
}

//===========================================================================
// BLOB
//===========================================================================
int CTSqlTable::SetMaximBlob(int maxim)
{
    fMaximBlob = maxim;
    return 0;
}

void CTSqlTable::ReadBLOB(CTString nom, void* vl, int llarg)
{
    ColSql* BB = Find(nom);
    if (!BB || !vl)
        return;

    if (BB->Lazy) {
        if (!ReadLazyField(*BB, fSelected))
            return;
        const std::size_t tope = (llarg > fMaximBlob)
                                     ? static_cast<std::size_t>(fMaximBlob)
                                     : static_cast<std::size_t>(llarg);
        const std::size_t copia =
            std::min<std::size_t>(tope, static_cast<std::size_t>(BB->LazyLen));
        std::memcpy(vl, BB->LazyCache.data(), copia);
        return;
    }

    BB->Longitud[0] = (llarg > fMaximBlob) ? fMaximBlob : llarg;
    std::memcpy(
        vl, BB->Buffer.data(), static_cast<std::size_t>(BB->Longitud[0]));
}

void CTSqlTable::WriteBLOB(CTString nom, void* vl, int llarg)
{
    ColSql* BB = Find(nom);
    if (!BB || !vl)
        return;

    if (BB->Lazy) {
        const std::size_t tope = (llarg > fMaximBlob)
                                     ? static_cast<std::size_t>(fMaximBlob)
                                     : static_cast<std::size_t>(llarg);
        BB->LazyCache.assign(
            static_cast<const char*>(vl), static_cast<const char*>(vl) + tope);
        BB->LazyLen = static_cast<SQLLEN>(tope);
        BB->LazyRow = fSelected;
        return;
    }

    BB->Longitud[0] = (llarg > fMaximBlob) ? fMaximBlob : llarg;
    std::memcpy(
        BB->Buffer.data(), vl, static_cast<std::size_t>(BB->Longitud[0]));
}

std::vector<unsigned char> CTSqlTable::GetImage(CTString nombreCampo)
{
    std::vector<unsigned char> data;
    ColSql* campo = Find(nombreCampo);
    if (!campo || campo->Type != SQL_C_BINARY)
        return data;

    if (campo->Lazy) {
        if (!ReadLazyField(*campo, fSelected))
            return data;
        data.assign(campo->LazyCache.begin(),
            campo->LazyCache.begin() + campo->LazyLen);
        return data;
    }

    if (fSelected < 0 || fSelected >= ROW_SIZE)
        return data;

    const SQLLEN len = campo->Longitud[fSelected];
    if (len <= 0)
        return data;

    const char* ptr = campo->DataAt(fSelected);
    data.assign(ptr, ptr + len);
    return data;
}

//===========================================================================
// Copy
//===========================================================================
int CTSqlTable::Copy(std::string &txt, char* bf, int ll)
{
    if (!bf || ll < 0) {
        txt.clear();
        return 0;
    }
    txt.assign(bf, bf + ll);
    return ll;
}

int CTSqlTable::Copy(std::string &txt, wchar_t* bf, int ll)
{
    if (!bf || ll < 0) {
        txt.clear();
        return 0;
    }
    txt = wstring_to_utf8(std::wstring(bf, static_cast<std::size_t>(ll)));
    return ll;
}

//===========================================================================
// Parametrización
//===========================================================================
bool CTSqlTable::Prepare(const std::wstring &sql)
{
    Close();
    if (!DB || !DB->hdbc)
        return false;

    retcode = SQLAllocHandle(SQL_HANDLE_STMT, DB->hdbc, &hstmt);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        DisplayError();
        return false;
    }

    retcode = SQLPrepareW(hstmt,
        reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(sql.c_str())),
        SQL_NTS);

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        DisplayError();
        return false;
    }
    return true;
}

bool CTSqlTable::Bind(int paramIndex, SQLSMALLINT paramType, SQLSMALLINT cType,
    SQLSMALLINT sqlType, SQLULEN colSize, SQLSMALLINT decDigits,
    SQLPOINTER value, SQLLEN bufferLen, SQLLEN* strLen_or_Ind)
{
    if (!hstmt)
        return false;

    retcode = SQLBindParameter(hstmt, static_cast<SQLUSMALLINT>(paramIndex),
        paramType, cType, sqlType, colSize, decDigits, value, bufferLen,
        strLen_or_Ind);

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        DisplayError();
        return false;
    }
    return true;
}

bool CTSqlTable::BindInt(int index, int& value)
{
    static thread_local SQLLEN len = sizeof(int);
    return Bind(index, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                sizeof(int), 0, &value, sizeof(int), &len);
}

bool CTSqlTable::BindWString(int index, const std::wstring& value)
{
    // OJO: value debe vivir hasta ExecutePrepared.
    static thread_local SQLLEN len = 0;
    len = static_cast<SQLLEN>(value.size() * sizeof(wchar_t));
    return Bind(index, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                static_cast<SQLULEN>(value.size()), 0,
                const_cast<wchar_t*>(value.c_str()),
                static_cast<SQLLEN>(value.size() * sizeof(wchar_t)), &len);
}

bool CTSqlTable::BindString(int index, const std::string& value)
{
    static thread_local SQLLEN len = 0;
    len = static_cast<SQLLEN>(value.size());
    return Bind(index, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                static_cast<SQLULEN>(value.size()), 0,
                const_cast<char*>(value.c_str()),
                static_cast<SQLLEN>(value.size()), &len);
}
bool CTSqlTable::ExecutePrepared(int maxrow)
{
    if (!hstmt)
        return false;

    if (maxrow > 0)
        ROW_SIZE = maxrow;

    hihaError = false;
    SQLCancel(hstmt);

    rowProceed.assign(static_cast<std::size_t>(ROW_SIZE), 0);
    RowStat.assign(static_cast<std::size_t>(ROW_SIZE), 0);

    SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
        reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(ROW_SIZE)), 0);
    SQLSetStmtAttr(
        hstmt, SQL_ATTR_ROW_STATUS_PTR, RowStat.data(), SQL_IS_POINTER);
    SQLSetStmtAttr(
        hstmt, SQL_ATTR_ROW_OPERATION_PTR, rowProceed.data(), SQL_IS_POINTER);

    retcode = SQLExecute(hstmt);

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        isOpen = true;
        CallGetMatrix();
        return true;
    }

    DisplayError();
    return false;
}

bool CTSqlTable::CallProcedure(
    const std::wstring &procName, const std::vector<std::wstring> &params)
{
    std::wstring sql = L"{call " + procName + L"(";
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0)
            sql += L",";
        sql += L"?";
    }
    sql += L")}";

    if (!Prepare(sql))
        return false;

    std::vector<SQLLEN> lens(params.size(), 0);

    for (std::size_t i = 0; i < params.size(); ++i) {
        lens[i] = static_cast<SQLLEN>(params[i].size());
        if (!Bind(static_cast<int>(i + 1), SQL_PARAM_INPUT, SQL_C_WCHAR,
                SQL_WVARCHAR, static_cast<SQLULEN>(params[i].size()), 0,
                reinterpret_cast<SQLPOINTER>(
                    const_cast<wchar_t*>(params[i].c_str())),
                static_cast<SQLLEN>(params[i].size() * sizeof(wchar_t)),
                &lens[i]))
        {
            return false;
        }
    }

    const bool ok = ExecutePrepared(100);
    return ok;
}

