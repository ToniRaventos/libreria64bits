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
    std::error_code ec;

    const fs::path p(pathfilename);

    // Ruta tal cual se pasó.
    pathFileName = p.string();

    // Ruta absoluta normalizada. Si falla, dejamos la original.
    fs::path absPath = fs::absolute(p, ec);
    pathAbsolut = ec ? p.string() : absPath.string();

    // Directorio contenedor.
    directory = p.parent_path().string();

    // Nombre del fichero (sin directorio).
    fileName = p.filename().string();
}
