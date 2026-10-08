//---------------------------------------------------------------------------
// CTPath.cpp — implementación portable
//---------------------------------------------------------------------------
#include "CTPath.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

//===========================================================================
// FileExists
//===========================================================================
bool FileExists(const std::string& filename)
{
    // Usamos std::filesystem en lugar de std::ifstream.
    // Ventajas:
    //   - Funciona con rutas Unicode en Windows (std::ifstream con
    //     std::string no siempre lo hace).
    //   - No abre el fichero (más rápido, sin bloqueos).
    //   - Distingue entre "no existe" y "no se puede acceder" con error_code.
    std::error_code ec;
    return fs::exists(filename, ec);
}

//===========================================================================
// CTPath
//===========================================================================
CTPath::CTPath()
{
    // Nada que hacer: los miembros son std::string, ya inicializados vacíos.
}

CTPath::CTPath(const std::string& pathfilename)
{
    SetPath(pathfilename);
}

void CTPath::SetPath(const std::string& pathfilename)
{
    const fs::path p(pathfilename);

    pathFileName = p.string();

    // Ruta absoluta sin usar fs::absolute.
    fs::path absPath;
    if (p.is_absolute()) {
        absPath = p;
    } else {
        std::error_code ec;
        fs::path cwd = fs::current_path(ec);
        absPath = ec ? p : (cwd / p);
    }
    pathAbsolut = absPath.string();

    directory = p.parent_path().string();
    fileName  = p.filename().string();
}
