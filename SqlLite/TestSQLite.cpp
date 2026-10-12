#include "TestSQLite.h"
#include "CTSqlite.h"
#include "Migrations.h"
#include <iostream>
#include <memory>
#include <string>
#include <sqlite3.h>


bool TestSQLite()
{
    std::cout << "\n=== Test SQLite ===\n";

    // 1. Crear BD de prueba en memoria (no toca el disco)
    CTSqlite db;
    if (!db.Open(":memory:")) {
        std::cout << "[FAIL] No se pudo abrir la BD: "
                  << db.LastError() << std::endl;
        return false;
    }
    std::cout << "[OK] BD abierta\n";

    // 2. Crear una tabla
    if (!db.Exec(
        "CREATE TABLE test ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  value INTEGER"
        ")"))
    {
        std::cout << "[FAIL] CREATE TABLE: " << db.LastError() << std::endl;
        return false;
    }
    std::cout << "[OK] Tabla creada\n";

    // 3. Insertar con binds
    if (!db.Prepare("INSERT INTO test (name, value) VALUES (?, ?)")) {
        std::cout << "[FAIL] Prepare: " << db.LastError() << std::endl;
        return false;
    }
    db.BindText(1, "hola");
    db.BindInt(2, 42);
    db.Step();
    db.Finalize();

    const auto id1 = db.LastInsertId();
    std::cout << "[OK] Insertado id=" << id1 << "\n";

    db.Prepare("INSERT INTO test (name, value) VALUES (?, ?)");
    db.BindText(1, "mundo");
    db.BindInt(2, 100);
    db.Step();
    db.Finalize();

    // 4. Consultar
    if (!db.Query("SELECT id, name, value FROM test ORDER BY id")) {
        std::cout << "[FAIL] Query: " << db.LastError() << std::endl;
        return false;
    }

    std::cout << "[OK] Filas: " << db.RowCount() << "\n";
    for (int i = 0; i < db.RowCount(); ++i) {
        db.Goto(i);
        std::cout << "    id=" << db.Int("id")
                  << " name=" << db.Text("name")
                  << " value=" << db.Int("value") << "\n";
    }

    // 5. JSON1 (si está activado)
    if (db.Query("SELECT json_extract('{\"a\":123}', '$.a') AS val")
        && db.RowCount() > 0)
    {
        db.Goto(0);
        std::cout << "[OK] JSON1 activado (json_extract='"
                  << db.Text("val") << "')\n";
    } else {
        std::cout << "[WARN] JSON1 no activado\n";
    }

    // 6. Thread-safe mode
    std::cout << "[INFO] SQLite version: " << sqlite3_libversion() << "\n";
    std::cout << "[INFO] Thread-safe: "
              << (sqlite3_threadsafe() ? "SI" : "NO") << "\n";

    std::cout << "=== Test SQLite OK ===\n\n";
    return true;
}
