//---------------------------------------------------------------------------
// CTPath.h — gestión de rutas portátil (std::filesystem)
//---------------------------------------------------------------------------
#ifndef CTPATH_H
#define CTPATH_H

#include <string>

//---------------------------------------------------------------------------
// Comprueba si un fichero existe. No confundir con "es un fichero regular":
// devuelve true para directorios, enlaces simbólicos, etc.
//
// Si necesitas comprobar que es un fichero regular, usa std::filesystem
// directamente:
//     std::filesystem::is_regular_file(path)
//---------------------------------------------------------------------------
bool FileExists(const std::string& filename);

//---------------------------------------------------------------------------
// CTPath
//---------------------------------------------------------------------------
class CTPath
{
public:
    std::string pathFileName;   // ruta tal como se pasó
    std::string pathAbsolut;    // ruta absoluta normalizada
    std::string directory;      // directorio contenedor
    std::string fileName;       // nombre del fichero (sin directorio)

    CTPath();
    explicit CTPath(const std::string& pathfilename);

    void SetPath(const std::string& pathfilename);
};

#endif // CTPATH_H
