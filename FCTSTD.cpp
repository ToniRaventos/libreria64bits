//---------------------------------------------------------------------------
// CTSTD.cpp — implementación portable
//---------------------------------------------------------------------------
#include "FCTSTD.h"

//---------------------------------------------------------------------------
// Helper libre
//---------------------------------------------------------------------------
int GetTipoClase(void* data)
{
    if (data == nullptr) return 0;
    return static_cast<CTSTD*>(data)->TpClase;
}

//---------------------------------------------------------------------------
// Ciclo de vida
//---------------------------------------------------------------------------
CTSTD::CTSTD()
    : ObjDep(nullptr), TpClase(0)
{
}

CTSTD::~CTSTD()
{
    // ObjDep es propiedad del objeto: se libera aquí.
    delete ObjDep;
    ObjDep = nullptr;
}

//---------------------------------------------------------------------------
// Patrón VCL
//---------------------------------------------------------------------------
void CTSTD::Free()
{
    delete this;
}



//---------------------------------------------------------------------------
// Fábrica / destrucción
//---------------------------------------------------------------------------
CTSTD* CTSTD::Create()
{
    return new CTSTD();
}

CTSTD* CTSTD::NEW()
{
    return new CTSTD();
}

void CTSTD::Destroy(CTSTD* ptr)
{
    delete ptr;
}

//---------------------------------------------------------------------------
// Ownership del objeto dependiente
//---------------------------------------------------------------------------
CTSTD* CTSTD::SetObjDep(CTSTD* obj)
{
    if (ObjDep == obj)
        return ObjDep;          // idempotente, evita auto-borrado

    delete ObjDep;              // el anterior deja de ser nuestro
    ObjDep = obj;
    return ObjDep;
}

CTSTD* CTSTD::GetObjDep() const
{
    return ObjDep;
}
