//---------------------------------------------------------------------------
// FCTLib.cpp — implementación portable
//---------------------------------------------------------------------------
#include "FCTLib.h"

#include <algorithm>

//===========================================================================
// ClasTStandard
//===========================================================================
ClasTStandard::ClasTStandard(void* owner)
    : CTLib(owner, nullptr)
{
}

ClasTStandard::~ClasTStandard() = default;

CTLib* ClasTStandard::NEW()
{
    return new ClasTStandard(FrmHwnd);
}


//===========================================================================
// Ciclo de vida
//===========================================================================
CTLib::CTLib(void* owner, CTLib* parent)
    : CTSTD()
{
    FrmHwnd     = owner;
    Anterior    = nullptr;
    Posterior   = nullptr;
    Ultim       = nullptr;
    Primer      = nullptr;
    Parent      = parent;
    AttachMode  = naAdd;

    Limpiar();

    CountGlobal    = 0;
    Count          = 0;
    FiltroParams   = nullptr;
    FuncFiltro     = nullptr;
    FunColor       = nullptr;
    lpColors       = nullptr;
    SortIndex      = 0;
    Object         = nullptr;
    ItemSelected   = nullptr;
    TpClase        = 1;
    Version        = "2.00";
    autoClearItems = true;
    CountRunOpc    = 0;
    MaxRunOpc      = 0;
    TreeIndex      = nullptr;
}

CTLib::~CTLib()
{
    // La lista enlazada NO se libera aquí: depende de autoClearItems y de
    // si el usuario llamó a Clear(). Liberar en el destructor sin más
    // rompería el patrón "MoveItems": el receptor no puede recibir nodos
    // ya liberados por el emisor al destruirse.
    Matrix.clear();
    FiltroMatrix.clear();

    delete TreeIndex;
    TreeIndex = nullptr;

    FiltroParams = nullptr;
    FuncFiltro   = nullptr;
    Anterior     = nullptr;
    Posterior    = nullptr;
    Ultim        = nullptr;
    Primer       = nullptr;
}

//===========================================================================
// Identificación / fábrica
//===========================================================================
CTLib* CTLib::NEW()
{
    return new CTLib(FrmHwnd);
}


//===========================================================================
// VCL lifecycle
//===========================================================================
void CTLib::Free()
{
    Clear(true);
}

void CTLib::Limpiar()
{
    Ficha = L"";
}

CTString CTLib::GetReturnResult() const
{
    return Ficha;
}

//===========================================================================
// Movimiento de listas
//===========================================================================
bool CTLib::MoveItems(CTLib* st)
{
    if (st == nullptr) return false;

    Posterior    = st->Posterior;
    CountGlobal  = st->CountGlobal;
    Ultim        = st->Ultim;

    st->Ultim       = nullptr;
    st->CountGlobal = 0;
    st->Posterior   = nullptr;

    st->Reindex();
    Reindex();
    return true;
}

//===========================================================================
// Acceso a items
//===========================================================================
CTLib* CTLib::Item(int pos) const
{
    if (pos < 0 || pos >= static_cast<int>(FiltroMatrix.size()))
        return nullptr;
    return FiltroMatrix[static_cast<std::size_t>(pos)];
}

CTLib* CTLib::ItemGlobal(int pos) const
{
    if (pos < 0 || pos >= static_cast<int>(Matrix.size()))
        return nullptr;
    return Matrix[static_cast<std::size_t>(pos)];
}

void CTLib::SetAutoClear(bool tp)
{
    autoClearItems = tp;
}

//===========================================================================
// Vaciado / limpieza
//===========================================================================
void CTLib::Zap()
{
    StopFiltro();
    Posterior   = nullptr;
    Anterior    = nullptr;
    Ultim       = nullptr;
    CountGlobal = 0;
    CreateMatrix();
}

void CTLib::Clear(bool autoclear, bool borraNode)
{
    StopFiltro();

    ItemSelected = nullptr;

    if (autoClearItems && borraNode)
    {
        CTLib* AA = Posterior;
        while (AA != nullptr)
        {
            CTLib* BB = AA->Posterior;
            AA->Posterior = nullptr;
            AA->Ultim     = nullptr;
            if (AA != this) delete AA;
            AA = BB;
            if (CountGlobal > 0) --CountGlobal;
        }
    }

    Anterior    = nullptr;
    Posterior   = nullptr;
    Ultim       = nullptr;
    CountGlobal = 0;
    CreateMatrix();

    if (autoclear)
        delete this;   // patrón VCL: solo válido sobre objetos con new
}

//===========================================================================
// Borrado de nodos
//===========================================================================
void CTLib::DeleteNode(CTLib* NN, bool /*borraDB*/, bool creaMatrix)
{
    if (NN == nullptr) return;
    if (NN == this)
    {
        // No se permite auto-borrado por esta vía: usa Free().
        return;
    }

    CTLib* AA = NN->Anterior;
    CTLib* PP = NN->Posterior;

    // Si no tiene anterior, el enlace hacia atrás lo lleva el padre.
    if (AA == nullptr) AA = this;

    if (AA != nullptr) AA->Posterior = NN->Posterior;
    if (PP != nullptr) PP->Anterior  = NN->Anterior;

    if (CountGlobal > 0) --CountGlobal;

    // Si el nodo era el último, el padre deja de tener último.
    if (NN == Ultim)
        Ultim = NN->Anterior;

    // Si el nodo era el primer hijo real, el padre ya no tiene Posterior.
    if (Posterior == NN)
        Posterior = NN->Posterior;

    delete NN;

    if (creaMatrix)
        CreateMatrix();
}

//===========================================================================
// Inserción
//===========================================================================
void CTLib::AddNode(CTLib* NN, bool creaMatrix)
{
    if (NN == nullptr) return;

    NN->Parent = this;

    if (Ultim == nullptr)
    {
        NN->Anterior  = nullptr;   // es el primero
        NN->Posterior = nullptr;
        Posterior     = NN;
    }
    else
    {
        NN->Anterior     = Ultim;
        NN->Posterior    = nullptr;
        Ultim->Posterior = NN;
    }

    ++CountGlobal;
    Ultim = NN;

    if (creaMatrix)
        CreateMatrix();
}

void CTLib::InsertItem(int pos, CTLib* NN, bool creaMatrix)
{
    if (NN == nullptr) return;

    CTLib* pare = (Parent != nullptr) ? Parent : this;

    NN->LiberaItem(creaMatrix);
    NN->Parent = pare;

    CTLib* ini = nullptr;
    if (pos >= 0 && pos < pare->CountGlobal)
        ini = pare->Item(pos);

    if (ini == nullptr || pare->Ultim == ini)
        pare->AddNode(NN, creaMatrix);
    else if (pare->Posterior != ini)
        ini->InsertNode(NN, creaMatrix);
    else
        ini->InsertNode_B(NN, creaMatrix);
}

void CTLib::InsertNode(CTLib* NN, bool creaMatrix)
{
    if (NN == nullptr) return;

    NN->Parent = (Parent != nullptr) ? Parent : this;
    CTLib* pare = NN->Parent;

    NN->Anterior = Anterior;
    if (Anterior != nullptr)
        Anterior->Posterior = NN;

    NN->Posterior = this;
    Anterior      = NN;

    if (pare->Posterior == this)
        pare->Posterior = NN;

    ++pare->CountGlobal;

    if (creaMatrix)
        pare->CreateMatrix();
}

void CTLib::InsertNode_B(CTLib* NN, bool creaMatrix)
{
    if (NN == nullptr) return;

    NN->Parent = (Parent != nullptr) ? Parent : this;
    CTLib* pare = NN->Parent;

    NN->Posterior = Posterior;
    if (Posterior != nullptr)
        Posterior->Anterior = NN;

    NN->Anterior = this;
    Posterior    = NN;

    if (pare->Posterior == NN)
        pare->Posterior = this;

    ++pare->CountGlobal;

    if (creaMatrix)
        pare->CreateMatrix();
}

void CTLib::MoveTo(CTLib* NN, bool creaMatrix)
{
    if (NN == nullptr) return;

    NN->LiberaItem(creaMatrix);

    NN->Parent = (Parent != nullptr) ? Parent : this;

    switch (AttachMode)
    {
        case naAdd:
            NN->Parent->AddNode(NN, creaMatrix);
            break;

        case naInsert:
            InsertNode(NN, creaMatrix);
            break;

        case naAddFirst:
        default:
        {
            CTLib* ini = NN->Parent->Anterior;
            if (ini != nullptr)
                ini->InsertNode(NN, creaMatrix);
            else if (Parent != nullptr)
                Parent->AddNode(NN, creaMatrix);
            break;
        }
    }
}

//===========================================================================
// Desvinculación (no libera memoria, solo quita de la lista)
//===========================================================================
void CTLib::LiberaItem(bool creaindex)
{
    CTLib* pare = (Parent != nullptr) ? Parent : this;

    CTLib* a1 = Anterior;
    CTLib* p1 = Posterior;

    if (a1 != nullptr) a1->Posterior = p1;
    if (p1 != nullptr) p1->Anterior  = a1;

    if (pare->Posterior == this)
        pare->Posterior = (p1 == nullptr) ? a1 : p1;

    if (pare->Ultim == this)
        pare->Ultim = a1;

    // Limpiar los punteros del propio nodo para no dejar enlaces vivos.
    Anterior  = nullptr;
    Posterior = nullptr;

    if (pare->CountGlobal > 0)
    {
        --pare->CountGlobal;
        if (creaindex)
            pare->Reindex();
    }
}

void CTLib::LiberaAll(bool creaMatrix)
{
    // Recorrer la lista enlazada CRUDA (no la filtrada), porque el objetivo
    // es desvincular todos los nodos reales.
    CTLib* AA = Posterior;
    while (AA != nullptr)
    {
        CTLib* BB = AA->Posterior;
        AA->LiberaItem(false);
        AA = BB;
    }

    Anterior    = nullptr;
    Posterior   = nullptr;
    Ultim       = nullptr;
    CountGlobal = 0;

    if (creaMatrix)
        CreateMatrix();
}

//===========================================================================
// Navegación
//===========================================================================
CTLib* CTLib::Next()  const { return Posterior; }
CTLib* CTLib::Prior() const { return Anterior;  }

//===========================================================================
// Filtro
//===========================================================================
void CTLib::CreateFiltro(CTBaseFiltro Compare, void* params)
{
    FiltroParams = params;
    FuncFiltro   = Compare;
    RunFiltro();
}

void CTLib::StopFiltro()
{
    FiltroParams = nullptr;
    FuncFiltro   = nullptr;
    RunFiltro();
}

void CTLib::RunFiltro()
{
    Count = 0;

    StartCalcul();

    FiltroMatrix.clear();
    if (CountGlobal > 0)
        FiltroMatrix.reserve(static_cast<std::size_t>(CountGlobal));

    if (CountGlobal == 0)
    {
        EndCalcul();
        return;
    }

    void* FautoF = StartFunFiltroAuto();

    CTLib* AA = Posterior;
    while (AA != nullptr)
    {
        const bool okAuto = FunFiltroAuto(AA, FautoF);
        const bool okUser = (FuncFiltro == nullptr || FiltroParams == nullptr ||
                             FuncFiltro(FrmHwnd, AA, FiltroParams) == 1);

        if (okAuto && okUser)
        {
            FiltroMatrix.push_back(AA);
            FuncCalcul(AA);
        }
        AA = AA->Posterior;
    }

    EndCalcul();
    EndFunFiltroAuto(FautoF);

    Count = static_cast<int>(FiltroMatrix.size());
}

//===========================================================================
// Ganchos virtuales por defecto
//===========================================================================
void* CTLib::StartFunFiltroAuto()                        { return nullptr; }
bool  CTLib::FunFiltroAuto(CTLib* /*item*/, void* /*p*/) { return true; }
void  CTLib::EndFunFiltroAuto(void* /*params*/)          { }
void  CTLib::StartCalcul()                               { }
void  CTLib::FuncCalcul(CTLib* /*item*/)                 { }
void  CTLib::EndCalcul()                                 { }

void CTLib::Reindex()
{
    CreateMatrix();
}

//===========================================================================
// Matriz / índice
//===========================================================================
void CTLib::CreateMatrix()
{
    Matrix.clear();
    if (CountGlobal < 0) CountGlobal = 0;
    Matrix.reserve(static_cast<std::size_t>(CountGlobal));

    Ultim = nullptr;

    CTLib* AA = Posterior;
    int a = 0;
    while (AA != nullptr && a < CountGlobal)
    {
        Matrix.push_back(AA);
        AA->Index = a;
        Ultim     = AA;
        AA        = AA->Posterior;
        ++a;
    }

    // Corrige CountGlobal si la lista enlazada no coincidía con el contador.
    CountGlobal = static_cast<int>(Matrix.size());

    RunFiltro();
}

void CTLib::IndexMatrix()
{
    if (Matrix.empty())
    {
        Posterior = nullptr;
        Ultim     = nullptr;
        return;
    }

    CTLib* AA = this;
    for (CTLib* BB : Matrix)
    {
        AA->Posterior = BB;
        BB->Anterior  = AA;
        BB->Ultim     = nullptr;
        AA = BB;
    }
    AA->Posterior = nullptr;

    Ultim = (AA != this) ? AA : nullptr;
}

//===========================================================================
// Ordenación
//===========================================================================
void CTLib::Sort(int index)
{
    StopFiltro();
    SortIndex = index;

    std::sort(Matrix.begin(), Matrix.end(),
              [this](CTLib* a, CTLib* b) { return SortCmp(a, b) < 0; });

    IndexMatrix();
    CreateMatrix();
}

int CTLib::SortCmp(void* vl1, void* vl2)
{
    CTLib* s1 = static_cast<CTLib*>(vl1);
    CTLib* s2 = static_cast<CTLib*>(vl2);
    return s1->Ficha.compare(s2->Ficha);
}

CTLib* CTLib::SortFuncPos(int pos) const
{
    if (pos < 0 || pos >= static_cast<int>(Matrix.size())) return nullptr;
    return Matrix[static_cast<std::size_t>(pos)];
}

int CTLib::SortFunc(int /*iLo*/, int /*iHi*/)
{
    return 0;
}

//===========================================================================
// Búsqueda binaria
//===========================================================================
CTLib* CTLib::Locate(void* sub1) const
{
    // Búsqueda binaria sobre Matrix (ya ordenada por Sort()).
    // sub1 se interpreta como const char* (UTF-8) y se compara contra Ficha.
    int alt  = CountGlobal - 1;
    int baix = 0;
    while (baix <= alt)
    {
        const int mig = (alt + baix) / 2;
        const int cmp = LocateCmp(sub1, Matrix[static_cast<std::size_t>(mig)]);
        if (cmp == 0)  return Matrix[static_cast<std::size_t>(mig)];
        if (cmp == -1) alt  = mig - 1;
        else           baix = mig + 1;
    }
    return nullptr;
}

int CTLib::LocateCmp(void* vl1, void* vl2) const
{
    // vl1: cadena UTF-8 (const char*)
    // vl2: CTLib* contra el que comparamos su Ficha
    const CTLib* s2 = static_cast<const CTLib*>(vl2);
    CTString mj = static_cast<const char*>(vl1);
    return mj.compare(s2->Ficha);
}

//===========================================================================
// Colores
//===========================================================================
int CTLib::SetFunColor(CTBaseColor fn, void* userData)
{
    FunColor = fn;
    lpColors = userData;

    // Recorre solo los items filtrados (visibles). Documentado.
    for (int a = 0; a < Count; ++a)
    {
        CTLib* BB = Item(a);
        if (BB != nullptr && FunColor != nullptr)
            FunColor(BB, lpColors);
    }
    return 0;
}

int CTLib::RunGetColor(CTLib* BB)
{
    if (FunColor == nullptr) return 1;
    FunColor(BB, lpColors);
    return 0;
}

//===========================================================================
// Texto del item
//===========================================================================
CTString CTLib::GetItemText(CTLib* BB) const
{
    return BB->Ficha;
}

//===========================================================================
// Selección
//===========================================================================
CTLib* CTLib::GetItemSelected() const
{
    return ItemSelected;
}

CTLib* CTLib::SetItemSelected(int pos)
{
    if (pos < 0 || pos >= Count)
        ItemSelected = nullptr;
    else
        ItemSelected = Item(pos);
    return ItemSelected;
}

CTLib* CTLib::SetItemSelected(CTLib* bf)
{
    ItemSelected = bf;
    return ItemSelected;
}

//===========================================================================
// Permisos (gancho de usuario)
//===========================================================================
int CTLib::GetPermis(bool& /*visible*/, bool& /*enabled*/)
{
    return 0;
}

//===========================================================================
// IO (ganchos de usuario)
//===========================================================================
bool CTLib::Importar(const std::wstring& /*name*/)                   { return false; }
bool CTLib::Exportar(const std::wstring& /*name*/,
                     const std::wstring& /*area*/,
                     const std::wstring& /*nom*/)                    { return false; }
bool CTLib::Informacion(const std::wstring& /*name*/)                { return false; }

//===========================================================================
// Índice AVL
//===========================================================================
int CTLib::CreateIndex()
{
    if (TreeIndex == nullptr)
        TreeIndex = new CTAVLTree();

    TreeIndex->Clear();

    CTLib* AA = Posterior;
    int a = 0;
    while (AA != nullptr && a < CountGlobal)
    {
        // CTAVLTree trabaja con std::string.
        // u_str() devuelve la representación en escapes \uXXXX, que es
        // única y estable para cada Ficha y no requiere conversión UTF-8.
        TreeIndex->Add(AA->Ficha.u_str());
        AA = AA->Posterior;
        ++a;
    }
    return 0;
}
