#include "Analyzer.h"
#include "../DiskManagement/DiskManagement.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../FileOperations/FileOperations.h"
#include <regex>
#include <sstream>
#include <map>
#include <algorithm>

namespace Analyzer {


// Analyze — El corazón del sistema de comandos
//
// Recibe:  "mkdisk -size=3 -unit=m -fit=bf -path=/home/A.mia"
//
// Pasos:
//  1. Ignorar líneas vacías y comentarios (#)
//  2. Extraer el nombre del comando (primera palabra)
//  3. Usar REGEX para extraer los pares -key=value
//  4. Buscar el comando y llamar al módulo correcto

std::string Analyze(const std::string& input) {

    // Ignorar vacíos 
    if (input.empty()) return "";

    // Quitar espacios/tabs al inicio
    std::string line = input;
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    line = line.substr(start);

    // Comentarios (#) 
    // mostrar comentarios en el área de salida
    if (line[0] == '#') {
        return line + "\n";
    }

    // Extraer nombre del comando
    std::istringstream iss(line);
    std::string command;
    iss >> command;
    // Convertir a minúscula: "MKDISK" -> "mkdisk"
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    // Patrón: -(\w+)=("[^"]+"|\S+)
    //
    //   -         -> literal, el guión
    //   (\w+)     -> GRUPO 1: letras/dígitos/_ = nombre del param
    //   =         -> literal, el igual
    //   (         -> inicio GRUPO 2: el valor, que puede ser:
    //     "[^"]+" -> entre comillas: permite espacios adentro
    //     |       -> O
    //     \S+     -> sin comillas: cualquier cosa sin espacios
    //   )
    //
    //   -size=3 -> key="size", value="3"
    //   -path=/home/A.mia -> key="path",  value="/home/A.mia"
    //   -path="/mi disco/A.mia" -> key="path", value="/mi disco/A.mia"
    //   -fit=bf -> key="fit", value="bf"
    
    std::regex  re(R"(-(\w+)=("[^"]+"|\S+))");
    auto        it  = std::sregex_iterator(line.begin(), line.end(), re);
    auto        end = std::sregex_iterator();

    std::map<std::string, std::string> params;

    for (; it != end; ++it) {
        std::string key   = (*it)[1].str();
        std::string value = (*it)[2].str();

        // Quitar comillas del valor si las tiene
        // "/mi disco/A.mia" -> /mi disco/A.mia
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        // Keys siempre en minúscula para comparar de manera uniforme
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        params[key] = value;
    }

    // Disco

    if (command == "mkdisk") {
        if (!params.count("size") || !params.count("path"))
            return "Error MKDISK: Se requieren -size y -path\n";
        return DiskManagement::Mkdisk(
            std::stoi(params["size"]),
            params.count("fit")  ? params["fit"]  : "ff",
            params.count("unit") ? params["unit"] : "m",
            params["path"]);

    } else if (command == "rmdisk") {
        if (!params.count("path"))
            return "Error RMDISK: Se requiere -path\n";
        return DiskManagement::Rmdisk(params["path"]);

    } else if (command == "fdisk") {
        if (!params.count("size") || !params.count("path") || !params.count("name"))
            return "Error FDISK: Se requieren -size, -path y -name\n";
        return DiskManagement::Fdisk(
            std::stoi(params["size"]),
            params["path"],
            params["name"],
            params.count("type") ? params["type"] : "p",
            params.count("fit")  ? params["fit"]  : "f",
            params.count("unit") ? params["unit"] : "k");

    } else if (command == "mount") {
        if (!params.count("path") || !params.count("name"))
            return "Error MOUNT: Se requieren -path y -name\n";
        return DiskManagement::Mount(params["path"], params["name"]);

    } else if (command == "mounted") {
        return DiskManagement::Mounted();

    // FilesSystem

    } else if (command == "mkfs") {
        if (!params.count("id"))
            return "Error MKFS: Se requiere -id\n";
       return FileSystem::Mkfs(
            params["id"],
            params.count("type") ? params["type"] : "full");

    // SESIÓN 
    } else if (command == "login") {
        if (!params.count("user") || !params.count("pass") || !params.count("id"))
            return "Error LOGIN: Se requieren -user, -pass y -id\n";
        return UserSession::Login(params["user"], params["pass"], params["id"]);

    } else if (command == "logout") {
        return UserSession::Logout();

    } else if (command == "mkgrp") {
        if (!params.count("name"))
            return "Error MKGRP: falta -name\n";
        return UserSession::Mkgrp(params["name"]);

    } else if (command == "rmgrp") {
        if (!params.count("name"))
            return "Error RMGRP: falta -name\n";
        return UserSession::Rmgrp(params["name"]);

    } else if (command == "mkusr") {
        if (!params.count("user") || !params.count("pass") || !params.count("grp"))
            return "Error MKUSR: Se requieren -user, -pass y -grp\n";
        return UserSession::Mkusr(params["user"], params["pass"], params["grp"]);

    } else if (command == "rmusr") {
        if (!params.count("user"))
            return "Error RMUSR: falta -user\n";
        return UserSession::Rmusr(params["user"]);

    } else if (command == "chgrp") {
        if (!params.count("user") || !params.count("grp"))
            return "Error CHGRP: Se requieren -user y -grp\n";
        return UserSession::Chgrp(params["user"], params["grp"]);

    //Archivos    
    } else if (command == "mkdir") {
        if (!params.count("path")) return "Error MKDIR: falta -path\n";
        bool p = params.count("p") || line.find(" -p") != std::string::npos;
        return FileOperations::Mkdir(params["path"], p);

    } else if (command == "mkfile") {
        if (!params.count("path")) return "Error MKFILE: falta -path\n";
        bool random = params.count("r") || line.find(" -r") != std::string::npos;
        int  size   = params.count("size") ? std::stoi(params["size"]) : 0;
        std::string cont = params.count("cont") ? params["cont"] : "";
        return FileOperations::Mkfile(params["path"], random, size, cont);

    } else if (command == "cat") {
        if (!params.count("file")) return "Error CAT: falta -file\n";
        return FileOperations::Cat(params["file"]);

    } else {
        return "Error: Comando no reconocido -> '" + command + "'\n";
    }
}


// AnalyzeScript — Ejecuta múltiples comandos 
// Acumula todo el output en un solo string.
std::string AnalyzeScript(const std::string& script) {
    std::ostringstream total;
    std::istringstream stream(script);
    std::string line;

    while (std::getline(stream, line)) {
        std::string result = Analyze(line);
        if (!result.empty()) {
            total << result;
        }
    }
    return total.str();
}

} 