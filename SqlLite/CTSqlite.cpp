//---------------------------------------------------------------------------
// CTSqlite.cpp — implementación
//---------------------------------------------------------------------------
#include "CTSqlite.h"

#include <sqlite3.h>

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <string>

//===========================================================================
// Constructor / destructor
//===========================================================================
CTSqlite::CTSqlite() = default;

CTSqlite::~CTSqlite()
{
    Close();
}

//===========================================================================
// Open / Close
//===========================================================================
bool CTSqlite::Open(const std::string& path)
{
    Close();

    int rc = sqlite3_open_v2(
        path.c_str(),
        &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (rc != SQLITE_OK) {
        lastError_ = db_ ? sqlite3_errmsg(db_) : "sqlite3_open_v2 failed";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Timeout de bloqueo: 5 segundos.
    sqlite3_busy_timeout(db_, 5000);

    return true;
}

void CTSqlite::Close()
{
    Finalize();
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

//===========================================================================
// Exec — para sentencias sin resultado
//===========================================================================
bool CTSqlite::Exec(const std::string& sql)
{
    if (!db_) {
        lastError_ = "BD no abierta";
        return false;
    }

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "sqlite3_exec failed";
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    return true;
}

//===========================================================================
// Query — SELECT completo, carga todas las filas en memoria
//===========================================================================
bool CTSqlite::Query(const std::string& sql)
{
    rows_.clear();
    nulls_.clear();
    colNames_.clear();
    colIndex_.clear();
    fSelected_ = -1;

    if (!db_) {
        lastError_ = "BD no abierta";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }

    const int nCols = sqlite3_column_count(stmt);

    // Nombres de columnas
    for (int i = 0; i < nCols; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        std::string n = name ? name : "";
        colNames_.push_back(n);
        colIndex_[n] = i;
    }

    // Filas
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<std::string> row;
        std::vector<bool>        nulls;
        row.reserve(nCols);
        nulls.reserve(nCols);

        for (int i = 0; i < nCols; ++i) {
            if (sqlite3_column_type(stmt, i) == SQLITE_NULL) {
                row.push_back("");
                nulls.push_back(true);
            } else {
                const unsigned char* txt = sqlite3_column_text(stmt, i);
                const int len = sqlite3_column_bytes(stmt, i);
                row.push_back(txt ? std::string(
                    reinterpret_cast<const char*>(txt), len) : "");
                nulls.push_back(false);
            }
        }
        rows_.push_back(std::move(row));
        nulls_.push_back(std::move(nulls));
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

//===========================================================================
// Cursor de resultados
//===========================================================================
int CTSqlite::RowCount() const
{
    return static_cast<int>(rows_.size());
}

int CTSqlite::ColCount() const
{
    return static_cast<int>(colNames_.size());
}

bool CTSqlite::Goto(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return false;
    }
    fSelected_ = row;
    return true;
}

//---------------------------------------------------------------------------
// Helpers de conversión
//---------------------------------------------------------------------------
namespace {

std::string rowValue(const std::vector<std::string>& row, int idx)
{
    if (idx < 0 || idx >= static_cast<int>(row.size())) return "";
    return row[idx];
}

bool rowIsNull(const std::vector<bool>& nulls, int idx)
{
    if (idx < 0 || idx >= static_cast<int>(nulls.size())) return true;
    return nulls[idx];
}

} // namespace

//--- Acceso por nombre -----------------------------------------------------
std::string CTSqlite::Text(const std::string& col) const
{
    if (fSelected_ < 0) return "";
    auto it = colIndex_.find(col);
    if (it == colIndex_.end()) return "";
    return rowValue(rows_[fSelected_], it->second);
}

int CTSqlite::Int(const std::string& col) const
{
    return static_cast<int>(Int64(col));
}

std::int64_t CTSqlite::Int64(const std::string& col) const
{
    const std::string s = Text(col);
    if (s.empty()) return 0;
    try { return std::stoll(s); } catch (...) { return 0; }
}

double CTSqlite::Double(const std::string& col) const
{
    const std::string s = Text(col);
    if (s.empty()) return 0.0;
    try { return std::stod(s); } catch (...) { return 0.0; }
}

std::vector<uint8_t> CTSqlite::Blob(const std::string& col) const
{
    // De momento devolvemos los bytes del string tal cual.
    // Si necesitas blobs puros, amplía con sqlite3_column_blob en Query().
    const std::string s = Text(col);
    return std::vector<uint8_t>(s.begin(), s.end());
}

bool CTSqlite::IsNull(const std::string& col) const
{
    if (fSelected_ < 0) return true;
    auto it = colIndex_.find(col);
    if (it == colIndex_.end()) return true;
    return rowIsNull(nulls_[fSelected_], it->second);
}

//--- Acceso por índice -----------------------------------------------------
std::string CTSqlite::Text(int col) const
{
    if (fSelected_ < 0) return "";
    return rowValue(rows_[fSelected_], col);
}

int CTSqlite::Int(int col) const
{
    return static_cast<int>(Int64(col));
}

std::int64_t CTSqlite::Int64(int col) const
{
    const std::string s = Text(col);
    if (s.empty()) return 0;
    try { return std::stoll(s); } catch (...) { return 0; }
}

double CTSqlite::Double(int col) const
{
    const std::string s = Text(col);
    if (s.empty()) return 0.0;
    try { return std::stod(s); } catch (...) { return 0.0; }
}

std::vector<uint8_t> CTSqlite::Blob(int col) const
{
    const std::string s = Text(col);
    return std::vector<uint8_t>(s.begin(), s.end());
}

bool CTSqlite::IsNull(int col) const
{
    if (fSelected_ < 0) return true;
    return rowIsNull(nulls_[fSelected_], col);
}

//===========================================================================
// Statements preparados
//===========================================================================
bool CTSqlite::Prepare(const std::string& sql)
{
    Finalize();

    if (!db_) {
        lastError_ = "BD no abierta";
        return false;
    }

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        stmt_ = nullptr;
        return false;
    }
    return true;
}

bool CTSqlite::Step()
{
    if (!stmt_) {
        lastError_ = "No hay statement preparado";
        return false;
    }

    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)  return true;   // hay fila
    if (rc == SQLITE_DONE) return false;  // fin (o ejecución OK)

    lastError_ = sqlite3_errmsg(db_);
    return false;
}

void CTSqlite::Finalize()
{
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}


void CTSqlite::Reset()
{
    if (stmt_) sqlite3_reset(stmt_);
}

void CTSqlite::ClearBindings()
{
    if (stmt_) sqlite3_clear_bindings(stmt_);
}

//--- Binds -----------------------------------------------------------------
void CTSqlite::BindText(int index, const std::string& value)
{
    if (!stmt_) return;
    sqlite3_bind_text(stmt_, index, value.c_str(),
                      static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void CTSqlite::BindInt(int index, int value)
{
    if (!stmt_) return;
    sqlite3_bind_int(stmt_, index, value);
}

void CTSqlite::BindInt64(int index, std::int64_t value)
{
    if (!stmt_) return;
    sqlite3_bind_int64(stmt_, index, value);
}

void CTSqlite::BindDouble(int index, double value)
{
    if (!stmt_) return;
    sqlite3_bind_double(stmt_, index, value);
}

void CTSqlite::BindBlob(int index, const void* data, std::size_t size)
{
    if (!stmt_) return;
    sqlite3_bind_blob(stmt_, index, data, static_cast<int>(size),
                      SQLITE_TRANSIENT);
}

void CTSqlite::BindNull(int index)
{
    if (!stmt_) return;
    sqlite3_bind_null(stmt_, index);
}

//--- Lectura de la fila actual --------------------------------------------
int CTSqlite::ColumnCount() const
{
    if (!stmt_) return 0;
    return sqlite3_column_count(stmt_);
}

std::string CTSqlite::ColumnText(int col) const
{
    if (!stmt_) return "";
    if (sqlite3_column_type(stmt_, col) == SQLITE_NULL) return "";
    const unsigned char* txt = sqlite3_column_text(stmt_, col);
    const int len = sqlite3_column_bytes(stmt_, col);
    return txt ? std::string(reinterpret_cast<const char*>(txt), len) : "";
}

int CTSqlite::ColumnInt(int col) const
{
    if (!stmt_) return 0;
    return sqlite3_column_int(stmt_, col);
}

std::int64_t CTSqlite::ColumnInt64(int col) const
{
    if (!stmt_) return 0;
    return sqlite3_column_int64(stmt_, col);
}

double CTSqlite::ColumnDouble(int col) const
{
    if (!stmt_) return 0.0;
    return sqlite3_column_double(stmt_, col);
}

std::vector<uint8_t> CTSqlite::ColumnBlob(int col) const
{
    std::vector<uint8_t> result;
    if (!stmt_) return result;

    const void* data = sqlite3_column_blob(stmt_, col);
    const int len = sqlite3_column_bytes(stmt_, col);
    if (data && len > 0) {
        result.assign(static_cast<const uint8_t*>(data),
                      static_cast<const uint8_t*>(data) + len);
    }
    return result;
}

bool CTSqlite::ColumnIsNull(int col) const
{
    if (!stmt_) return true;
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

//===========================================================================
// Transacciones
//===========================================================================
bool CTSqlite::Begin()    { return Exec("BEGIN TRANSACTION"); }
bool CTSqlite::Commit()   { return Exec("COMMIT"); }
bool CTSqlite::Rollback() { return Exec("ROLLBACK"); }

//===========================================================================
// Utilidades
//===========================================================================
std::int64_t CTSqlite::LastInsertId() const
{
    if (!db_) return 0;
    return sqlite3_last_insert_rowid(db_);
}

int CTSqlite::Changes() const
{
    if (!db_) return 0;
    return sqlite3_changes(db_);
}


std::string CTSqlite::Pragma(const std::string& name)
{
    if (!Query("PRAGMA " + name)) {
        return "";
    }
    if (RowCount() == 0 || ColCount() == 0) {
        return "";
    }
    Goto(0);
    return Text(0);
}
//===========================================================================
// Error
//===========================================================================
void CTSqlite::SetError(const std::string& msg)
{
    lastError_ = msg;
}


