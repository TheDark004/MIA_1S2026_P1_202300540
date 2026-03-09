#include "DiskManagement.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <algorithm>

namespace DiskManagement
{

    // Estado global en RAM (no persiste entre reinicios del backend)
    // mountedPartitions[] -> arreglo de particiones actualmente montadas
    // mountedCount        -> cuántas hay en el arreglo
    // globalCorrelative   -> número que sube con cada MOUNT (1, 2, 3...)

    static MountedPartition mountedPartitions[100];
    static int mountedCount = 0;
    static int globalCorrelative = 1;

    // MKDISK — Crea un disco virtual .mia
    // Pasos:
    //  1. Validar parámetros
    //  2. Calcular tamaño en bytes según -unit
    //  3. Crear archivo vacío
    //  4. Llenar con ceros (buffer de 1024 para eficiencia)
    //  5. Construir MBR y escribirlo en el byte 0

    std::string Mkdisk(int size, const std::string &fit,
                       const std::string &unit, const std::string &path)
    {
        std::ostringstream out;
        out << "======= MKDISK =======\n";

        // Normalizar a minúsculas para comparar sin importar cómo lo escribió el usuario
        std::string f = fit, u = unit;
        for (auto &c : f)
            c = tolower(c);
        for (auto &c : u)
            c = tolower(c);

        //  Validaciones
        if (size <= 0)
        {
            out << "Error: -size debe ser mayor a 0\n";
            return out.str();
        }
        if (f != "bf" && f != "ff" && f != "wf")
        {
            out << "Error: -fit debe ser 'bf', 'ff' o 'wf'\n";
            return out.str();
        }
        if (u != "k" && u != "m")
        {
            out << "Error: -unit debe ser 'k' o 'm'\n";
            return out.str();
        }
        if (path.empty())
        {
            out << "Error: -path es obligatorio\n";
            return out.str();
        }

        //  Convertir a bytes
        int sizeBytes = size;
        if (u == "k")
            sizeBytes *= 1024;
        else
            sizeBytes *= 1024 * 1024;

        //  Crear el archivo vacío
        if (!Utilities::CreateFile(path))
        {
            out << "Error: No se pudo crear el archivo en: " << path << "\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el archivo\n";
            return out.str();
        }

        //  Llenar con ceros
        // Usamos buffer de 1024 bytes → mucho más rápido que byte a byte.
        // Para un disco de 3MB: 3072 iteraciones vs 3.145.728 sin buffer.
        std::vector<char> zeros(1024, 0);
        int chunks = sizeBytes / 1024;
        int remainder = sizeBytes % 1024;

        for (int i = 0; i < chunks; i++)
        {
            file.seekp(i * 1024);
            file.write(zeros.data(), 1024);
        }
        if (remainder > 0)
        {
            file.seekp(chunks * 1024);
            file.write(zeros.data(), remainder);
        }

        // Construir y escribir el MBR
        MBR mbr{};
        mbr.MbrSize = sizeBytes;
        mbr.Signature = rand(); // número random para identificar el disco

        std::string date = Utilities::GetCurrentDateTime();
        std::memcpy(mbr.CreationDate, date.c_str(), 19);
        std::memcpy(mbr.Fit, f.c_str(), 2);

        // Inicializar las 4 ranuras como vacías
        for (int i = 0; i < 4; i++)
        {
            mbr.Partitions[i].Start = -1;
            mbr.Partitions[i].Size = -1;
            mbr.Partitions[i].Correlative = -1;
            mbr.Partitions[i].Status[0] = '0';
        }

        // Escribir MBR en el byte 0
        Utilities::WriteObject(file, mbr, 0);
        file.close();

        out << "Disco creado: " << path << "\n";
        out << "Tamaño: " << sizeBytes << " bytes\n";
        out << "Fit: " << f << "\n";
        out << "======================\n";
        return out.str();
    }

    // RMDISK — Elimina el archivo .mia del sistema
    std::string Rmdisk(const std::string &path)
    {
        std::ostringstream out;
        out << "======= RMDISK =======\n";

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        std::filesystem::remove(path);
        out << "Disco eliminado: " << path << "\n";
        out << "======================\n";
        return out.str();
    }

    // FDISK — Crea una partición dentro de un disco existente
    // Pasos:
    //  1. Validar parámetros
    //  2. Leer el MBR del disco
    //  3. Verificar restricciones (máx 4 particiones, 1 extendida)
    //  4. Calcular byte de inicio (justo después de la última partición)
    //  5. Llenar la ranura libre y escribir MBR actualizado

    std::string Fdisk(int size, const std::string &path, const std::string &name,
                      const std::string &type, const std::string &fit,
                      const std::string &unit)
    {
        std::ostringstream out;
        out << "======= FDISK =======\n";

        std::string t = type, f = fit, u = unit;
        for (auto &c : t)
            c = tolower(c);
        for (auto &c : f)
            c = tolower(c);
        for (auto &c : u)
            c = tolower(c);

        //  Validaciones
        if (size <= 0)
        {
            out << "Error: -size debe ser mayor a 0\n";
            return out.str();
        }
        if (t != "p" && t != "e" && t != "l")
        {
            out << "Error: -type debe ser 'p', 'e' o 'l'\n";
            return out.str();
        }
        if (f != "b" && f != "f" && f != "w")
        {
            out << "Error: -fit debe ser 'b', 'f' o 'w'\n";
            return out.str();
        }
        if (u != "b" && u != "k" && u != "m")
        {
            out << "Error: -unit debe ser 'b', 'k' o 'm'\n";
            return out.str();
        }
        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        //  Convertir a bytes
        int sizeBytes = size;
        if (u == "k")
            sizeBytes *= 1024;
        else if (u == "m")
            sizeBytes *= 1024 * 1024;
        // u == "b" → ya está en bytes

        //  Leer MBR
        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        //  Verificar restricciones
        int freeIndex = -1;    // índice de ranura libre en Partitions[4]
        int extendedCount = 0; // cuántas extendidas hay (máx 1)

        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start == -1)
            {
                // ranura vacía
                if (freeIndex == -1)
                    freeIndex = i;
            }
            else
            {
                if (mbr.Partitions[i].Type[0] == 'e')
                    extendedCount++;
            }
        }

        if (freeIndex == -1)
        {
            out << "Error: El disco ya tiene 4 particiones (máximo)\n";
            file.close();
            return out.str();
        }
        if (t == "e" && extendedCount >= 1)
        {
            out << "Error: Solo puede haber UNA partición extendida por disco\n";
            file.close();
            return out.str();
        }

        //  Calcular byte de inicio
        // La primera partición empieza justo después del MBR.
        // Las siguientes empiezan donde termina la anterior.
        //
        int nextStart = sizeof(MBR); // punto de partida = fin del MBR

        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                int endOfThis = mbr.Partitions[i].Start + mbr.Partitions[i].Size;
                if (endOfThis > nextStart)
                {
                    nextStart = endOfThis;
                }
            }
        }

        //  Verificar espacio disponible
        if (nextStart + sizeBytes > mbr.MbrSize)
        {
            out << "Error: No hay espacio suficiente en el disco\n";
            out << "  Disponible: " << (mbr.MbrSize - nextStart) << " bytes\n";
            out << "  Requerido:  " << sizeBytes << " bytes\n";
            file.close();
            return out.str();
        }

        //  Llenar la ranura libre
        Partition &p = mbr.Partitions[freeIndex];
        p.Status[0] = '0'; // sin montar todavía
        p.Type[0] = t[0];  // 'p', 'e' o 'l'
        p.Fit[0] = f[0];   // 'b', 'f' o 'w'
        p.Start = nextStart;
        p.Size = sizeBytes;
        p.Correlative = -1;

        // Copiar nombre (máx 15 chars + null terminator)
        std::memset(p.Name, 0, 16);
        std::memcpy(p.Name, name.c_str(), std::min(name.size(), (size_t)15));

        //  Escribir MBR actualizado
        Utilities::WriteObject(file, mbr, 0);
        file.close();

        out << "Partición creada: " << name << "\n";
        out << "Tipo: " << t << "  |  Fit: " << f << "\n";
        out << "Inicio: byte " << nextStart << "\n";
        out << "Tamaño: " << sizeBytes << " bytes\n";
        out << "=====================\n";
        return out.str();
    }

    // MOUNT — Monta una partición y le asigna un ID
    // El ID tiene formato: [2 dígitos carnet][correlativo][letra disco]
    // carnet=202012345, disco="DiscoA.mia"
    //   --> toma últimos 2 dígitos = "45"
    //   --> correlativo = 1
    //   --> letra = 'A'
    //   --> ID = "451A"

    std::string Mount(const std::string &path, const std::string &name)
    {
        std::ostringstream out;
        out << "======= MOUNT =======\n";

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        // Buscar partición por nombre
        int found = -1;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                std::string partName(mbr.Partitions[i].Name);
                if (partName == name)
                {
                    found = i;
                    break;
                }
            }
        }

        if (found == -1)
        {
            out << "Error: No existe la partición '" << name << "' en el disco\n";
            file.close();
            return out.str();
        }

        //  Generar ID
        // stem() extrae el nombre del archivo sin extensión
        // "/home/user/DiscoA.mia" -> stem = "DiscoA" -> last char = 'A'
        std::filesystem::path p(path);
        std::string diskName = p.stem().string();
        char diskLetter = diskName.back();

        std::string carnet = "40";

        int discCorrelative = 1;
        for (int i = 0; i < mountedCount; i++)
        {
            std::filesystem::path mp(mountedPartitions[i].path);
            if (mp.stem().string().back() == diskLetter)
            {
                discCorrelative++;
            }
        }

        std::string id = carnet + std::to_string(discCorrelative) + diskLetter;

        //  Actualizar partición en MBR
        mbr.Partitions[found].Status[0] = '1'; // montada
        mbr.Partitions[found].Correlative = discCorrelative;
        std::memset(mbr.Partitions[found].Id, 0, 4);
        std::memcpy(mbr.Partitions[found].Id, id.c_str(),
                    std::min(id.size(), (size_t)4));

        Utilities::WriteObject(file, mbr, 0);
        file.close();

        //  Guardar en arreglo RAM
        mountedPartitions[mountedCount].path = path;
        mountedPartitions[mountedCount].name = name;
        mountedPartitions[mountedCount].id = id;
        mountedPartitions[mountedCount].correlative = discCorrelative;
        mountedCount++;
        globalCorrelative++;

        out << "Partición montada: " << name << "\n";
        out << "ID asignado: " << id << "\n";
        out << "=====================\n";
        return out.str();
    }

    // MOUNTED — Lista todas las particiones montadas

    std::string Mounted()
    {
        std::ostringstream out;
        out << "====== MOUNTED ======\n";

        if (mountedCount == 0)
        {
            out << "No hay particiones montadas\n";
        }
        else
        {
            for (int i = 0; i < mountedCount; i++)
            {
                out << "  " << mountedPartitions[i].id
                    << "  |  " << mountedPartitions[i].name
                    << "  |  " << mountedPartitions[i].path << "\n";
            }
        }

        out << "=====================\n";
        return out.str();
    }

    // FindMountedById 
    // Busca una partición montada por su ID en el arreglo RAM

    int FindMountedById(const std::string &id, std::string &outPath)
    {
        for (int i = 0; i < mountedCount; i++)
        {
            if (mountedPartitions[i].id == id)
            {
                outPath = mountedPartitions[i].path;
                return i;
            }
        }
        return -1;
    }

}