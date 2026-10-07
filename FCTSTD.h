//---------------------------------------------------------------------------
// CTSTD.h — clase base mínima portable
//
// Diseño:
//   - Ningún miembro estático de plataforma.
//   - Ninguna dependencia de Win32.
//   - Copy/move prohibidos (evita dobles deletes por copia accidental).
//   - Sin operator new/delete personalizados (el allocator por defecto es
//     correcto y compatible con sanitizers).
//   - Free() sigue el patrón VCL: sólo se llama sobre objetos con new.
//
// Ownership:
//   - ObjDep es propiedad del objeto. El destructor lo libera.
//   - Para transferir un objeto dependiente ya existente, usa SetObjDep
//     y asume que a partir de ese momento el objeto base lo controla.
//---------------------------------------------------------------------------
#ifndef CTLIB_CTSTD_H
#define CTLIB_CTSTD_H

#include <string>

//---------------------------------------------------------------------------
// Helper libre
//---------------------------------------------------------------------------
int GetTipoClase(void* data);

//---------------------------------------------------------------------------
class CTSTD
{
private:
    CTSTD* ObjDep;

public:
    // Identificador de tipo. El usuario lo inicializa a un valor >= 0.
    // 0 = clase base sin identificar.
    int TpClase;

    CTSTD();
    virtual ~CTSTD();

    // Copy/move prohibidos: evita dobles deletes sobre ObjDep.
    CTSTD(const CTSTD&)            = delete;
    CTSTD& operator=(const CTSTD&) = delete;
    CTSTD(CTSTD&&)                 = delete;
    CTSTD& operator=(CTSTD&&)      = delete;

    // Patrón VCL: sólo válido sobre objetos creados con `new`.
    virtual void Free();

        // Fábrica / destrucción.
    static  CTSTD* Create();
    static  void   Destroy(CTSTD* ptr);
    virtual CTSTD* NEW();

    // Ownership del objeto dependiente.
    CTSTD* GetObjDep() const;
    CTSTD* SetObjDep(CTSTD* obj);
};

#endif // CTLIB_CTSTD_H
