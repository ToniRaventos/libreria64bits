//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// TestCTSqlite.cpp — test completo de la clase CTSqlite
//
// Uso: llama a TestCTSqlite() al principio del main, antes de arrancar
//      el servidor. Si devuelve true, la clase funciona correctamente.
//---------------------------------------------------------------------------
#include "TestCTSqlite.h"

#include "CTSqlite.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>


namespace {

int g_ok = 0;
int g_fail = 0;

//---------------------------------------------------------------------------
// Helpers de aserción
//---------------------------------------------------------------------------
void CHECK(bool cond, const std::string& what)
{
    if (cond) {
        ++g_ok;
        std::cout << "  [OK]   " << what << std::endl;
    } else {
        ++g_fail;
        std::cout << "  [FAIL] " << what << std::endl;
    }
}

void SECTION(const std::string& name)
{
    std::cout << "\n--- " << name << " ---" << std::endl;
}

//---------------------------------------------------------------------------
// 1. Open / Close
//---------------------------------------------------------------------------
void Test_OpenClose()
{
    SECTION("1. Open / Close / IsOpen");

    CTSqlite db;
    CHECK(!db.IsOpen(), "BD recién creada: IsOpen() == false");

    CHECK(db.Open(":memory:"), "Open(\":memory:\") devuelve true");
    CHECK(db.IsOpen(), "Tras Open: IsOpen() == true");

    db.Close();
    CHECK(!db.IsOpen(), "Tras Close: IsOpen() == false");

    // Abrir dos veces sin cerrar: no debe romper
    CHECK(db.Open(":memory:"), "Segundo Open tras Close OK");
    CHECK(db.Open(":memory:"), "Tercer Open sin Close: se cierra el anterior");
    db.Close();
}

//---------------------------------------------------------------------------
// 2. Exec (CREATE, INSERT, UPDATE, DELETE)
//---------------------------------------------------------------------------
void Test_Exec()
{
    SECTION("2. Exec (sin resultados)");

    CTSqlite db;
    db.Open(":memory:");

    CHECK(db.Exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"),
          "CREATE TABLE");

    CHECK(db.Exec("INSERT INTO t (name) VALUES ('uno')"),
          "INSERT literal");

    CHECK(db.Exec("INSERT INTO t (name) VALUES ('dos')"),
          "INSERT literal 2");

    CHECK(db.Changes() == 1, "Changes() == 1 tras INSERT");

    CHECK(db.Exec("UPDATE t SET name='UNO' WHERE id=1"),
          "UPDATE");

    CHECK(db.Changes() == 1, "Changes() == 1 tras UPDATE");

    CHECK(db.Exec("DELETE FROM t WHERE id=2"),
          "DELETE");

    CHECK(db.Changes() == 1, "Changes() == 1 tras DELETE");

    // SQL mal formado debe devolver false
    CHECK(!db.Exec("CREATE TABLA mal"), "SQL inválido devuelve false");

    CHECK(!db.LastError().empty(), "LastError() relleno tras error");

    db.Close();
}

//---------------------------------------------------------------------------
// 3. Query simple (sin binds)
//---------------------------------------------------------------------------
void Test_QuerySimple()
{
    SECTION("3. Query simple (sin binds)");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, value INTEGER)");
    db.Exec("INSERT INTO t (name, value) VALUES ('uno', 42)");
    db.Exec("INSERT INTO t (name, value) VALUES ('dos', 100)");
    db.Exec("INSERT INTO t (name, value) VALUES ('tres', 7)");

    CHECK(db.Query("SELECT id, name, value FROM t ORDER BY id"),
          "Query devuelve true");

    CHECK(db.RowCount() == 3, "RowCount() == 3");
    CHECK(db.ColCount() == 3, "ColCount() == 3");

    // Recorrer con Goto
    db.Goto(0);
    CHECK(db.Int("id") == 1, "Fila 0: id == 1");
    CHECK(db.Text("name") == "uno", "Fila 0: name == 'uno'");
    CHECK(db.Int("value") == 42, "Fila 0: value == 42");

    db.Goto(1);
    CHECK(db.Int("id") == 2, "Fila 1: id == 2");
    CHECK(db.Text("name") == "dos", "Fila 1: name == 'dos'");

    db.Goto(2);
    CHECK(db.Int("id") == 3, "Fila 2: id == 3");
    CHECK(db.Text("name") == "tres", "Fila 2: name == 'tres'");

    // Acceso por índice (0-based)
    db.Goto(1);
    CHECK(db.Text(1) == "dos", "Acceso por índice (col 1) == 'dos'");
    CHECK(db.Int(2) == 100, "Acceso por índice (col 2) == 100");

    // Goto fuera de rango
    CHECK(!db.Goto(5), "Goto(5) fuera de rango devuelve false");
    CHECK(!db.Goto(-1), "Goto(-1) devuelve false");

    // Columna inexistente
    db.Goto(0);
    CHECK(db.Text("no_existe").empty(), "Columna inexistente devuelve ''");
    CHECK(db.Int("no_existe") == 0, "Columna inexistente Int devuelve 0");

    db.Close();
}

//---------------------------------------------------------------------------
// 4. Prepare / Bind / Step (INSERT)
//---------------------------------------------------------------------------
void Test_PrepareInsert()
{
    SECTION("4. Prepare / Bind / Step (INSERT)");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  email TEXT,"
            "  age INTEGER,"
            "  score REAL,"
            "  notes BLOB,"
            "  deleted_at DATETIME"
            ")");

    // Insertar con todos los tipos
    CHECK(db.Prepare(
        "INSERT INTO users (name, email, age, score, notes, deleted_at) "
        "VALUES (?, ?, ?, ?, ?, ?)"),
        "Prepare INSERT");

    db.BindText(1, "Juan");
    db.BindText(2, "juan@example.com");
    db.BindInt(3, 30);
    db.BindDouble(4, 99.5);
    const std::string blobData = "BLOB-DATA-12345";
    db.BindBlob(5, blobData.data(), blobData.size());
    db.BindNull(6);   // deleted_at = NULL

    db.Step();
    db.Finalize();

    CHECK(db.LastInsertId() == 1, "LastInsertId() == 1");

    // Insertar un segundo
    db.Prepare("INSERT INTO users (name, age) VALUES (?, ?)");
    db.BindText(1, "Maria");
    db.BindInt(2, 25);
    db.Step();
    db.Finalize();

    CHECK(db.LastInsertId() == 2, "LastInsertId() == 2");

    // Verificar
    db.Query("SELECT name, email, age, score FROM users ORDER BY id");

    CHECK(db.RowCount() == 2, "Hay 2 usuarios");

    db.Goto(0);
    CHECK(db.Text("name") == "Juan", "Fila 0 name == 'Juan'");
    CHECK(db.Text("email") == "juan@example.com", "Fila 0 email");
    CHECK(db.Int("age") == 30, "Fila 0 age == 30");
    CHECK(db.Double("score") == 99.5, "Fila 0 score == 99.5");

    db.Goto(1);
    CHECK(db.Text("name") == "Maria", "Fila 1 name == 'Maria'");
    CHECK(db.IsNull("email"), "Fila 1 email es NULL");

    db.Close();
}

//---------------------------------------------------------------------------
// 5. Prepare / Step iterado (SELECT con binds)
//---------------------------------------------------------------------------
void Test_PrepareSelect()
{
    SECTION("5. Prepare / Step iterado (SELECT)");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, category TEXT)");
    db.Exec("INSERT INTO items (name, category) VALUES ('a', 'X')");
    db.Exec("INSERT INTO items (name, category) VALUES ('b', 'X')");
    db.Exec("INSERT INTO items (name, category) VALUES ('c', 'Y')");
    db.Exec("INSERT INTO items (name, category) VALUES ('d', 'X')");

    CHECK(db.Prepare(
        "SELECT name FROM items WHERE category = ? ORDER BY name"),
        "Prepare SELECT con bind");

    db.BindText(1, "X");

    int count = 0;
    std::string collected;
    while (db.Step()) {
        collected += db.ColumnText(0) + ",";
        ++count;
    }
    db.Finalize();

    CHECK(count == 3, "3 items en categoría 'X'");
    CHECK(collected == "a,b,d,", "Nombres correctos: a,b,d");

    // Categoría sin items
    db.Prepare("SELECT name FROM items WHERE category = ?");
    db.BindText(1, "Z");
    int zcount = 0;
    while (db.Step()) ++zcount;
    db.Finalize();
    CHECK(zcount == 0, "Categoría 'Z' no tiene items");

    db.Close();
}

//---------------------------------------------------------------------------
// 6. Transacciones (COMMIT)
//---------------------------------------------------------------------------
void Test_TransactionCommit()
{
    SECTION("6. Transacción con COMMIT");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");

    CHECK(db.Begin(), "BEGIN");

    db.Prepare("INSERT INTO t (name) VALUES (?)");
    db.BindText(1, "A");
    db.Step();
    db.Finalize();

    db.Prepare("INSERT INTO t (name) VALUES (?)");
    db.BindText(1, "B");
    db.Step();
    db.Finalize();

    CHECK(db.Commit(), "COMMIT");

    db.Query("SELECT COUNT(*) AS n FROM t");
    db.Goto(0);
    CHECK(db.Int("n") == 2, "Dos filas tras COMMIT");

    db.Close();
}

//---------------------------------------------------------------------------
// 7. Transacciones (ROLLBACK)
//---------------------------------------------------------------------------
void Test_TransactionRollback()
{
    SECTION("7. Transacción con ROLLBACK");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
    db.Exec("INSERT INTO t (name) VALUES ('original')");

    CHECK(db.Begin(), "BEGIN");

    db.Prepare("INSERT INTO t (name) VALUES (?)");
    db.BindText(1, "temporal");
    db.Step();
    db.Finalize();

    CHECK(db.Rollback(), "ROLLBACK");

    db.Query("SELECT COUNT(*) AS n FROM t");
    db.Goto(0);
    CHECK(db.Int("n") == 1, "Una sola fila tras ROLLBACK");

    db.Query("SELECT name FROM t");
    db.Goto(0);
    CHECK(db.Text("name") == "original", "Solo queda 'original'");

    db.Close();
}

//---------------------------------------------------------------------------
// 8. Tipos de datos (INTEGER, REAL, TEXT, BLOB, NULL)
//---------------------------------------------------------------------------
void Test_DataTypes()
{
    SECTION("8. Tipos de datos");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE t ("
            "  i INTEGER,"
            "  r REAL,"
            "  t TEXT,"
            "  b BLOB,"
            "  n TEXT"
            ")");

    db.Prepare("INSERT INTO t (i, r, t, b, n) VALUES (?, ?, ?, ?, ?)");
    db.BindInt(1, 42);
    db.BindDouble(2, 3.14159);
    db.BindText(3, "hola mundo");
    const unsigned char bin[] = { 0x00, 0x01, 0x02, 0x03, 0xFF };
    db.BindBlob(4, bin, sizeof(bin));
    db.BindNull(5);
    db.Step();
    db.Finalize();

    db.Query("SELECT i, r, t, b, n FROM t");
    db.Goto(0);

    CHECK(db.Int("i") == 42, "INTEGER == 42");
    CHECK(db.Double("r") > 3.14 && db.Double("r") < 3.15, "REAL ≈ 3.14159");
    CHECK(db.Text("t") == "hola mundo", "TEXT == 'hola mundo'");
    CHECK(db.IsNull("n"), "NULL detectado en 'n'");
    CHECK(!db.IsNull("i"), "No NULL en 'i'");

    db.Close();
}

//---------------------------------------------------------------------------
// 9. Números negativos, grandes, etc.
//---------------------------------------------------------------------------
void Test_Numbers()
{
    SECTION("9. Números especiales");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("CREATE TABLE t (i INTEGER, r REAL)");

    db.Prepare("INSERT INTO t (i, r) VALUES (?, ?)");
    db.BindInt(1, -12345);
    db.BindDouble(2, -0.5);
    db.Step();
    db.Finalize();

    db.Prepare("INSERT INTO t (i, r) VALUES (?, ?)");
    db.BindInt(1, 2147483647);   // INT_MAX
    db.BindDouble(2, 1e20);
    db.Step();
    db.Finalize();

    db.Query("SELECT i, r FROM t ORDER BY i");

    db.Goto(0);
    CHECK(db.Int("i") == -12345, "Negativo pequeño");
    CHECK(db.Double("r") == -0.5, "Real negativo");

    db.Goto(1);
    CHECK(db.Int("i") == 2147483647, "INT_MAX");
    CHECK(db.Double("r") > 1e19, "Real grande");

    // Int64 explícito
    db.Exec("CREATE TABLE big (n INTEGER)");
    db.Prepare("INSERT INTO big (n) VALUES (?)");
    const std::int64_t big = 9000000000LL;   // > 2^31
    db.BindInt64(1, big);
    db.Step();
    db.Finalize();

    db.Query("SELECT n FROM big");
    db.Goto(0);
    CHECK(db.Int64("n") == big, "Int64 preservado");

    db.Close();
}

//---------------------------------------------------------------------------
// 10. JSON1 (funciones JSON)
//---------------------------------------------------------------------------
void Test_JSON()
{
    SECTION("10. JSON1");

    CTSqlite db;
    db.Open(":memory:");

    // Comprobar que JSON1 está activado
    if (db.Query("SELECT json_extract('{\"a\":123}', '$.a') AS v")
        && db.RowCount() > 0)
    {
        db.Goto(0);
        CHECK(db.Int("v") == 123, "json_extract funciona");

        // Más complejo
        db.Query("SELECT json_extract('{\"user\":{\"name\":\"Juan\"}}', '$.user.name') AS n");
        db.Goto(0);
        CHECK(db.Text("n") == "Juan", "json_extract anidado");
    } else {
        CHECK(false, "JSON1 no activado en esta compilación");
    }

    db.Close();
}

//---------------------------------------------------------------------------
// 11. Concurrencia multi-hilo (lectura mientras escritura)
//---------------------------------------------------------------------------
void Test_Concurrency()
{
    SECTION("11. WAL mode (lectura + escritura)");

    // WAL solo funciona con ficheros en disco, no con :memory:
    // Usamos el directorio temporal del sistema.
    const std::string dbPath =
        (std::filesystem::temp_directory_path() / "test_wal.db3").string();

    // Borrar ficheros anteriores si existen
    std::error_code ec;
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath + "-wal", ec);
    std::filesystem::remove(dbPath + "-shm", ec);

    CTSqlite db;
    if (!db.Open(dbPath)) {
        CHECK(false, "No se pudo abrir " + dbPath + ": " + db.LastError());
        return;
    }

    // Activar WAL ANTES de crear tablas
    db.Exec("PRAGMA journal_mode=WAL");

    // Comprobar que WAL está activo
    const std::string jm = db.Pragma("journal_mode");
    CHECK(jm == "wal", "journal_mode == wal (era '" + jm + "')");

    // Crear tabla y probar transacción
    db.Exec("CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY, name TEXT)");

    db.Begin();
    db.Exec("INSERT INTO t (name) VALUES ('wal-test')");
    db.Commit();

    db.Query("SELECT name FROM t");
    if (db.RowCount() > 0) {
        db.Goto(0);
        CHECK(db.Text("name") == "wal-test", "Datos visibles tras WAL commit");
    } else {
        CHECK(false, "No hay filas tras COMMIT");
    }

    db.Close();

    // Limpiar ficheros temporales
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath + "-wal", ec);
    std::filesystem::remove(dbPath + "-shm", ec);
}
//---------------------------------------------------------------------------
// 12. LastError
//---------------------------------------------------------------------------
void Test_LastError()
{
    SECTION("12. LastError");

    CTSqlite db;
    db.Open(":memory:");

    CHECK(db.LastError().empty(), "Sin error: LastError() vacío");

    db.Exec("SELECT * FROM tabla_que_no_existe");
    CHECK(!db.LastError().empty(), "Tras error: LastError() no vacío");
    std::cout << "     → Mensaje: " << db.LastError() << std::endl;

    db.Close();
}

//---------------------------------------------------------------------------
// 13. Rendimiento (10.000 inserts en transacción)
//---------------------------------------------------------------------------
void Test_Performance()
{
    SECTION("13. Rendimiento (10.000 INSERT reusando statement)");

    CTSqlite db;
    db.Open(":memory:");
    db.Exec("PRAGMA journal_mode=MEMORY");   // para test rápido
    db.Exec("CREATE TABLE perf (id INTEGER PRIMARY KEY, name TEXT, value INTEGER)");

    const auto t0 = std::chrono::steady_clock::now();

    db.Begin();

    // Prepare UNA SOLA VEZ, fuera del bucle
    db.Prepare("INSERT INTO perf (name, value) VALUES (?, ?)");

    for (int i = 0; i < 10000; ++i) {
        db.ClearBindings();                                 // limpia binds anteriores
        db.BindText(1, "item_" + std::to_string(i));        // nuevos valores
        db.BindInt(2, i);
        db.Step();                                          // ejecuta
        db.Reset();                                         // listo para reusar
    }

    db.Finalize();                                          // libera el statement

    db.Commit();

    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    db.Query("SELECT COUNT(*) AS n FROM perf");
    db.Goto(0);
    CHECK(db.Int("n") == 10000, "10.000 filas insertadas");

    std::cout << "     → " << ms << " ms ("
              << (ms > 0 ? 10000 / ms : 0) << "K inserts/seg aprox)" << std::endl;

    db.Close();
}

} // namespace

//===========================================================================
// Test principal
//===========================================================================
bool TestCTSqlite()
{
    std::cout << "\n";
    std::cout << "+----------------------------------------------+\n";
    std::cout << "|  Test CTSqlite                              |\n";
    std::cout << "+----------------------------------------------+\n";

    g_ok = 0;
    g_fail = 0;

    const auto t0 = std::chrono::steady_clock::now();

    Test_OpenClose();
    Test_Exec();
    Test_QuerySimple();
    Test_PrepareInsert();
    Test_PrepareSelect();
    Test_TransactionCommit();
    Test_TransactionRollback();
    Test_DataTypes();
    Test_Numbers();
    Test_JSON();
    Test_Concurrency();
    Test_LastError();
    Test_Performance();

    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "\n";
    std::cout << "+----------------------------------------------+\n";
    std::cout << "|  Resultado                                  |\n";
    std::cout << "+----------------------------------------------+\n";
    std::cout << "  OK:      " << g_ok   << "\n";
    std::cout << "  FAIL:    " << g_fail << "\n";
    std::cout << "  Tiempo:  " << ms << " ms\n";

    if (g_fail == 0) {
        std::cout << "  ✅ TODOS LOS TESTS PASARON\n";
    } else {
        std::cout << "  ❌ HAY " << g_fail << " FALLO(S)\n";
    }

    std::cout << "+----------------------------------------------+\n\n";

    return g_fail == 0;
}
