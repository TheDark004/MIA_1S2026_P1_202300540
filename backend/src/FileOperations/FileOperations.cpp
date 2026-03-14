#include "FileOperations.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace FileOperations {

// SplitPath — Divide un path en sus partes
//
// "/home/user/docs" -> ["home", "user", "docs"]
// "/home/" -> ["home"]
// "/" -> []

std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string token;
 
    while (std::getline(ss, token, '/')) {
        if (!token.empty()) {
            parts.push_back(token);
        }
    }
    return parts;
}

// FindInDir — Busca una entrada por nombre en un directorio
// Un directorio puede tener múltiples bloques (cuando tiene más de 4 entradas).
// Recorremos todos los bloques del inodo.
// Retorna el número de inodo de la entrada, o -1 si no existe.

int FindInDir(std::fstream& file, const SuperBloque& sb,
              int dirInodeNum, const std::string& name) {
 
    Inode dirInode{};
    FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);
 
    // Recorrer los 12 bloques directos del inodo
    for (int i = 0; i < 12; i++) {
        if (dirInode.i_block[i] == -1) break;
 
        // Leer el FolderBlock
        FolderBlock fb{};
        int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
        Utilities::ReadObject(file, fb, pos);
 
        // Revisar las 4 entradas del bloque
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
 
            std::string entryName(fb.b_content[j].b_name);
            if (entryName == name) {
                return fb.b_content[j].b_inodo;
            }
        }
    }
    return -1; // no encontrado
}

// AddEntryToDir — Agrega una entrada a un directorio
// Pasos:
// 1. Leer el inodo del directorio
// 2. Buscar un espacio vacío (b_inodo == -1) en sus bloques
// 3. Si no hay espacio -> asignar nuevo bloque al directorio
// 4. Escribir la entrada en el espacio encontrado
bool AddEntryToDir(std::fstream& file, SuperBloque& sb,
                   int partStart, int dirInodeNum,
                   const std::string& name, int newInodeNum) {

    Inode dirInode{};
    FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);

    // Buscar espacio vacío en bloques existentes 
    for (int i = 0; i < 12; i++) {
        if (dirInode.i_block[i] == -1) break;

        FolderBlock fb{};
        int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
        Utilities::ReadObject(file, fb, pos);

        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) {
                // espacio libre encontrado → escribir la entrada
                std::memset(fb.b_content[j].b_name, 0, 12);
                std::memcpy(fb.b_content[j].b_name, name.c_str(),
                            std::min(name.size(), (size_t)11));
                fb.b_content[j].b_inodo = newInodeNum;

                Utilities::WriteObject(file, fb, pos);

                // Actualizar tamaño del directorio
                dirInode.i_size += sizeof(FolderContent);
                std::string now = Utilities::GetCurrentDateTime();
                std::memcpy(dirInode.i_mtime, now.c_str(), 19);
                FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);

                // Guardar SuperBloque actualizado
                FileSystem::WriteSuperBloque(file, partStart, sb);
                return true;
            }
        }
    }

    //  No hay espacio → asignar nuevo bloque al directorio 
    // Buscar siguiente posición libre en i_block[]
    int nextBlockSlot = -1;
    for (int i = 0; i < 12; i++) {
        if (dirInode.i_block[i] == -1) {
            nextBlockSlot = i;
            break;
        }
    }

    if (nextBlockSlot == -1) {
        // Directorio lleno (más de 48 entradas) -> necesitaría indirecto
        return false;
    }

    // Asignar nuevo bloque
    int newBlockNum = FileSystem::AllocateBlock(file, sb);
    if (newBlockNum == -1) return false;

    // Inicializar el bloque nuevo con entradas vacías
    FolderBlock newFb{};
    for (int j = 0; j < 4; j++) {
        std::memset(newFb.b_content[j].b_name, 0, 12);
        newFb.b_content[j].b_inodo = -1;
    }

    // Agregar la nueva entrada en la primera posición
    std::memset(newFb.b_content[0].b_name, 0, 12);
    std::memcpy(newFb.b_content[0].b_name, name.c_str(),
                std::min(name.size(), (size_t)11));
    newFb.b_content[0].b_inodo = newInodeNum;

    // Escribir el bloque nuevo
    int pos = sb.s_block_start + newBlockNum * sizeof(FolderBlock);
    Utilities::WriteObject(file, newFb, pos);

    // Actualizar el inodo del directorio
    dirInode.i_block[nextBlockSlot] = newBlockNum;
    dirInode.i_size += sizeof(FolderContent);
    std::string now = Utilities::GetCurrentDateTime();
    std::memcpy(dirInode.i_mtime, now.c_str(), 19);
    FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);

    FileSystem::WriteSuperBloque(file, partStart, sb);
    return true;
}


// TraversePath — Navega el arbol y retorna el inodo del path
// Empieza siempre desde el inodo 0 (raíz) y va bajando.
//  path = ["home", "user"]
//  1. Busca "home" en inodo 0 → inodo 2
//  2. Busca "user" en inodo 2 → inodo 3
//  3. Retorna 3
int TraversePath(std::fstream& file, const SuperBloque& sb,
                 const std::vector<std::string>& parts) {
    int currentInode = 0; // empezar en la raíz

    for (const auto& part : parts) {
        int found = FindInDir(file, sb, currentInode, part);
        if (found == -1) return -1; // no existe
        currentInode = found;
    }
    return currentInode;
}


// TraverseToParent — Retorna el inodo del directorio PADRE
//   parts = ["home", "user", "docs"]
//   -> navega hasta ["home", "user"] y retorna ese inodo
//   -> "docs" es lo que se va a crear

int TraverseToParent(std::fstream& file, const SuperBloque& sb,
                     const std::vector<std::string>& parts) {
    if (parts.empty()) return -1;
    if (parts.size() == 1) return 0; // el padre es la raíz

    // Navegar hasta el penúltimo elemento
    std::vector<std::string> parentParts(parts.begin(), parts.end() - 1);
    return TraversePath(file, sb, parentParts);
}


// MKDIR — Crea un directorio en el sistema de archivos
// Parámetros:
//   path  -> ruta absoluta: "/home/user/documentos"
//   createParents -> si es true (-p), crea directorios intermedios
// Pasos:
//  1. Verificar sesión activa
//  2. Dividir el path en partes
//  3. Si -p: crear cada directorio intermedio que no exista
//  4. Si no -p: verificar que el padre existe
//  5. Crear el inodo (tipo carpeta)
//  6. Crear el FolderBlock con "." y ".."
//  7. Agregar la entrada al directorio padre
std::string Mkdir(const std::string& path, bool createParents) {
    std::ostringstream out;
    out << "======= MKDIR =======\n";

    // Verificar sesión activa
    if (!UserSession::currentSession.active) {
        out << "Error: No hay sesión activa. Usa LOGIN primero\n";
        return out.str();
    }

    auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
    if (!file.is_open()) {
        out << "Error: No se pudo abrir el disco\n";
        return out.str();
    }

    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

    std::vector<std::string> parts = SplitPath(path);
    if (parts.empty()) {
        out << "Error: Path inválido\n";
        file.close(); return out.str();
    }

    // ── Con -p: crear cada nivel que no exista ────────────────
    if (createParents) {
        int currentInode = 0; // empezar en raíz

        for (int i = 0; i < (int)parts.size(); i++) {
            int existsInode = FindInDir(file, sb, currentInode, parts[i]);

            if (existsInode != -1) {
                // Ya existe → simplemente avanzar
                currentInode = existsInode;
                continue;
            }

            // No existe → crear este directorio
            int newInodeNum = FileSystem::AllocateInode(file, sb);
            if (newInodeNum == -1) {
                out << "Error: No hay inodos disponibles\n";
                file.close(); return out.str();
            }

            int newBlockNum = FileSystem::AllocateBlock(file, sb);
            if (newBlockNum == -1) {
                out << "Error: No hay bloques disponibles\n";
                file.close(); return out.str();
            }

            // Crear FolderBlock con "." y ".."
            FolderBlock fb{};
            for (int j = 0; j < 4; j++) {
                std::memset(fb.b_content[j].b_name, 0, 12);
                fb.b_content[j].b_inodo = -1;
            }
            std::memcpy(fb.b_content[0].b_name, ".", 1);
            fb.b_content[0].b_inodo = newInodeNum;
            std::memcpy(fb.b_content[1].b_name, "..", 2);
            fb.b_content[1].b_inodo = currentInode;

            FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);

            // Crear el inodo del nuevo directorio
            std::string now = Utilities::GetCurrentDateTime();
            Inode newInode{};
            newInode.i_uid  = UserSession::currentSession.uid;
            newInode.i_gid  = UserSession::GetGid(
                                UserSession::currentSession.group,
                                UserSession::ReadUsersFile(file, sb));
            newInode.i_size = sizeof(FolderBlock);
            std::memcpy(newInode.i_atime, now.c_str(), 19);
            std::memcpy(newInode.i_ctime, now.c_str(), 19);
            std::memcpy(newInode.i_mtime, now.c_str(), 19);
            for (int j = 0; j < 15; j++) newInode.i_block[j] = -1;
            newInode.i_block[0] = newBlockNum;
            newInode.i_type[0]  = '0'; // carpeta
            std::memcpy(newInode.i_perm, "664", 3);

            FileSystem::WriteInode(file, sb, newInodeNum, newInode);

            // Agregar entrada al directorio padre
            AddEntryToDir(file, sb,
                          UserSession::currentSession.partStart,
                          currentInode, parts[i], newInodeNum);

            out << "Directorio creado: /";
            for (int j = 0; j <= i; j++) {
                out << parts[j];
                if (j < i) out << "/";
            }
            out << "\n";

            currentInode = newInodeNum;
        }

    } else {
        // ── Sin -p: el padre debe existir ────────────────────
        int parentInode = TraverseToParent(file, sb, parts);
        if (parentInode == -1) {
            out << "Error: El directorio padre no existe\n";
            out << "Usa -p para crear directorios intermedios\n";
            file.close(); return out.str();
        }

        std::string dirName = parts.back();

        // Verificar que no existe ya
        if (FindInDir(file, sb, parentInode, dirName) != -1) {
            out << "Error: Ya existe '" << dirName << "'\n";
            file.close(); return out.str();
        }

        // Crear inodo y bloque
        int newInodeNum = FileSystem::AllocateInode(file, sb);
        int newBlockNum = FileSystem::AllocateBlock(file, sb);

        if (newInodeNum == -1 || newBlockNum == -1) {
            out << "Error: No hay espacio disponible\n";
            file.close(); return out.str();
        }

        // FolderBlock con "." y ".."
        FolderBlock fb{};
        for (int j = 0; j < 4; j++) {
            std::memset(fb.b_content[j].b_name, 0, 12);
            fb.b_content[j].b_inodo = -1;
        }
        std::memcpy(fb.b_content[0].b_name, ".", 1);
        fb.b_content[0].b_inodo = newInodeNum;
        std::memcpy(fb.b_content[1].b_name, "..", 2);
        fb.b_content[1].b_inodo = parentInode;

        FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);

        std::string now = Utilities::GetCurrentDateTime();
        Inode newInode{};
        newInode.i_uid  = UserSession::currentSession.uid;
        newInode.i_gid  = UserSession::GetGid(
                            UserSession::currentSession.group,
                            UserSession::ReadUsersFile(file, sb));
        newInode.i_size = sizeof(FolderBlock);
        std::memcpy(newInode.i_atime, now.c_str(), 19);
        std::memcpy(newInode.i_ctime, now.c_str(), 19);
        std::memcpy(newInode.i_mtime, now.c_str(), 19);
        for (int j = 0; j < 15; j++) newInode.i_block[j] = -1;
        newInode.i_block[0] = newBlockNum;
        newInode.i_type[0]  = '0';
        std::memcpy(newInode.i_perm, "664", 3);

        FileSystem::WriteInode(file, sb, newInodeNum, newInode);
        AddEntryToDir(file, sb,
                      UserSession::currentSession.partStart,
                      parentInode, dirName, newInodeNum);

        out << "Directorio creado: " << path << "\n";
    }

    file.close();
    out << "=====================\n";
    return out.str();
}


// MKFILE — Crea un archivo en el sistema de archivos
// Parámetros:
//   path  -> ruta del archivo: "/home/user/archivo.txt"
//   random ->si true (-r), genera contenido aleatorio de 'size' bytes
//   size  -> tamaño del contenido aleatorio
//   cont  -> contenido directo del archivo (string)
// Si no se especifica -r ni -cont -> archivo vacío

std::string Mkfile(const std::string& path, bool random,
                   int size, const std::string& cont) {
    std::ostringstream out;
    out << "======= MKFILE =======\n";

    if (!UserSession::currentSession.active) {
        out << "Error: No hay sesión activa. Usa LOGIN primero\n";
        return out.str();
    }

    auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
    if (!file.is_open()) {
        out << "Error: No se pudo abrir el disco\n";
        return out.str();
    }

    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

    std::vector<std::string> parts = SplitPath(path);
    if (parts.empty()) {
        out << "Error: Path inválido\n";
        file.close(); return out.str();
    }

    // Verificar que el directorio padre existe
    int parentInode = TraverseToParent(file, sb, parts);
    if (parentInode == -1) {
        if (random && size == 0) {
            // crear carpetas padres automáticamente
            std::vector<std::string> parentParts(parts.begin(), parts.end() - 1);
            int currentInode = 0;
            for (int i = 0; i < (int)parentParts.size(); i++) {
                int existsInode = FindInDir(file, sb, currentInode, parentParts[i]);
                if (existsInode != -1) { currentInode = existsInode; continue; }
                int newInodeNum = FileSystem::AllocateInode(file, sb);
                int newBlockNum = FileSystem::AllocateBlock(file, sb);
                if (newInodeNum == -1 || newBlockNum == -1) {
                    out << "Error: No hay espacio para crear carpetas padres\n";
                    file.close(); return out.str();
                }
                FolderBlock fb{};
                for (int j = 0; j < 4; j++) {
                    std::memset(fb.b_content[j].b_name, 0, 12);
                    fb.b_content[j].b_inodo = -1;
                }
                std::memcpy(fb.b_content[0].b_name, ".", 1);
                fb.b_content[0].b_inodo = newInodeNum;
                std::memcpy(fb.b_content[1].b_name, "..", 2);
                fb.b_content[1].b_inodo = currentInode;
                FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);

                std::string now = Utilities::GetCurrentDateTime();
                Inode newInode{};
                newInode.i_uid = UserSession::currentSession.uid;
                newInode.i_gid = 1;
                newInode.i_size = sizeof(FolderBlock);
                std::memcpy(newInode.i_atime, now.c_str(), 19);
                std::memcpy(newInode.i_ctime, now.c_str(), 19);
                std::memcpy(newInode.i_mtime, now.c_str(), 19);
                for (int j = 0; j < 15; j++) newInode.i_block[j] = -1;
                newInode.i_block[0] = newBlockNum;
                newInode.i_type[0]  = '0';
                std::memcpy(newInode.i_perm, "664", 3);
                FileSystem::WriteInode(file, sb, newInodeNum, newInode);
                AddEntryToDir(file, sb, UserSession::currentSession.partStart,
                              currentInode, parentParts[i], newInodeNum);
                currentInode = newInodeNum;
            }
            parentInode = currentInode;
        } else {
            out << "Error: El directorio padre no existe\n";
            out << "Usa MKDIR -p para crear los directorios intermedios\n";
            file.close(); return out.str();
        }
    }

    std::string fileName = parts.back();

    // Verificar que no existe ya
    if (FindInDir(file, sb, parentInode, fileName) != -1) {
        out << "Error: Ya existe '" << fileName << "'\n";
        file.close(); return out.str();
    }

    // Validar size negativo
    if (size < 0) {
        out << "Error: -size no puede ser negativo\n";
        file.close(); return out.str();
    }

    // Preparar el contenido
    // -cont= puede ser ruta a archivo REAL del sistema Linux o contenido directo
    // -size= genera contenido aleatorio de 'size' bytes
    std::string content;

    if (!cont.empty()) {
        std::ifstream realFile(cont);
        if (realFile.is_open()) {
            content = std::string((std::istreambuf_iterator<char>(realFile)),
                                   std::istreambuf_iterator<char>());
            realFile.close();
        } else {
            content = cont;
        }
    } else if (size > 0) {
        std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789\n";
        for (int i = 0; i < size; i++) {
            content += chars[std::rand() % chars.size()];
        }
    }

    //  Crear el inodo del archivo 
    int newInodeNum = FileSystem::AllocateInode(file, sb);
    if (newInodeNum == -1) {
        out << "Error: No hay inodos disponibles\n";
        file.close(); return out.str();
    }

    std::string now = Utilities::GetCurrentDateTime();
    Inode newInode{};
    newInode.i_uid  = UserSession::currentSession.uid;
    newInode.i_gid  = UserSession::GetGid(
                        UserSession::currentSession.group,
                        UserSession::ReadUsersFile(file, sb));
    newInode.i_size = (int)content.size();
    std::memcpy(newInode.i_atime, now.c_str(), 19);
    std::memcpy(newInode.i_ctime, now.c_str(), 19);
    std::memcpy(newInode.i_mtime, now.c_str(), 19);
    for (int j = 0; j < 15; j++) newInode.i_block[j] = -1;
    newInode.i_type[0] = '1'; // archivo
    std::memcpy(newInode.i_perm, "664", 3);

    // Escribir el contenido en bloques 
    // Cada FileBlock tiene 64 bytes.
    // Si el contenido es mayor a 64 bytes, usamos múltiples bloques.
    if (!content.empty()) {
        int offset     = 0;
        int blockSlot  = 0; // índice en i_block[0..11]
        int totalBytes = (int)content.size();

        while (offset < totalBytes && blockSlot < 12) {
            int newBlockNum = FileSystem::AllocateBlock(file, sb);
            if (newBlockNum == -1) {
                out << "Error: No hay bloques disponibles\n";
                file.close(); return out.str();
            }

            FileBlock fb{};
            std::memset(fb.b_content, 0, 64);
            int toCopy = std::min(totalBytes - offset, (int)sizeof(FileBlock));
            std::memcpy(fb.b_content, content.c_str() + offset, toCopy);

            FileSystem::WriteFileBlock(file, sb, newBlockNum, fb);

            newInode.i_block[blockSlot] = newBlockNum;
            offset    += toCopy;
            blockSlot++;
        }
    }

    // Escribir el inodo
    FileSystem::WriteInode(file, sb, newInodeNum, newInode);

    // Agregar entrada al directorio padre
    AddEntryToDir(file, sb,
                  UserSession::currentSession.partStart,
                  parentInode, fileName, newInodeNum);

    file.close();

    out << "Archivo creado: " << path << "\n";
    out << "Tamaño: " << content.size() << " bytes\n";
    if (!content.empty() && !random) {
        out << "Contenido: " << content << "\n";
    }
    out << "======================\n";
    return out.str();
}

// CAT — Muestra el contenido de un archivo
// Pasos:
//  1. Navegar el árbol hasta encontrar el archivo
//  2. Leer el inodo y verificar que es tipo '1' (archivo)
//  3. Leer los bloques y concatenar el contenido

std::string Cat(const std::string& filePath) {
    std::ostringstream out;
    out << "======= CAT =======\n";

    if (!UserSession::currentSession.active) {
        out << "Error: No hay sesión activa\n";
        return out.str();
    }

    auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
    if (!file.is_open()) {
        out << "Error: No se pudo abrir el disco\n";
        return out.str();
    }

    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

    std::vector<std::string> parts = SplitPath(filePath);
    if (parts.empty()) {
        out << "Error: Path inválido\n";
        file.close(); return out.str();
    }

    // Navegar hasta el archivo
    int fileInodeNum = TraversePath(file, sb, parts);
    if (fileInodeNum == -1) {
        out << "Error: No existe '" << filePath << "'\n";
        file.close(); return out.str();
    }

    // Leer el inodo
    Inode fileInode{};
    FileSystem::ReadInode(file, sb, fileInodeNum, fileInode);

    // Verificar que es un archivo y no una carpeta
    if (fileInode.i_type[0] == '0') {
        out << "Error: '" << filePath << "' es un directorio, no un archivo\n";
        file.close(); return out.str();
    }

    // Leer el contenido bloque por bloque
    std::string content;
    int remaining = fileInode.i_size;

    for (int i = 0; i < 12 && remaining > 0; i++) {
        if (fileInode.i_block[i] == -1) break;

        FileBlock fb{};
        int pos = sb.s_block_start + fileInode.i_block[i] * sizeof(FolderBlock);
        Utilities::ReadObject(file, fb, pos);

        int toRead = std::min(remaining, (int)sizeof(FileBlock));
        content.append(fb.b_content, toRead);
        remaining -= toRead;
    }

    file.close();

    out << content << "\n";
    out << "===================\n";
    return out.str();
}

} 
