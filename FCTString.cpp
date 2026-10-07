//---------------------------------------------------------------------------
// FCTString.cpp
//
// Reescritura 64-bit-safe / C++ estandar. Ver notas en FCTString.h.
//---------------------------------------------------------------------------

#pragma hdrstop

#ifdef TRC_WIN32
 #include <Vcl.h>
#endif

#include "FCTString.h"
#include <vector>
#include <cctype>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cstdarg>

//---------------------------------------------------------------------------
#pragma package(smart_init)

size_t Buffer_Size(const char* bf)
{
    return bf ? std::strlen(bf) : 0;
}

// NOTA: las comparaciones de acentos del fichero original llegaron con la
// codificacion corrupta (caracteres de reemplazo). He dejado la funcion con
// normalizacion ASCII basica (isalpha/tolower); si me dices que vocales
// acentuadas / enye iban ahi, la completo.
std::string normalizeString(const std::string& input)
{
    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input) {
        if (std::isalpha(c))
            result += static_cast<char>(std::tolower(c));
        else
            result += static_cast<char>(c);
    }
    return result;
}

// Se mantienen tal cual (no estan declaradas en el .h, asi que nada fuera
// de este .cpp puede llamarlas salvo que algo declare su propio prototipo).
// Aviso: no comprueban el tamano de "result" contra lo escrito - si algo
// del proyecto las usa con cadenas largas, conviene revisarlas aparte.
void formatString(char* result, const char* format, ...) {
    va_list args;
    va_start(args, format);

    size_t formatLen = strlen(format);
    size_t resultLen = 0;

    for (size_t i = 0; i < formatLen; ++i) {
        if (format[i] == '%' && i + 1 < formatLen) {
            switch (format[i + 1]) {
                case 's': {
                    const char* str = va_arg(args, const char*);
                    size_t strLen = strlen(str);
                    strncpy(result + resultLen, str, strLen);
                    resultLen += strLen;
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    resultLen += sprintf(result + resultLen, "%d", num);
                    break;
                }
                default:
                    result[resultLen++] = format[i];
                    result[resultLen++] = format[i + 1];
                    break;
            }
            ++i;
        } else {
            result[resultLen++] = format[i];
        }
    }

    result[resultLen] = '\0';
    va_end(args);
}

void formatString2(const char* format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; ++i) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            switch (format[i + 1]) {
                case 's': {
                    const char* str = va_arg(args, const char*);
                    printf("Tipo: Cadena, Valor: %s\n", str);
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    printf("Tipo: Entero, Valor: %d\n", num);
                    break;
                }
                case 'f': {
                    double num = va_arg(args, double);
                    printf("Tipo: Doble, Valor: %lf\n", num);
                    break;
                }
                default:
                    break;
            }

            ++i;
        }
    }

    va_end(args);
}

//---------------------------------------------------------------------------
// Conversion helpers internas char <-> wchar_t (evitan reinterpret_cast y
// la variable "bufferSize" sin declarar que tenia el original)
//---------------------------------------------------------------------------
namespace {

#ifdef _CHART
std::wstring NarrowToStorage(const char* str, size_t len) {
    std::vector<wchar_t> buf(len + 1);
    size_t converted = std::mbstowcs(buf.data(), str, len + 1);
    if (converted == static_cast<size_t>(-1)) converted = 0;
    return std::wstring(buf.data(), converted);
}
#else
std::string NarrowToStorage(const char* str, size_t len) {
    return std::string(str, len);
}
#endif

#ifdef _CHART
std::wstring WideToStorage(const wchar_t* str, size_t len) {
    return std::wstring(str, len);
}
#else
std::string WideToStorage(const wchar_t* str, size_t len) {
    std::vector<char> buf(len * 4 + 1);
    size_t converted = std::wcstombs(buf.data(), str, buf.size());
    if (converted == static_cast<size_t>(-1)) converted = 0;
    return std::string(buf.data(), converted);
}
#endif

// A diferencia de WideToStorage, esta SIEMPRE devuelve std::string,
// independientemente de si CharT es char o wchar_t. La usa ToString().
std::string ToNarrowString(const wchar_t* str, size_t len) {
    std::vector<char> buf(len * 4 + 1);
    size_t converted = std::wcstombs(buf.data(), str, buf.size());
    if (converted == static_cast<size_t>(-1)) converted = 0;
    return std::string(buf.data(), converted);
}

} // namespace

//---------------------------------------------------------------------------
CTString UtoS(const char* src)
{
    CTString outputString;
    if (!src) return outputString;

    size_t len = std::strlen(src);
    for (size_t a = 0; a < len; ++a) {
        if (src[a] == '\\' && a + 2 < len && src[a + 1] == '\\' && src[a + 2] == 'u' && a + 6 < len) {
            char match[5] = {0, 0, 0, 0, 0};
            for (int b = 0; b < 4; ++b) match[b] = src[a + 3 + b];
            outputString += static_cast<char>(std::stoi(match, nullptr, 16));
            a += 6;
        } else {
            outputString += src[a];
        }
    }
    return outputString;
}

//---------------------------------------------------------------------------
// Constructores / destructor
//---------------------------------------------------------------------------
CTString::CTString() : position(npos) {}

CTString::CTString(const char* str)
{
    if (!str) str = "";
    data = NarrowToStorage(str, std::strlen(str));
    position = data.empty() ? npos : data.size() - 1;
}

CTString::CTString(const wchar_t* wstr)
{
    if (!wstr) wstr = L"";
    data = WideToStorage(wstr, std::wcslen(wstr));
    position = data.empty() ? npos : data.size() - 1;
}

#ifdef TRC_WIN32
CTString::CTString(const String& other)
{
    std::wstring ws(other.c_str());
    data = WideToStorage(ws.c_str(), ws.size());
    position = data.empty() ? npos : data.size() - 1;
}
#endif

CTString::CTString(const std::string& str)
{
    data = NarrowToStorage(str.c_str(), str.size());
    position = data.empty() ? npos : data.size() - 1;
}

CTString::CTString(const std::wstring& wstr)
{
    data = WideToStorage(wstr.c_str(), wstr.size());
    position = data.empty() ? npos : data.size() - 1;
}

CTString::CTString(int num)
{
#ifdef _CHART
    data = std::to_wstring(num);
#else
    data = std::to_string(num);
#endif
    position = data.empty() ? npos : data.size() - 1;
}

CTString::CTString(double num)
{
#ifdef _CHART
    data = std::to_wstring(num);
#else
    data = std::to_string(num); // equivale a sprintf("%f", num), igual que el original
#endif
    position = data.empty() ? npos : data.size() - 1;
}

CTString::CTString(const CTString& other) : data(other.data), position(other.position) {}

CTString::CTString(CTString&& other) noexcept
    : data(std::move(other.data)), position(other.position)
{
    other.position = npos;
}

CTString::~CTString() = default;

void CTString::Clear()
{
    data.clear();
    position = npos;
}

//---------------------------------------------------------------------------
// Longitud / cursor / copia
//---------------------------------------------------------------------------
size_t CTString::SetLength(size_t ll)
{
    data.assign(ll, CharT(0));
    position = ll > 0 ? ll - 1 : npos;
    return ll;
}

size_t CTString::Length() const { return data.size(); }

size_t CTString::Copy(const char* bf, size_t ll)
{
    data = NarrowToStorage(bf, ll);
    position = data.empty() ? npos : data.size() - 1;
    return ll;
}

size_t CTString::Copy(const wchar_t* bf, size_t ll)
{
    data = WideToStorage(bf, ll);
    position = data.empty() ? npos : data.size() - 1;
    return ll;
}

size_t CTString::Copy(const CTString& bf, size_t ll)
{
    data = bf.data.substr(0, ll);
    position = data.empty() ? npos : data.size() - 1;
    return ll;
}

CharT* CTString::GetData()
{
    // std::basic_string garantiza almacenamiento contiguo y &data[0] valido
    // (incluso vacio, apunta al terminador nulo) desde C++11.
    return &data[0];
}

const CharT* CTString::c_str() const { return data.c_str(); }

std::string CTString::u_str() const
{
    std::ostringstream out;
    for (size_t a = 0; a < data.size(); ++a) {
        unsigned int c = static_cast<unsigned int>(static_cast<unsigned char>(data[a]));
        if (c < 127)
            out << static_cast<char>(c);
        else
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << c;
    }
    return out.str();
}

//---------------------------------------------------------------------------
// Asignacion
//---------------------------------------------------------------------------
CTString& CTString::operator=(const CTString& other)
{
    if (this != &other) {
        data = other.data;
        position = other.position;
    }
    return *this;
}

CTString& CTString::operator=(CTString&& other) noexcept
{
    if (this != &other) {
        data = std::move(other.data);
        position = other.position;
        other.position = npos;
    }
    return *this;
}

//---------------------------------------------------------------------------
// Comparacion (delegada en std::basic_string; antes reimplementaba
// strcmp/wcscmp a mano y la rama _CHART de operator< tenia un typo -
// "str2.c_str()()" - que no compilaba)
//---------------------------------------------------------------------------
bool CTString::operator==(const CTString& str2) const { return data == str2.data; }
bool CTString::operator!=(const CTString& str2) const { return data != str2.data; }
bool CTString::operator<(const CTString& str2)  const { return data <  str2.data; }
bool CTString::operator<=(const CTString& str2) const { return data <= str2.data; }
bool CTString::operator>(const CTString& str2)  const { return data >  str2.data; }
bool CTString::operator>=(const CTString& str2) const { return data >= str2.data; }

int CTString::compare(const CTString& vl) const { return data.compare(vl.data); }

//---------------------------------------------------------------------------
// operator+=
//
// NOTA IMPORTANTE: en el original, operator+=(char) NO crecia el buffer -
// sobreescribia el ultimo caracter existente (bug real, inconsistente con
// el resto de +=). Aqui SI crece, igual que los demas overloads y que
// std::string::operator+=. Si en el resto del proyecto algo dependia del
// comportamiento viejo (sobreescribir en vez de anadir), hay que revisarlo.
//---------------------------------------------------------------------------
CTString& CTString::operator+=(const CTString& other)
{
    data += other.data;
    position = data.empty() ? npos : data.size() - 1;
    return *this;
}

CTString& CTString::operator+=(CharT other)
{
    data.push_back(other);
    position = data.size() - 1;
    return *this;
}

CTString& CTString::operator+=(const char* other)
{
    return *this += CTString(other);
}

CTString& CTString::operator+=(const std::string& other)
{
    return *this += CTString(other);
}

//---------------------------------------------------------------------------
// operator+
//---------------------------------------------------------------------------
CTString CTString::operator+(const CTString& other) const
{
    CTString result(*this);
    result += other;
    return result;
}

CTString CTString::operator+(const char* str) const { return *this + CTString(str); }
CTString CTString::operator+(const wchar_t* str) const { return *this + CTString(str); }
CTString CTString::operator+(const std::string& str) const { return *this + CTString(str); }
CTString CTString::operator+(const std::wstring& str) const { return *this + CTString(str); }
CTString CTString::operator+(int num) const { return *this + CTString(num); }
CTString CTString::operator+(double num) const { return *this + CTString(num); }

//---------------------------------------------------------------------------
// Indexado
//---------------------------------------------------------------------------
CharT& CTString::operator[](size_t index)
{
    if (index >= data.size()) throw std::out_of_range("CTString: index out of range");
    return data[index];
}

const CharT& CTString::operator[](size_t index) const
{
    if (index >= data.size()) throw std::out_of_range("CTString: index out of range");
    return data[index];
}

//---------------------------------------------------------------------------
// Subcadenas
//---------------------------------------------------------------------------
CTString CTString::SubString(size_t startIndex, size_t len) const
{
    // Igual que std::string::substr: startIndex == size() es valido y
    // produce cadena vacia (por ejemplo Right(0) sobre una cadena no
    // vacia cae exactamente en ese caso). Solo se considera fuera de
    // rango si startIndex > size().
    if (startIndex >=data.size())
        throw std::out_of_range("CTString::SubString: invalid start index");
    return CTString(data.substr(startIndex, len));
}

CTString CTString::Left(size_t len) const
{
    return SubString(0, len);
}

CTString CTString::Right(size_t len) const
{
    if (len >= data.size()) return *this;
    return SubString(data.size() - len, len);
}

//---------------------------------------------------------------------------
// Conversion
//---------------------------------------------------------------------------
std::string CTString::ToString() const
{
#ifdef _CHART
    return ToNarrowString(data.c_str(), data.size());
#else
    return data;
#endif
}

int CTString::ToInt() const
{
#ifdef _CHART
    return static_cast<int>(std::wcstol(data.c_str(), nullptr, 10));
#else
    return std::atoi(data.c_str());
#endif
}

double CTString::ToDouble() const
{
#ifdef _CHART
    return std::wcstod(data.c_str(), nullptr);
#else
    return std::atof(data.c_str());
#endif
}

//---------------------------------------------------------------------------
// Mayusculas / minusculas
//---------------------------------------------------------------------------
CTString CTString::Upper() const
{
    CTString result(*this);
    for (auto& ch : result.data) {
#ifdef _CHART
        ch = static_cast<CharT>(std::towupper(static_cast<std::wint_t>(ch)));
#else
        ch = static_cast<CharT>(std::toupper(static_cast<unsigned char>(ch)));
#endif
    }
    return result;
}

CTString CTString::Lower() const
{
    CTString result(*this);
    for (auto& ch : result.data) {
#ifdef _CHART
        ch = static_cast<CharT>(std::towlower(static_cast<std::wint_t>(ch)));
#else
        ch = static_cast<CharT>(std::tolower(static_cast<unsigned char>(ch)));
#endif
    }
    return result;
}

//---------------------------------------------------------------------------
// Posicion / busqueda / reemplazo
//---------------------------------------------------------------------------
size_t CTString::SetPosition(size_t pos)
{
    if (data.empty() || pos >= data.size())
        position = data.empty() ? npos : data.size() - 1;
    else
        position = pos;
    return position;
}

size_t CTString::Find(const CTString& subStr, size_t pos) const
{
    if (pos > data.size()) return npos;
    return data.find(subStr.data, pos); // std::basic_string::find ya devuelve npos si no hay match
}

CTString CTString::Replace(const CTString& findStr, const CTString& replaceStr)
{
    // Guarda de bucle infinito: el original no comprobaba findStr vacio y
    // "pos += findLen" con findLen==0 nunca avanzaba.
    if (findStr.data.empty() || findStr.data.size() > data.size())
        return *this;

    std::basic_string<CharT> result;
    size_t prev = 0;
    size_t pos;
    while ((pos = data.find(findStr.data, prev)) != std::basic_string<CharT>::npos) {
        result.append(data, prev, pos - prev);
        result.append(replaceStr.data);
        prev = pos + findStr.data.size();
    }
    result.append(data, prev, data.size() - prev);

    data = std::move(result);
    position = data.empty() ? npos : data.size() - 1;
    return *this;
}
