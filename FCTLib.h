//---------------------------------------------------------------------------
// FCTLib.h — contenedor de datos con lista enlazada + índice + AVL + filtro
//
// Versión 2.0 — portable, 16/32/64 bits, sin Win32, sin UI.
//
// Ownership:
//   - Cada CTLib es dueño de sus nodos hijos (Posterior ... Ultim).
//   - Clear(true) libera todos los hijos recursivamente.
//   - No se puede copiar ni mover un CTLib (evita dobles deletes).
//
// Thread-safety:
//   - NO es thread-safe. Si necesitas concurrencia, protege con un mutex
//     externo o usa CTAVLTree (que sí lo es) para el índice secundario.
//---------------------------------------------------------------------------
#ifndef CTLIB_CTLIB_H
#define CTLIB_CTLIB_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "FCTSTD.h"
#include "FCTString.h"
#include "FCTAVLTree.h"
#include "General.h"
#ifdef GetClassName
    #undef GetClassName
#endif

//---------------------------------------------------------------------------
// Colores (0xRRGGBB)
//---------------------------------------------------------------------------
using CTColor = std::uint32_t;

#define TclBlack         0x000000u
#define TclWhite         0xFFFFFFu
#define TclRed           0xFF0000u
#define TclGreen         0x008000u
#define TclBlue          0x0000FFu
#define TclYellow        0xFFFF00u
#define TclCyan          0x00FFFFu
#define TclMagenta       0xFF00FFu
#define TclGray          0x808080u
#define TclDarkRed       0x8B0000u
#define TclDarkGreen     0x006400u
#define TclDarkBlue      0x00008Bu
#define TclOrange        0xFFA500u
#define TclPurple        0x800080u
#define TclPink          0xFFC0CBu
#define TclBrown         0xA52A2Au
#define TclLime          0x00FF00u

#define TclHighlighted   0xFFFFCCu
#define TclSelected      0xADD8E6u
#define TclHovered       0xE0E0E0u
#define TclDisabled      0xD3D3D3u

#define TclLightRed      0xFFC0C0u
#define TclLightGreen    0x90EE90u
#define TclLightOrange   0xFFDAB9u
#define TclDarkOrange    0xFF8C00u
#define TclLightPurple   0xE6E6FAu
#define TclDarkPurple    0x800080u

//---------------------------------------------------------------------------
// Mensajes de usuario (etiquetas neutras, sin depender de WM_USER).
//---------------------------------------------------------------------------
enum CTUserMsg : int
{
    CTMSG_ALTA    = 1,
    CTMSG_BAJA    = 2,
    CTMSG_MODIF   = 3,
    CTMSG_CONS    = 4,

    CTMSG_OBJCREA   = 10,
    CTMSG_OBJBAJA   = 11,
    CTMSG_OBJPROP   = 12,
    CTMSG_OBJBUSCAR = 13,
    CTMSG_OBJPRINT  = 14
};

//---------------------------------------------------------------------------
// Callbacks.
//   form   -> opaco, definido por el usuario (antes HWND)
//   filtro -> opaco, definido por el usuario (antes long*)
//---------------------------------------------------------------------------
using CTBaseFiltro = int (*)(void* form, void* item, void* filtro);
using CTBaseColor  = int (*)(void* item, void* userData);

//---------------------------------------------------------------------------
// Clase base CTLib
//---------------------------------------------------------------------------
class CTLib : public CTSTD
{
protected:
    bool          autoClearItems;
    int           SortIndex;

    // Matriz completa (todos los items) y filtrada (los que pasan el filtro).
    std::vector<CTLib*> Matrix;
    std::vector<CTLib*> FiltroMatrix;

    void*         FiltroParams;
    CTBaseFiltro  FuncFiltro;
    CTBaseColor   FunColor;
    void*         lpColors;

    int           CountRunOpc;
    int           MaxRunOpc;
    CTAVLTree*    TreeIndex;

    // --- Helpers internos ---
    CTLib* SortFuncPos(int pos) const;
    virtual int SortCmp(void* vl1, void* vl2);
    virtual int LocateCmp(void* vl1, void* vl2) const;

    void IndexMatrix();
    void RunFiltro();

    // Stub histórico. Reemplazado por std::sort en Sort(). Se conserva por
    // compatibilidad, pero ya no se llama.
    [[deprecated("Usar Sort(index). Este stub ya no hace nada.")]]
    int SortFunc(int start, int end_list);

public:
    enum TypeAttachMode { naAdd, naAddFirst, naInsert };

    // --- Datos públicos (compatibilidad) ---
    std::string   Version;
    void*         FrmHwnd;      // opaco, uso libre del usuario
    CTString      Clas;
    CTString      Ficha;
    void*         Object;
    std::intptr_t rec;
    int           Index;
    int           Count;
    int           CountGlobal;

    CTLib* Anterior;
    CTLib* Posterior;
    CTLib* Ultim;
    CTLib* Primer;

    CTLib*         Parent;
    TypeAttachMode AttachMode;
    CTLib*         ItemSelected;

    // --- Ciclo de vida ---
    CTLib(void* owner = nullptr, CTLib* parent = nullptr);
    ~CTLib() override;

    // Copy/move prohibidos
    CTLib(const CTLib&)            = delete;
    CTLib& operator=(const CTLib&) = delete;
    CTLib(CTLib&&)                 = delete;
    CTLib& operator=(CTLib&&)      = delete;

    // --- Fábrica ---
    CTLib* NEW() override;

    // --- Identificación ---

    // --- Patrón VCL (solo sobre objetos con new) ---
    void Free() override;
    virtual void Limpiar();

    void SetAutoClear(bool tp);

    // --- Ganchos de filtro/cálculo ---
    virtual void* StartFunFiltroAuto();
    virtual void  EndFunFiltroAuto(void* params);
    virtual bool  FunFiltroAuto(CTLib* item, void* params);
    virtual void  FuncCalcul(CTLib* item);
    virtual void  StartCalcul();
    virtual void  EndCalcul();

    // --- Navegación ---
    CTLib* Next()  const;
    CTLib* Prior() const;

    // --- Acceso a items ---
    CTLib* ItemGlobal(int pos) const;
    CTLib* Item(int pos) const;

    // --- Filtro ---
    void CreateFiltro(CTBaseFiltro Compare, void* param);
    void StopFiltro();

    // --- Colores ---
    int SetFunColor(CTBaseColor fn, void* userData);
    int RunGetColor(CTLib* BB);

    // --- Resultado ---
    virtual CTString GetReturnResult() const;

    // --- Gestión de la lista ---
    void         DeleteNode(CTLib* NN, bool borraDB = true, bool creaMatrix = true);
    virtual void InsertItem(int pos, CTLib* NN, bool creaMatrix = true);
    virtual void InsertNode  (CTLib* NN, bool createMatrix = true);
    virtual void InsertNode_B(CTLib* NN, bool creaMatrix   = true);

    virtual void AddNode   (CTLib* NN, bool createMatrix = true);
    virtual bool MoveItems (CTLib* st);
    virtual void MoveTo    (CTLib* NN, bool creaMatrix);
    virtual void LiberaItem(bool creaindex = true);
    virtual void LiberaAll (bool creaMatrix = true);

    virtual void Clear(bool autoclear = false, bool borraNode = true);
    void         Zap();
    void         Sort(int index);

    // Búsqueda binaria: sub1 se interpreta como const char* (UTF-8)
    // y se compara contra Ficha. Devuelve el nodo o nullptr.
    CTLib*       Locate(void* sub1) const;

    void CreateMatrix();
    void Reindex();

    // --- IO (ganchos de usuario; devuelven false por defecto) ---
    virtual bool Importar   (const std::wstring& name);
    virtual bool Exportar   (const std::wstring& name,
                             const std::wstring& area,
                             const std::wstring& nom);
    virtual bool Informacion(const std::wstring& name);

    // --- Texto y selección ---
    virtual CTString GetItemText(CTLib* BB) const;

    CTLib* GetItemSelected() const;
    CTLib* SetItemSelected(int pos);
    CTLib* SetItemSelected(CTLib* bf);

    // --- Permisos (gancho de usuario) ---
    virtual int GetPermis(bool& visible, bool& enabled);

    // --- Índice AVL ---
    int CreateIndex();
};

//---------------------------------------------------------------------------
// Clase derivada mínima
//---------------------------------------------------------------------------
class ClasTStandard : public CTLib
{
public:
    explicit ClasTStandard(void* owner = nullptr);
    ~ClasTStandard() override;

    CTLib*      NEW() override;

};

#endif // CTLIB_CTLIB_H
