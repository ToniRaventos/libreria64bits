//---------------------------------------------------------------------------
// TSqlTable.h — tabla de resultados ODBC portable (sin Win32)
//
// Portado a 64 bits. Mantiene la API y la filosofía original.
//
// Modo lazy:
//   Si el tamaño de columna > lazyThreshold_ y el tipo es
//   CHAR/WCHAR/BINARY, el campo se marca Lazy=true. No se reserva buffer
//   ni se hace SQLBindCol; se lee bajo demanda con SQLGetData cuando el
//   usuario llama a Store/Text/Get/GetImage/ReadBLOB.
//
//   LÍMITE: el modo lazy solo se activa con ROW_SIZE == 1 (uso habitual).
//   Con ROW_SIZE > 1, los campos grandes se truncan a lazyThreshold_
//   para evitar reservar Gigabytes de memoria.
//
// ODBC 3.x: SQLSetStmtAttr, SQLDescribeColW, SQLGetData.
//---------------------------------------------------------------------------
#ifndef TSQLTABLE_H
#define TSQLTABLE_H

#include "TDataBase.h"
#include "FCTSTD.h"
#include "FCTString.h"

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <iterator>

//---------------------------------------------------------------------------
// Constantes (compatibilidad con la API original)
//---------------------------------------------------------------------------
#define BOF_ERR 62
#define EOF_ERR 61
#define ADD_REG (-1)
#define MAX_BLOB (10 * 1000 * 1000L)
#define MAX_LEN_CHAR (10 * 1000 * 1000L)

//---------------------------------------------------------------------------
// Umbral por defecto para activar el modo lazy (256 KB)
//---------------------------------------------------------------------------
constexpr std::size_t TSQLTABLE_DEFAULT_LAZY_THRESHOLD = 256 * 1024;

//---------------------------------------------------------------------------
// ColSql: descriptor de columna
//---------------------------------------------------------------------------
struct ColSql
{
    std::wstring NombreCampo;
    std::size_t ColumnSizePtr;
    short int NameLengthPtr;
    short int DecimalDigitsPtr;
    short int NullablePtr;
    short int DataType;
    short int Type;
    short int ColumnIndex;

    std::vector<SQLLEN> Longitud;
    std::vector<char> Buffer;

    bool Lazy;
    int LazyRow;
    std::vector<char> LazyCache;
    SQLLEN LazyLen;

    ColSql() :
        ColumnSizePtr(0), NameLengthPtr(0), DecimalDigitsPtr(0), NullablePtr(0),
        DataType(0), Type(0), ColumnIndex(0), Lazy(false), LazyRow(-1),
        LazyLen(0)
    {
    }

    char* DataAt(int row)
    {
        return Buffer.data() + ColumnSizePtr * static_cast<std::size_t>(row);
    }
    const char* DataAt(int row) const
    {
        return Buffer.data() + ColumnSizePtr * static_cast<std::size_t>(row);
    }
};

//---------------------------------------------------------------------------
// Index_Cursor
//---------------------------------------------------------------------------
struct Index_Cursor
{
    std::wstring Campo;
    ColSql* Fl;

    Index_Cursor() : Fl(nullptr) {}
};

//---------------------------------------------------------------------------
// CTSqlTable
//---------------------------------------------------------------------------
class CTSqlTable : public CTSTD
{
  protected:
    int ROW_SIZE;
    bool m_bSupressErrors;
    int fMaximBlob;
    std::size_t lazyThreshold_;

    std::vector<Index_Cursor> Index;
    long int RowCountPtr;
    int fRowCount;
    int fSelected;
    RETCODE retcode;
    bool hihaError;

    void CreateFields(bool bind = true);
    void Limpiar();
    void Replace(ColSql* field, void* vl);
    int StoreLlarg(wchar_t* mj, int tope);
    int StoreLlarg(char* mj, int tope);
    void Store(ColSql* field, void* vl, int llarg = -1);

    int Sort(Index_Cursor* values, int start, int end_list);
    ColSql* Find(const CTString &sub1);
    std::string GetErrorSQL();
    void VerificaTipus(int tipus, short int* SqlTipus, std::size_t* size);

    bool EnsureConnected();
    bool IsConnectionAlive();
    bool IsConnectionLost();

    bool ReadLazyField(ColSql &f, int row);
    void InvalidarLazyCache();
  public:
    CTDataBase* DB;
    std::vector<SQLUSMALLINT> RowStat;
    short int ColumnCountPtr;

    std::vector<ColSql> Fields;
    SQLHANDLE hstmt;
    std::vector<SQLUSMALLINT> rowProceed;

    bool isOpen;

    SQLWCHAR SqlState[6];
    bool isWchar_t;

    std::string Sql;
  public:
    CTSqlTable(CTDataBase* db, bool iswchar_t = false);
    ~CTSqlTable() override;

    int Open(int tipus = 0);
    void Close(bool borra = false);
    void InitTabla();

    int ColCount();
    int RowCount();
    void SetMaxRow(int num);
    int GetMaxRow(int num);

    int GetError();
    int Fetch();
    int Stop();
    int Execute(int maxrow = 1);
    int ExecuteSql();

    int GetMatriz();
    int CallGetMatrix();
    int Call(int maxrow = 1);

    int Recno();
    int Goto(int selec);
    bool Rollback();
    bool Commit();
    int Update(int _eof);

    void ActiveErrors(bool act);
    void DisplayError();

    std::string ISO8601(ColSql* fl);
    ColSql* GetField(int pos);
    int toInt(ColSql* fl);
    double toDouble(ColSql* fl);

    void* Get(wchar_t* name);
    void* Get(int pos);
    CTString Say(int p, int x = 0);
    CTString Text(ColSql* fl);
    CTString Text(CTString nom);
    double Double(CTString nom);
    int Int(CTString nom);
    ColSql* GetCol(CTString name);
    ColSql* GetCol(int col);
    int Aling(int col);
    int GetColAling(int col);
    int GetColWidth(void* hwnd, int col, int max = 350);
    int GetColLen(int col);
    int GetColType(int col);

    void Set(CTString name, void* bf, int x = 0);
    CTString GetColCaption(int col);
    void Store(CTString nom, void* vl, int llarg = -1);
    void Store(CTString nom, wchar_t* vl);
    void Store(CTString nom, CTString &vl);
    void Store(CTString nom, std::string &vl);

    void PosaLongitud(ColSql* field);
    void Replace(CTString nom, void* vl);
    void Replace(CTString nom, wchar_t* vl);
    void Replace(CTString nom, CTString vl);

    int SetMaximBlob(int maxi);
    void ReadBLOB(CTString nom, void* vl, int llarg);
    void WriteBLOB(CTString nom, void* vl, int llarg);
    std::vector<unsigned char> GetImage(CTString nombreCampo);

    int TypeField(CTString nom);
    int Copy(std::string &txt, wchar_t* bf, int ll);
    int Copy(std::string &txt, char* bf, int ll);

    void SetLazyThreshold(std::size_t bytes)
    {
        lazyThreshold_ = bytes;
    }
    std::size_t GetLazyThreshold() const
    {
        return lazyThreshold_;
    }

    bool Prepare(const std::wstring &sql);
    bool Bind(int paramIndex, SQLSMALLINT paramType, SQLSMALLINT cType,
        SQLSMALLINT sqlType, SQLULEN colSize, SQLSMALLINT decDigits,
        SQLPOINTER value, SQLLEN bufferLen, SQLLEN* strLen_or_Ind);
    bool BindInt(int index, int &value);
    bool BindWString(int index, const std::wstring &value);
    bool BindString(int index, const std::string &value);
    bool ExecutePrepared(int maxrow);
    bool CallProcedure(
        const std::wstring &procName, const std::vector<std::wstring> &params);

    bool PrepareStatement(const std::wstring &sql)
    {
        return Prepare(sql);
    }
    bool BindParameter(int index, SQLSMALLINT sqlType, void* value,
        SQLLEN sizeOrIndicator, SQLSMALLINT cType);
    bool ExecuteProcedure(const std::wstring &procedureName,
        const std::vector<std::wstring> &params)
    {
        return CallProcedure(procedureName, params);
    }
};

#endif // TSQLTABLE_H

