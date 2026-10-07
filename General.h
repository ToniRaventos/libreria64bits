//---------------------------------------------------------------------------
// General.h — utilidades comunes portátiles (sin Win32)
//---------------------------------------------------------------------------
#ifndef GeneralH
#define GeneralH
//---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <locale>
#include <sstream>

#include "FCTString.h"
#include "FCTStringList.h"

//---------------------------------------------------------------------------
// Timestamp portable (antes TIMESTAMP_STRUCT_TRC).
// Se mantiene el typedef por compatibilidad con código existente.
//---------------------------------------------------------------------------
struct CTTimestamp
{
    std::int16_t  year;
    std::int16_t  month;
    std::int16_t  day;
    std::int16_t  hour;
    std::int16_t  minute;
    std::int16_t  second;
    std::uint32_t fraction;   // fracción de segundo (0 .. 999_999_999)
};

// Alias histórico
typedef CTTimestamp TIMESTAMP_STRUCT_TRC;

//---------------------------------------------------------------------------
// Cadenas: mayúsculas / minúsculas
//---------------------------------------------------------------------------
std::string  toLower(const std::string& str);

std::string  Upper(const std::string& texto);
std::string  Lower(const std::string& texto);
std::wstring Upper(const std::wstring& texto);
std::wstring Lower(const std::wstring& texto);

//---------------------------------------------------------------------------
// Valores nulos / por defecto
//---------------------------------------------------------------------------
int          isNull(int v1, int v2);
double       isNull(double v1, double v2);
std::string  isNull(std::string v1, std::string v2);
std::wstring isNull(std::wstring v1, std::wstring v2);
CTString     isNull(CTString v1, std::wstring v2);
bool         isNull(CTString vl);

bool isNull(void* vl);
bool isNull(int txt);
bool isNull(double txt);
bool isNull(std::string txt);
bool isNull(std::wstring txt);
bool isNull(char* txt);
bool isNull(wchar_t* txt);

//---------------------------------------------------------------------------
// Copia / concat / comparación de cadenas (portables, sin Win32)
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Copia / concat / comparación de cadenas
//
// En Windows (incluido C++Builder) estas funciones YA EXISTEN en <winbase.h>
// con la semántica clásica (devuelven LPWSTR/LPSTR). No podemos redefinirlas
// sin colisionar con la API del sistema.
//
// En plataformas POSIX (Linux, macOS, ...) no existen, así que las
// proporcionamos nosotros con una semántica equivalente y ampliada
// (aceptan char* y wchar_t* mezclados, devuelven longitud copiada).
//
// El resto del código sigue funcionando igual: en Windows llama a las de
// Win32, en POSIX a las nuestras.
//---------------------------------------------------------------------------

#if !defined(_WIN32) && !defined(__BORLANDC__) && !defined(_MSC_VER)

// lstrcpy: copia src en dst. Devuelve longitud copiada (sin NUL).
int lstrcpy(char*    dst, const char*    src);
int lstrcpy(char*    dst, const wchar_t* src);
int lstrcpy(wchar_t* dst, const char*    src);
int lstrcpy(wchar_t* dst, const wchar_t* src);

// lstrcat: añade src al final de dst. Devuelve longitud total.
int lstrcat(char*    dst, const char*    src);
int lstrcat(char*    dst, const wchar_t* src);
int lstrcat(wchar_t* dst, const char*    src);
int lstrcat(wchar_t* dst, const wchar_t* src);

// lstrcmpi: comparación case-insensitive. Devuelve <0, 0, >0.
int lstrcmpi(const char*    s1, const char*    s2);
int lstrcmpi(const wchar_t* s1, const wchar_t* s2);
int lstrcmpi(const char*    s1, const wchar_t* s2);
int lstrcmpi(const wchar_t* s1, const char*    s2);

// Variantes seguras (con tamaño de buffer destino). Devuelven -1 si truncan.
int lstrcpy_s(char*    dst, std::size_t dstSize, const char*    src);
int lstrcpy_s(wchar_t* dst, std::size_t dstSize, const wchar_t* src);
int lstrcat_s(char*    dst, std::size_t dstSize, const char*    src);
int lstrcat_s(wchar_t* dst, std::size_t dstSize, const wchar_t* src);


inline int lstrcpyA(char*    d, const char*    s) { return lstrcpy(d, s); }
inline int lstrcpyA(char*    d, const wchar_t* s) { return lstrcpy(d, s); }
inline int lstrcpyA(wchar_t* d, const char*    s) { return lstrcpy(d, s); }
inline int lstrcpyA(wchar_t* d, const wchar_t* s) { return lstrcpy(d, s); }

inline int lstrcatA(char*    d, const char*    s) { return lstrcat(d, s); }
inline int lstrcatA(char*    d, const wchar_t* s) { return lstrcat(d, s); }
inline int lstrcatA(wchar_t* d, const char*    s) { return lstrcat(d, s); }
inline int lstrcatA(wchar_t* d, const wchar_t* s) { return lstrcat(d, s); }

inline int lstrcmpiA(const char*    a, const char*    b) { return lstrcmpi(a, b); }
inline int lstrcmpiA(const char*    a, const wchar_t* b) { return lstrcmpi(a, b); }
inline int lstrcmpiA(const wchar_t* a, const char*    b) { return lstrcmpi(a, b); }
inline int lstrcmpiA(const wchar_t* a, const wchar_t* b) { return lstrcmpi(a, b); }


#endif // !_WIN32 (y compatibles)

//---------------------------------------------------------------------------
// NumCmp: compara dos cadenas como números. Devuelve <0, 0, >0.
// No colisiona con Win32: es propio y siempre disponible.
//---------------------------------------------------------------------------
int NumCmp(const char*    s1, const char*    s2);
int NumCmp(const wchar_t* s1, const wchar_t* s2);



// Variantes seguras (con tamaño de buffer destino). Devuelven -1 si truncan.
int lstrcpy_s(char*    dst, std::size_t dstSize, const char*    src);
int lstrcpy_s(wchar_t* dst, std::size_t dstSize, const wchar_t* src);
int lstrcat_s(char*    dst, std::size_t dstSize, const char*    src);
int lstrcat_s(wchar_t* dst, std::size_t dstSize, const wchar_t* src);

// Alias *A por compatibilidad con código antiguo




//---------------------------------------------------------------------------
// Timestamps
//---------------------------------------------------------------------------
int datcpy(TIMESTAMP_STRUCT_TRC* h1, TIMESTAMP_STRUCT_TRC* h2);

TIMESTAMP_STRUCT_TRC GetDate();
int                  GetDate(TIMESTAMP_STRUCT_TRC* sub1);

int    DifHora(const TIMESTAMP_STRUCT_TRC& t1, const TIMESTAMP_STRUCT_TRC& t2);
int    DifHora(const std::tm& t1, const std::tm& t2);
double DifHora_Now(const TIMESTAMP_STRUCT_TRC& t);

std::string TimestampToISO8601(const TIMESTAMP_STRUCT_TRC& t);

//---------------------------------------------------------------------------
// Formato numérico
//---------------------------------------------------------------------------
std::string XTrans(double number, int length, int decimals);
std::string Trans (double number, int length, int decimals,
                   std::string sufix = "");

double Round(double number, int decimals);
int    ConvertirEnMinutos(double horaDecimal);
double IncrementarSegundo(double horaDecimal, int inc, bool div);

//---------------------------------------------------------------------------
// Horas
//---------------------------------------------------------------------------
double dtoh(double decimalHours);
double htod(double sub1);
double htod(TIMESTAMP_STRUCT_TRC su1, bool sec);

CTString strhor(double hora, int dec);
double   val(const CTString& vl);

std::string Strhor(TIMESTAMP_STRUCT_TRC su1, bool sec = false);
std::string Strhor(double su1, bool sec = false, bool isdecimal = true);

std::string formatTimeSegons(int totalSeconds);

//---------------------------------------------------------------------------
// Formato heredado que devuelve char* (usa buffer estático, no thread-safe).
// Se mantiene por compatibilidad.
//---------------------------------------------------------------------------
char* Bstr  (double su1, int sub, int sub2);
char* str   (double su1, int sub, int sub2);
char* trans (double su1, int sub, int sub2, int decminim = -1, bool blanc = false);
char* Xtrans(double su1, int sub, int sub2);

CTString cat(CTString s1,
             CTString s2 = "",
             CTString s3 = "",
             CTString s4 = "",
             CTString s5 = "",
             CTString s6 = "",
             CTString s7 = "");

//---------------------------------------------------------------------------
// Sistema de ficheros (portable, C++17 std::filesystem)
//---------------------------------------------------------------------------
bool        fileExists(const std::string& filename);
std::string extractFilePath(const std::string fullPath);
bool        crearDirectorioSiNoExiste(const std::string& ruta);
std::string getFileName(const std::string& ruta);

//---------------------------------------------------------------------------
// Conversiones de cadena
//---------------------------------------------------------------------------
std::string  sToU(const std::string input);
std::wstring StringToWString(const std::string& str);

//---------------------------------------------------------------------------
// Color
//---------------------------------------------------------------------------
std::string RGBtoHTML(int r, int g, int b);
std::string RGBtoHTML(int rgb);



#endif // GeneralH
