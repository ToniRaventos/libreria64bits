//---------------------------------------------------------------------------
// sqlite3_custom.h — definiciones de compilación para SQLite
//---------------------------------------------------------------------------
#ifndef SQLITE3_CUSTOM_H
#define SQLITE3_CUSTOM_H

#define SQLITE_THREADSAFE               1
#define SQLITE_ENABLE_JSON1
#define SQLITE_ENABLE_COLUMN_METADATA
#define SQLITE_OMIT_LOAD_EXTENSION
#define SQLITE_DQS                      0
#define SQLITE_DEFAULT_MEMORY_USAGE     5000000

#endif // SQLITE3_CUSTOM_H
