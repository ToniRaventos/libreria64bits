//---------------------------------------------------------------------------
// CTStringList.h — lista de cadenas con valor asociado y objeto opaco
//---------------------------------------------------------------------------
#ifndef CTLIB_CTSTRINGLIST_H
#define CTLIB_CTSTRINGLIST_H

#include "FCTString.h"
#include <cstddef>

class CTStringList
{
private:
    struct Node
    {
        CTString data;
        CTString Value;
        void*    Obj;
        Node*    next;

        explicit Node(const CTString& str)
            : data(str), Value(), Obj(nullptr), next(nullptr) {}

        Node(const Node&)            = delete;
        Node& operator=(const Node&) = delete;
    };

    Node* m_head;
    int   m_count;

protected:
    // Compatibilidad: se mantiene la firma, pero ya no se usa internamente.
    // Prefiere comparaciones directas con CTString::operator==.
    [[deprecated("Usar CTString::operator== en su lugar")]]
    bool Comparar(CharT* s1, CharT* s2);

public:
    CTStringList();
    ~CTStringList();

    // Patrón VCL heredado. Sólo válido sobre objetos creados con new.
    void Free();

    CTString operator[](int index) const;

    void Add(const CTString& str);
    void Add(const CTString& str, const CTString& valor);
    void Insert(int index, const CTString& str);
    void Delete(int index);
    int  Count() const;

    CTString Get(int index) const;
    CTString GetValue(int index) const;
    CTString GetValue() const;
    CTString GetCaption() const;

    void* GetObject();
    void* GetObj(int index);
    void  SetObject(void* obj);
    void  SetObject(int index, void* obj);

    int      Find(const CTString& str) const;
    CTString GetValue(const CTString& str) const;

    void Clear();

    // Divide `txt` usando `separador` como delimitador y añade cada trozo.
    // Si `charspecials` es false, se descartan los caracteres de control (< 32).
    void Add_Spliter(CTString txt, CTString separador, bool charspecials = false);

    // Carga pares clave/valor desde un JSON (requiere jsoncpp, ver #ifdef).
    // Devuelve el número de elementos cargados, o 0 si falla o no hay soporte.
    int Execute(const char* snapclient, int max = 0);

    // Declarada por compatibilidad con código antiguo.
    // Su implementación no existe en la versión portable: si la necesitas,
    // reimplementa sobre CTString::Find / Replace.
    // char* ExecuteChat(char* bf, int bytesReceived);
};

// Función de prueba histórica. Marcada como obsoleta: usar tests/test_ctstringlist.cpp.
[[deprecated("Usar los tests unitarios")]]
void ProvaString();

#endif // CTLIB_CTSTRINGLIST_H
