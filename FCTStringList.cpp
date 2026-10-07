//---------------------------------------------------------------------------
// CTStringList.cpp — implementación portable (sin Win32, sin dependencias
// obligatorias). JSON y logging son opcionales mediante macros.
//---------------------------------------------------------------------------
#include "FCTStringList.h"
#include "General.h"    // isNull(CTString), etc.

#include <iostream>
#include <sstream>
#include <string>

#ifdef CTLIB_WITH_JSONCPP
    #include <json/json.h>
#endif

#ifdef CTLIB_WITH_LOGGER
    #include "FCTLogger.h"
#endif

//---------------------------------------------------------------------------
// Helpers internos
//---------------------------------------------------------------------------
namespace {

void log_error(const std::string& msg)
{
#ifdef CTLIB_WITH_LOGGER
    Logger(msg, 1);
#else
    std::cerr << "[CTStringList] " << msg << std::endl;
#endif
}

} // namespace

//---------------------------------------------------------------------------
// Ciclo de vida
//---------------------------------------------------------------------------
CTStringList::CTStringList()
    : m_head(nullptr), m_count(0)
{
}

CTStringList::~CTStringList()
{
    Clear();
}

void CTStringList::Free()
{
    delete this;
}

//---------------------------------------------------------------------------
// Acceso
//---------------------------------------------------------------------------
CTString CTStringList::operator[](int index) const
{
    return Get(index);
}

int CTStringList::Count() const
{
    return m_count;
}

CTString CTStringList::Get(int index) const
{
    if (index < 0 || index >= m_count) return CTString();

    Node* current = m_head;
    for (int i = 0; i < index && current; ++i)
        current = current->next;

    return current ? current->data : CTString();
}

CTString CTStringList::GetValue(int index) const
{
    if (index < 0 || index >= m_count) return CTString();

    Node* current = m_head;
    for (int i = 0; i < index && current; ++i)
        current = current->next;

    return current ? current->Value : CTString();
}

CTString CTStringList::GetValue() const
{
    return m_head ? m_head->Value : CTString();
}

CTString CTStringList::GetCaption() const
{
    return m_head ? m_head->data : CTString();
}

//---------------------------------------------------------------------------
// Objetos asociados
//---------------------------------------------------------------------------
void* CTStringList::GetObject()
{
    return m_head ? m_head->Obj : nullptr;
}

void* CTStringList::GetObj(int index)
{
    if (index < 0 || index >= m_count) return nullptr;

    Node* current = m_head;
    for (int i = 0; i < index && current; ++i)
        current = current->next;

    return current ? current->Obj : nullptr;
}

void CTStringList::SetObject(void* obj)
{
    if (m_head) m_head->Obj = obj;
}

void CTStringList::SetObject(int index, void* obj)
{
    if (index < 0 || index >= m_count) return;

    Node* current = m_head;
    for (int i = 0; i < index && current; ++i)
        current = current->next;

    if (current) current->Obj = obj;
}

//---------------------------------------------------------------------------
// Inserción
//---------------------------------------------------------------------------
void CTStringList::Add(const CTString& str)
{
    Add(str, CTString());
}

void CTStringList::Add(const CTString& str, const CTString& valor)
{
    Node* newNode = new Node(str);
    newNode->Value = valor;

    if (m_head == nullptr)
    {
        m_head = newNode;
    }
    else
    {
        Node* current = m_head;
        while (current->next != nullptr)
            current = current->next;
        current->next = newNode;
    }
    ++m_count;
}

void CTStringList::Insert(int index, const CTString& str)
{
    if (index < 0 || index > m_count) return;

    Node* newNode = new Node(str);

    if (index == 0)
    {
        newNode->next = m_head;
        m_head        = newNode;
    }
    else
    {
        Node* current = m_head;
        for (int i = 0; i < index - 1 && current; ++i)
            current = current->next;

        if (current)
        {
            newNode->next = current->next;
            current->next = newNode;
        }
        else
        {
            delete newNode;
            return;
        }
    }
    ++m_count;
}

//---------------------------------------------------------------------------
// Borrado
//---------------------------------------------------------------------------
void CTStringList::Delete(int index)
{
    if (index < 0 || index >= m_count) return;

    if (index == 0)
    {
        Node* temp = m_head;
        m_head     = m_head->next;
        delete temp;
    }
    else
    {
        Node* current = m_head;
        for (int i = 0; i < index - 1 && current; ++i)
            current = current->next;

        if (!current || !current->next) return;

        Node* temp    = current->next;
        current->next = temp->next;
        delete temp;
    }
    --m_count;
}

void CTStringList::Clear()
{
    while (m_head != nullptr)
    {
        Node* temp = m_head;
        m_head     = m_head->next;
        delete temp;
    }
    m_count = 0;
}

//---------------------------------------------------------------------------
// Búsqueda
//---------------------------------------------------------------------------
int CTStringList::Find(const CTString& str) const
{
    CTString key = str.Upper();
    Node* current = m_head;
    int index = 0;
    while (current != nullptr)
    {
        if (current->data.Upper() == key)
            return index;
        current = current->next;
        ++index;
    }
    return -1;
}

CTString CTStringList::GetValue(const CTString& str) const
{
    CTString key = str.Upper();
    Node* current = m_head;
    while (current != nullptr)
    {
        if (current->data.Upper() == key)
            return current->Value;
        current = current->next;
    }
    return CTString();
}

//---------------------------------------------------------------------------
// Helper histórico (ya no se usa internamente)
//---------------------------------------------------------------------------
bool CTStringList::Comparar(CharT* s1, CharT* s2)
{
    if (!s1 || !s2) return false;
    while (*s2)
    {
        if (*s1 == 0) return false;
        if (*s1 != *s2) return false;
        ++s1; ++s2;
    }
    return true;
}

//---------------------------------------------------------------------------
// Add_Spliter — versión sin buffers manuales
//---------------------------------------------------------------------------
void CTStringList::Add_Spliter(CTString txt, CTString separador, bool charspecials)
{
    if (separador.Length() == 0)
    {
        Add(txt);
        return;
    }

    int start = 0;
    const int len = txt.Length();
    const int sepLen = separador.Length();

    for (int i = 0; i + sepLen <= len; )
    {
        // ¿Coincide el separador en la posición i?
        bool match = true;
        for (int k = 0; k < sepLen; ++k)
        {
            if (txt[i + k] != separador[k]) { match = false; break; }
        }

        if (match)
        {
            if (i > start)
                Add(txt.SubString(start, i - start));
            i     += sepLen;
            start  = i;
        }
        else
        {
            if (!charspecials && static_cast<unsigned>(txt[i]) < 32u)
            {
                // Carácter de control: se omite del fragmento actual.
                // Para simplificar, lo sustituimos por espacio en una copia.
                // (Otra opción: saltarlo y compactar; se hace abajo.)
                txt[i] = L' ';
            }
            ++i;
        }
    }

    if (start < len)
        Add(txt.SubString(start, len - start));
}

//---------------------------------------------------------------------------
// Execute — carga pares clave/valor desde JSON
//---------------------------------------------------------------------------
int CTStringList::Execute(const char* snapclient, int max)
{
    Clear();
    if (snapclient == nullptr || *snapclient == '\0')
        return 0;

#ifndef CTLIB_WITH_JSONCPP
    log_error("Execute() llamado sin soporte JSON (define CTLIB_WITH_JSONCPP).");
    return 0;
#else
    Json::Value root;
    Json::CharReaderBuilder builder;
    builder.settings_["emitUTF8"] = false;

    std::istringstream iss(snapclient);
    std::string parseError;

    if (!Json::parseFromStream(builder, iss, &root, &parseError))
    {
        log_error("Error al analizar JSON: " + parseError);
        return 0;
    }

    if (!root.isObject())
    {
        log_error("El JSON no es un objeto.");
        return 0;
    }

    for (const auto& field : root.getMemberNames())
    {
        const Json::Value& value = root[field];
        Add(CTString(field.c_str()), CTString(value.asString().c_str()));
        if (max && Count() >= max)
            break;
    }
    return Count();
#endif
}

//---------------------------------------------------------------------------
// ProvaString — movida al test unitario, se mantiene por compatibilidad
//---------------------------------------------------------------------------
void ProvaString()
{
    // Redirigida a std::cerr para no ensuciar stdout en tests.
    std::cerr << "[ProvaString] obsoleto. Usa tests/test_ctstringlist.cpp.\n";
}
