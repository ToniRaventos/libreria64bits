//---------------------------------------------------------------------------
// CTSqlite.h — wrapper ligero sobre SQLite3
//
// Uso:
//     CTSqlite db;
//     if (!db.Open("data/local.db3")) { ... }
//
//     // Ejecutar (INSERT/UPDATE/DELETE/CREATE)
//     db.Exec("CREATE TABLE ...");
//     db.Exec("INSERT INTO ...");
//
//     // Consultar (SELECT)
//     if (db.Query("SELECT id, name FROM ...")) {
//         for (int i = 0; i < db.RowCount(); ++i) {
//             db.Goto(i);
//             int id      = db.Int("id");
//             std::string n = db.Text("name");
//         }
//     }
//
//     // Consultar con parámetros bindeados
//     db.Prepare("INSERT INTO t (a, b) VALUES (?, ?)");
//     db.BindText(1, "valor");
//     db.BindInt(2, 42);
//     db.Step();
//---------------------------------------------------------------------------
#ifndef CTSQLITE_H
#define CTSQLITE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Forward declaration: no exponemos sqlite3 a los que incluyan este header
struct sqlite3;
struct sqlite3_stmt;

class CTSqlite
{
public:
    CTSqlite();
    ~CTSqlite();

    // No copiable
    CTSqlite(const CTSqlite&)            = delete;
    CTSqlite& operator=(const CTSqlite&) = delete;

    //--- Ciclo de vida ------------------------------------------------
    // Abre (o crea) la base de datos. Devuelve true si OK.
    bool Open(const std::string& path);
    void Close();
    bool IsOpen() const { return db_ != nullptr; }

    //--- Ejecución directa (sin resultados) ---------------------------
    // Para CREATE / INSERT / UPDATE / DELETE / PRAGMA...
    // Devuelve true si OK.
    bool Exec(const std::string& sql);

    //--- Consulta (SELECT) --------------------------------------------
    // Ejecuta la query y carga todas las filas en memoria.
    // Devuelve true si OK.
    bool Query(const std::string& sql);

    // Cursor sobre las filas cargadas por Query()
    int  RowCount() const;
    int  ColCount() const;
    bool Goto(int row);

    // Acceso por nombre de columna (recuerda hacer Goto antes)
    std::string          Text  (const std::string& col) const;
    int                  Int   (const std::string& col) const;
    std::int64_t         Int64 (const std::string& col) const;
    double               Double(const std::string& col) const;
    std::vector<uint8_t> Blob  (const std::string& col) const;
    bool                 IsNull(const std::string& col) const;

    // Acceso por índice (0-based)
    std::string          Text  (int col) const;
    int                  Int   (int col) const;
    std::int64_t         Int64 (int col) const;
    double               Double(int col) const;
    std::vector<uint8_t> Blob  (int col) const;
    bool                 IsNull(int col) const;

    //--- Statements preparados (con binds) ----------------------------
    // Uso:
    //     db.Prepare("INSERT INTO t (a, b) VALUES (?, ?)");
    //     db.BindText(1, "hola");
    //     db.BindInt(2, 123);
    //     db.Step();
    //     db.Finalize();
    //
    // O con iteración:
    //     db.Prepare("SELECT id FROM t WHERE status = ?");
    //     db.BindText(1, "pending");
    //     while (db.Step()) { int id = db.ColumnInt(0); }
    //     db.Finalize();
    bool Prepare(const std::string& sql);
    bool Step();         // true si hay fila (o si fue INSERT/UPDATE)
    void Finalize();


    void Reset();
    void ClearBindings();


    // Binds (1-based, como en ODBC)
    void BindText  (int index, const std::string& value);
    void BindInt   (int index, int value);
    void BindInt64 (int index, std::int64_t value);
    void BindDouble(int index, double value);
    void BindBlob  (int index, const void* data, std::size_t size);
    void BindNull  (int index);

    // Lectura de la fila actual (0-based)
    int                  ColumnCount() const;
    std::string          ColumnText  (int col) const;
    int                  ColumnInt   (int col) const;
    std::int64_t         ColumnInt64 (int col) const;
    double               ColumnDouble(int col) const;
    std::vector<uint8_t> ColumnBlob  (int col) const;
    bool                 ColumnIsNull(int col) const;

    //--- Transacciones ------------------------------------------------
    bool Begin();
    bool Commit();
    bool Rollback();

    //--- Utilidades ---------------------------------------------------
    // Última clave insertada (AUTOINCREMENT / INTEGER PRIMARY KEY)
    std::int64_t LastInsertId() const;

    // Último error (mensaje)
    std::string LastError() const { return lastError_; }

    // Número de filas afectadas por el último Exec/Step (INSERT/UPDATE/DELETE)
    int Changes() const;

    // Ejecuta un PRAGMA (devuelve el resultado como string si aplica)
    std::string Pragma(const std::string& name);

private:
    sqlite3*      db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
    std::string   lastError_;

    // Resultados de Query()
    std::vector<std::string>              colNames_;   // nombre por índice
    std::map<std::string, int>            colIndex_;   // nombre → índice
    std::vector<std::vector<std::string>> rows_;       // filas × columnas (como texto)
    std::vector<std::vector<bool>>        nulls_;      // marcadores de NULL
    int                                   fSelected_ = -1;

    void SetError(const std::string& msg);
};

#endif // CTSQLITE_H
