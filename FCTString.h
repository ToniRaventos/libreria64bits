//---------------------------------------------------------------------------
// FCTString.h
//
// Reescritura para C++ estandar / 64-bit-safe (2026-09).
// Internamente usa std::basic_string<CharT> en vez de un buffer manual
// new[]/delete[], por lo que ya no hay riesgo de fugas ni de truncar
// punteros/tamanos al compilar para Win64. La API publica se mantiene
// igual que la version original salvo donde se indica lo contrario.
//---------------------------------------------------------------------------

#ifndef CTSTRING_H
#define CTSTRING_H

#ifdef TRC_WIN32
 #include <Vcl.h>
#endif

#ifdef _WIN32
 #include <tchar.h>
#else
 typedef char _TCHAR;
 #define _tmain main
#endif

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>

//#ifdef _CHART
using CharT = wchar_t;
//#else
//using CharT = char;
//#endif

// Helpers sueltos (no forman parte de la clase)
size_t Buffer_Size(const char* bf);
std::string normalizeString(const std::string& input);

class CTString
{
private:
    std::basic_string<CharT> data;
    size_t position; // "cursor" interno usado por SetLength/SetPosition/operator+=(CharT)

public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    CTString();
    CTString(const char* str);
    CTString(const wchar_t* wstr);

#ifdef TRC_WIN32
    CTString(const String& other);
#endif

    CTString(const std::string& str);
    CTString(const std::wstring& wstr);
    CTString(int num);
    CTString(double num);
    CTString(const CTString& other);
    CTString(CTString&& other) noexcept;
    ~CTString();

    void Clear();
    size_t SetLength(size_t ll);
    size_t Length() const;
    size_t Copy(const char* bf, size_t ll);
    size_t Copy(const wchar_t* bf, size_t ll);
    size_t Copy(const CTString& bf, size_t ll);

    CharT* GetData();
    const CharT* c_str() const;
    std::string u_str() const;

    CTString& operator=(const CTString& other);
    CTString& operator=(CTString&& other) noexcept;

    bool operator==(const CTString& str2) const;
    bool operator!=(const CTString& str2) const;
    bool operator<(const CTString& str2) const;
    bool operator<=(const CTString& str2) const;
    bool operator>(const CTString& str2) const;
    bool operator>=(const CTString& str2) const;

    CTString& operator+=(const CTString& other);
    CTString& operator+=(CharT other);
    CTString& operator+=(const char* other);
    CTString& operator+=(const std::string& other);

    CTString operator+(const CTString& other) const;
    CTString operator+(const char* str) const;
    CTString operator+(const wchar_t* str) const;
    CTString operator+(const std::string& str) const;
    CTString operator+(const std::wstring& str) const;
    CTString operator+(int num) const;
    CTString operator+(double num) const;

    CharT& operator[](size_t index);
    const CharT& operator[](size_t index) const;

    CTString SubString(size_t startIndex, size_t len) const;
    CTString Left(size_t len) const;
    CTString Right(size_t len) const;
    int ToInt() const;
    double ToDouble() const;
    std::string ToString() const;

    CTString Upper() const;
    CTString Lower() const;
    size_t SetPosition(size_t pos);
    size_t Find(const CTString& subStr, size_t pos = 0) const;
    CTString Replace(const CTString& findStr, const CTString& replaceStr);

    int compare(const CTString& vl) const;
};

CTString UtoS(const char* data);

#endif  // CTSTRING_H
