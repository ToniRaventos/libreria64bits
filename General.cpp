//---------------------------------------------------------------------------
// General.cpp — implementación portable (sin Win32)
//---------------------------------------------------------------------------
#include "General.h"

#include <filesystem>
#include <cwchar>
#include <cwctype>
#include <cctype>
#include <stdexcept>

namespace fs = std::filesystem;

//===========================================================================
// Helpers internos (namespace anónimo)
//===========================================================================
namespace {

// --- UTF-8 <-> wstring (UTF-16 en Windows, UTF-32 en otras plataformas) ---

inline void append_utf16(std::wstring& out, char32_t cp)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        if (cp <= 0xFFFF)
        {
            out.push_back(static_cast<wchar_t>(cp));
        }
        else
        {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    else
    {
        out.push_back(static_cast<wchar_t>(cp));
    }
}

std::wstring utf8_to_wstring(const char* s)
{
    std::wstring out;
    if (!s) return out;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    while (*p)
    {
        char32_t cp = 0;
        int extra = 0;

        if      (*p < 0x80)            { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0)  { cp = *p++ & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0)  { cp = *p++ & 0x0F; extra = 2; }
        else if ((*p & 0xF8) == 0xF0)  { cp = *p++ & 0x07; extra = 3; }
        else                           { ++p; continue; }

        bool ok = true;
        for (int i = 0; i < extra; ++i)
        {
            if ((*p & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (*p++ & 0x3F);
        }
        if (!ok) continue;

        append_utf16(out, cp);
    }
    return out;
}

std::string wstring_to_utf8(const wchar_t* s)
{
    std::string out;
    if (!s) return out;

    for (std::size_t i = 0; s[i] != 0; ++i)
    {
        char32_t cp = static_cast<char32_t>(static_cast<std::uint16_t>(s[i]));

        if constexpr (sizeof(wchar_t) == 2)
        {
            if (cp >= 0xD800 && cp <= 0xDBFF && s[i + 1] != 0)
            {
                char32_t lo = static_cast<char32_t>(
                                  static_cast<std::uint16_t>(s[i + 1]));
                if (lo >= 0xDC00 && lo <= 0xDFFF)
                {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    ++i;
                }
            }
        }

        if      (cp < 0x80)    out.push_back(static_cast<char>(cp));
        else if (cp < 0x800)   {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// --- Comparadores case-insensitive carácter a carácter ---

inline int cmp_i_char(char a, char b)
{
    int ca = std::toupper(static_cast<unsigned char>(a));
    int cb = std::toupper(static_cast<unsigned char>(b));
    return (ca < cb) ? -1 : (ca > cb) ? 1 : 0;
}

inline int cmp_i_wchar(wchar_t a, wchar_t b)
{
    wint_t ca = std::towupper(static_cast<wint_t>(a));
    wint_t cb = std::towupper(static_cast<wint_t>(b));
    return (ca < cb) ? -1 : (ca > cb) ? 1 : 0;
}

} // namespace

//===========================================================================
// Mayúsculas / minúsculas
//===========================================================================
std::string toLower(const std::string& str)
{
    std::string r = str;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

std::string Upper(const std::string& texto)
{
    std::string r = texto;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return r;
}

std::string Lower(const std::string& texto)
{
    std::string r = texto;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

std::wstring Upper(const std::wstring& texto)
{
    std::wstring r = texto;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return r;
}

std::wstring Lower(const std::wstring& texto)
{
    std::wstring r = texto;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return r;
}

//===========================================================================
// isNull
//===========================================================================
int    isNull(int v1, int v2)             { return (v1 == 0) ? v2 : v1; }
double isNull(double v1, double v2)       { return (v1 == 0.0) ? v2 : v1; }

std::string isNull(std::string v1, std::string v2)
{
    return v1.empty() ? v2 : v1;
}

std::wstring isNull(std::wstring v1, std::wstring v2)
{
    return v1.empty() ? v2 : v1;
}

CTString isNull(CTString v1, std::wstring v2)
{
    return v1.Length() == 0 ? CTString(v2) : v1;
}

bool isNull(CTString vl)                  { return vl.Length() == 0; }
bool isNull(void* vl)                     { return vl == nullptr; }
bool isNull(int txt)                      { return txt == 0; }
bool isNull(double txt)                   { return txt == 0.0; }
bool isNull(std::string txt)              { return txt.empty(); }
bool isNull(std::wstring txt)             { return txt.empty(); }
bool isNull(char* txt)                    { return txt == nullptr || *txt == '\0'; }
bool isNull(wchar_t* txt)                 { return txt == nullptr || *txt == L'\0'; }
#if !defined(_WIN32) && !defined(__BORLANDC__) && !defined(_MSC_VER)

//===========================================================================
// lstrcpy
//===========================================================================
int lstrcpy(char* dst, const char* src)
{
    if (!dst) return 0;
    if (!src) { dst[0] = '\0'; return 0; }
    std::size_t n = std::strlen(src);
    std::memcpy(dst, src, n);
    dst[n] = '\0';
    return static_cast<int>(n);
}

int lstrcpy(wchar_t* dst, const wchar_t* src)
{
    if (!dst) return 0;
    if (!src) { dst[0] = L'\0'; return 0; }
    std::size_t n = std::wcslen(src);
    std::wmemcpy(dst, src, n);
    dst[n] = L'\0';
    return static_cast<int>(n);
}

int lstrcpy(char* dst, const wchar_t* src)
{
    if (!dst) return 0;
    if (!src) { dst[0] = '\0'; return 0; }
    std::string tmp = wstring_to_utf8(src);
    std::memcpy(dst, tmp.c_str(), tmp.size());
    dst[tmp.size()] = '\0';
    return static_cast<int>(tmp.size());
}

int lstrcpy(wchar_t* dst, const char* src)
{
    if (!dst) return 0;
    if (!src) { dst[0] = L'\0'; return 0; }
    std::wstring tmp = utf8_to_wstring(src);
    std::wmemcpy(dst, tmp.c_str(), tmp.size());
    dst[tmp.size()] = L'\0';
    return static_cast<int>(tmp.size());
}

// ... (todo el resto del bloque: lstrcat, lstrcmpi, lstrcpy_s, lstrcat_s) ...

#endif // !_WIN32 (y compatibles)

//===========================================================================
// NumCmp — siempre disponible (no colisiona con Win32)
//===========================================================================
int NumCmp(const char* s1, const char* s2)
{
    if (s1 == s2) return 0;
    if (!s1) return -1;
    if (!s2) return  1;

    char* e1 = nullptr;
    char* e2 = nullptr;
    double d1 = std::strtod(s1, &e1);
    double d2 = std::strtod(s2, &e2);

    bool n1 = (e1 != s1) && (*e1 == '\0');
    bool n2 = (e2 != s2) && (*e2 == '\0');

    if (n1 && n2)
    {
        if (d1 < d2) return -1;
        if (d1 > d2) return  1;
        return 0;
    }
    return std::strcmp(s1, s2);
}

int NumCmp(const wchar_t* s1, const wchar_t* s2)
{
    if (s1 == s2) return 0;
    if (!s1) return -1;
    if (!s2) return  1;

    wchar_t* e1 = nullptr;
    wchar_t* e2 = nullptr;
    double d1 = std::wcstod(s1, &e1);
    double d2 = std::wcstod(s2, &e2);

    bool n1 = (e1 != s1) && (*e1 == L'\0');
    bool n2 = (e2 != s2) && (*e2 == L'\0');

    if (n1 && n2)
    {
        if (d1 < d2) return -1;
        if (d1 > d2) return  1;
        return 0;
    }
    return std::wcscmp(s1, s2);
}
//===========================================================================
// Variantes seguras
//===========================================================================
int lstrcpy_s(char* dst, std::size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) return -1;
    if (!src) { dst[0] = '\0'; return 0; }

    std::size_t n = std::strlen(src);
    if (n + 1 > dstSize)
    {
        std::memcpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = '\0';
        return -1;
    }
    std::memcpy(dst, src, n);
    dst[n] = '\0';
    return static_cast<int>(n);
}

int lstrcpy_s(wchar_t* dst, std::size_t dstSize, const wchar_t* src)
{
    if (!dst || dstSize == 0) return -1;
    if (!src) { dst[0] = L'\0'; return 0; }

    std::size_t n = std::wcslen(src);
    if (n + 1 > dstSize)
    {
        std::wmemcpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = L'\0';
        return -1;
    }
    std::wmemcpy(dst, src, n);
    dst[n] = L'\0';
    return static_cast<int>(n);
}

int lstrcat_s(char* dst, std::size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) return -1;
    std::size_t d = std::strlen(dst);
    if (d >= dstSize) { dst[dstSize - 1] = '\0'; return -1; }
    if (!src) return static_cast<int>(d);

    std::size_t s = std::strlen(src);
    if (d + s + 1 > dstSize)
    {
        std::size_t room = dstSize - d - 1;
        std::memcpy(dst + d, src, room);
        dst[dstSize - 1] = '\0';
        return -1;
    }
    std::memcpy(dst + d, src, s);
    dst[d + s] = '\0';
    return static_cast<int>(d + s);
}

int lstrcat_s(wchar_t* dst, std::size_t dstSize, const wchar_t* src)
{
    if (!dst || dstSize == 0) return -1;
    std::size_t d = std::wcslen(dst);
    if (d >= dstSize) { dst[dstSize - 1] = L'\0'; return -1; }
    if (!src) return static_cast<int>(d);

    std::size_t s = std::wcslen(src);
    if (d + s + 1 > dstSize)
    {
        std::size_t room = dstSize - d - 1;
        std::wmemcpy(dst + d, src, room);
        dst[dstSize - 1] = L'\0';
        return -1;
    }
    std::wmemcpy(dst + d, src, s);
    dst[d + s] = L'\0';
    return static_cast<int>(d + s);
}

//===========================================================================
// Timestamps
//===========================================================================
int datcpy(TIMESTAMP_STRUCT_TRC* h1, TIMESTAMP_STRUCT_TRC* h2)
{
    if (!h1 || !h2) return -1;
    *h1 = *h2;
    return 0;
}

TIMESTAMP_STRUCT_TRC GetDate()
{
    TIMESTAMP_STRUCT_TRC ts{};
    std::time_t t = std::time(nullptr);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    ts.year   = static_cast<std::int16_t>(lt.tm_year + 1900);
    ts.month  = static_cast<std::int16_t>(lt.tm_mon  + 1);
    ts.day    = static_cast<std::int16_t>(lt.tm_mday);
    ts.hour   = static_cast<std::int16_t>(lt.tm_hour);
    ts.minute = static_cast<std::int16_t>(lt.tm_min);
    ts.second = static_cast<std::int16_t>(lt.tm_sec);
    ts.fraction = 0;
    return ts;
}

int GetDate(TIMESTAMP_STRUCT_TRC* sub1)
{
    if (!sub1) return -1;
    *sub1 = GetDate();
    return 0;
}

namespace {

// Convierte TIMESTAMP_STRUCT_TRC a std::tm. Devuelve false si la fecha es inválida.
bool to_tm(const TIMESTAMP_STRUCT_TRC& t, std::tm& out)
{
    out = std::tm{};
    out.tm_year = t.year - 1900;
    out.tm_mon  = t.month - 1;
    out.tm_mday = t.day;
    out.tm_hour = t.hour;
    out.tm_min  = t.minute;
    out.tm_sec  = t.second;
    out.tm_isdst = -1;
    return true;
}

} // namespace

int DifHora(const TIMESTAMP_STRUCT_TRC& t1, const TIMESTAMP_STRUCT_TRC& t2)
{
    std::tm a{}, b{};
    to_tm(t1, a);
    to_tm(t2, b);
    std::time_t ta = std::mktime(&a);
    std::time_t tb = std::mktime(&b);
    if (ta == static_cast<std::time_t>(-1) ||
        tb == static_cast<std::time_t>(-1))
        return 0;
    double diff = std::difftime(tb, ta);
    // Devuelve segundos (positivo si t2 > t1).
    return static_cast<int>(diff);
}

int DifHora(const std::tm& t1, const std::tm& t2)
{
    std::tm a = t1;
    std::tm b = t2;
    std::time_t ta = std::mktime(&a);
    std::time_t tb = std::mktime(&b);
    if (ta == static_cast<std::time_t>(-1) ||
        tb == static_cast<std::time_t>(-1))
        return 0;
    return static_cast<int>(std::difftime(tb, ta));
}

double DifHora_Now(const TIMESTAMP_STRUCT_TRC& t)
{
    std::tm a{};
    to_tm(t, a);
    std::time_t ta = std::mktime(&a);
    std::time_t now = std::time(nullptr);
    if (ta == static_cast<std::time_t>(-1)) return 0.0;
    return std::difftime(now, ta);
}

std::string TimestampToISO8601(const TIMESTAMP_STRUCT_TRC& t)
{
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << t.year   << '-'
        << std::setw(2) << std::setfill('0') << t.month  << '-'
        << std::setw(2) << std::setfill('0') << t.day    << 'T'
        << std::setw(2) << std::setfill('0') << t.hour   << ':'
        << std::setw(2) << std::setfill('0') << t.minute << ':'
        << std::setw(2) << std::setfill('0') << t.second;
    if (t.fraction != 0)
        oss << '.' << std::setw(3) << std::setfill('0') << (t.fraction / 1000000);
    return oss.str();
}

//===========================================================================
// Formato numérico
//===========================================================================
double Round(double number, int decimals)
{
    if (decimals < 0) decimals = 0;
    double factor = std::pow(10.0, decimals);
    return std::round(number * factor) / factor;
}

std::string XTrans(double number, int length, int decimals)
{
    std::ostringstream oss;
    if (decimals > 0)
        oss << std::fixed << std::setprecision(decimals) << number;
    else
        oss << static_cast<long long>(number);

    std::string s = oss.str();
    if (length > 0 && static_cast<int>(s.size()) < length)
        s.insert(s.begin(), length - s.size(), ' ');
    return s;
}

std::string Trans(double number, int length, int decimals, std::string sufix)
{
    std::string s = XTrans(number, length, decimals);
    if (!sufix.empty())
        s += sufix;
    return s;
}

int ConvertirEnMinutos(double horaDecimal)
{
    // horaDecimal expresada como HH.MM (dos decimales = minutos).
    int hh = static_cast<int>(horaDecimal);
    int mm = static_cast<int>(std::round((horaDecimal - hh) * 100.0));
    return hh * 60 + mm;
}

double IncrementarSegundo(double horaDecimal, int inc, bool div)
{
    // horaDecimal como fracción de día (0.0 .. 1.0).
    const double secsPerDay = 86400.0;
    double segundos = horaDecimal * secsPerDay;
    segundos += div ? -inc : inc;
    return segundos / secsPerDay;
}

//===========================================================================
// Horas
//===========================================================================
double dtoh(double decimalHours)
{
    // decimalHours: horas en formato decimal (1.5 = 1h 30m)
    // devuelve fracción de día (0.0 .. 1.0)
    return decimalHours / 24.0;
}

double htod(double sub1)
{
    // inverso de dtoh
    return sub1 * 24.0;
}

double htod(TIMESTAMP_STRUCT_TRC su1, bool sec)
{
    double h = su1.hour + su1.minute / 60.0;
    if (sec)
        h += su1.second / 3600.0;
    return h;
}

CTString strhor(double hora, int dec)
{
    // hora decimal -> "HH:MM" o "HH:MM:SS"
    int hh = static_cast<int>(hora);
    int mm = static_cast<int>(std::round((hora - hh) * 60.0));
    if (mm == 60) { ++hh; mm = 0; }

    wchar_t buf[32];
    if (dec <= 0)
        std::swprintf(buf, 32, L"%02d:%02d", hh, mm);
    else
    {
        int ss = static_cast<int>(std::round(
                     (((hora - hh) * 60.0) - mm) * 60.0));
        if (ss == 60) { ++mm; ss = 0; }
        std::swprintf(buf, 32, L"%02d:%02d:%02d", hh, mm, ss);
    }
    return CTString(buf);
}

double val(const CTString& vl)
{
    // Acepta "1234.5", "1234,5", "12:34", "1.234,56", etc.
    std::wstring s = vl.c_str();
    // Si contiene ':' lo tratamos como hora HH:MM[:SS]
    if (s.find(L':') != std::wstring::npos)
    {
        int hh = 0, mm = 0, ss = 0;
        std::swscanf(s.c_str(), L"%d:%d:%d", &hh, &mm, &ss);
        return hh + mm / 60.0 + ss / 3600.0;
    }
    try { return std::stod(s); }
    catch (...) { return 0.0; }
}

std::string Strhor(TIMESTAMP_STRUCT_TRC su1, bool sec)
{
    char buf[32];
    if (sec)
        std::snprintf(buf, 32, "%02d:%02d:%02d",
                      su1.hour, su1.minute, su1.second);
    else
        std::snprintf(buf, 32, "%02d:%02d", su1.hour, su1.minute);
    return buf;
}

std::string Strhor(double su1, bool sec, bool isdecimal)
{
    if (isdecimal)
    {
        // su1 es fracción de día (0..1) -> segundos -> HH:MM:SS
        double totalSecs = su1 * 86400.0;
        int hh = static_cast<int>(totalSecs / 3600.0);
        int mm = static_cast<int>((totalSecs - hh * 3600.0) / 60.0);
        int ss = static_cast<int>(std::round(totalSecs - hh * 3600.0 - mm * 60.0));
        char buf[32];
        if (sec)
            std::snprintf(buf, 32, "%02d:%02d:%02d", hh, mm, ss);
        else
            std::snprintf(buf, 32, "%02d:%02d", hh, mm);
        return buf;
    }
    else
    {
        // su1 ya es hora decimal
        int hh = static_cast<int>(su1);
        int mm = static_cast<int>(std::round((su1 - hh) * 60.0));
        char buf[32];
        if (sec)
        {
            int ss = static_cast<int>(std::round(
                         (((su1 - hh) * 60.0) - mm) * 60.0));
            std::snprintf(buf, 32, "%02d:%02d:%02d", hh, mm, ss);
        }
        else
        {
            std::snprintf(buf, 32, "%02d:%02d", hh, mm);
        }
        return buf;
    }
}

std::string formatTimeSegons(int totalSeconds)
{
    int hh = totalSeconds / 3600;
    int mm = (totalSeconds % 3600) / 60;
    int ss = totalSeconds % 60;
    char buf[32];
    std::snprintf(buf, 32, "%02d:%02d:%02d", hh, mm, ss);
    return buf;
}

//===========================================================================
// Formato heredado que devuelve char* (buffer estático, no thread-safe)
//===========================================================================
namespace {
    char g_buf1[256];
    char g_buf2[256];
}

char* Bstr(double su1, int sub, int sub2)
{
    std::string s = XTrans(su1, sub, sub2);
    std::snprintf(g_buf1, sizeof(g_buf1), "%s", s.c_str());
    return g_buf1;
}

char* str(double su1, int sub, int sub2)
{
    std::string s = XTrans(su1, sub, sub2);
    std::snprintf(g_buf1, sizeof(g_buf1), "%s", s.c_str());
    return g_buf1;
}

char* trans(double su1, int sub, int sub2, int decminim, bool blanc)
{
    int dec = (decminim >= 0) ? decminim : sub2;
    std::string s = XTrans(su1, sub, dec);
    if (blanc && s.empty())
        s = " ";
    std::snprintf(g_buf2, sizeof(g_buf2), "%s", s.c_str());
    return g_buf2;
}

char* Xtrans(double su1, int sub, int sub2)
{
    std::string s = XTrans(su1, sub, sub2);
    std::snprintf(g_buf2, sizeof(g_buf2), "%s", s.c_str());
    return g_buf2;
}

CTString cat(CTString s1,
             CTString s2, CTString s3, CTString s4,
             CTString s5, CTString s6, CTString s7)
{
    CTString r = s1;
    r += s2; r += s3; r += s4;
    r += s5; r += s6; r += s7;
    return r;
}

//===========================================================================
// Sistema de ficheros (std::filesystem)
//===========================================================================
bool fileExists(const std::string& filename)
{
    std::error_code ec;
    return fs::exists(filename, ec) && fs::is_regular_file(filename, ec);
}

std::string extractFilePath(const std::string fullPath)
{
    return fs::path(fullPath).parent_path().string();
}

bool crearDirectorioSiNoExiste(const std::string& ruta)
{
    std::error_code ec;
    if (fs::exists(ruta, ec))
        return fs::is_directory(ruta, ec);
    return fs::create_directories(ruta, ec);
}

std::string getFileName(const std::string& ruta)
{
    return fs::path(ruta).filename().string();
}

//===========================================================================
// Conversiones
//===========================================================================
std::string sToU(const std::string input)
{
    // Devuelve el string con escapes \uXXXX para caracteres > 127.
    std::ostringstream oss;
    for (unsigned char c : input)
    {
        if (c < 127)
            oss << static_cast<char>(c);
        else
            oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<unsigned int>(c);
    }
    return oss.str();
}

std::wstring StringToWString(const std::string& str)
{
    return utf8_to_wstring(str.c_str());
}

//===========================================================================
// Color
//===========================================================================
std::string RGBtoHTML(int r, int g, int b)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                  r & 0xFF, g & 0xFF, b & 0xFF);
    return buf;
}

std::string RGBtoHTML(int rgb)
{
    return RGBtoHTML((rgb >> 16) & 0xFF,
                     (rgb >>  8) & 0xFF,
                      rgb        & 0xFF);
}





