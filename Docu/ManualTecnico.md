# Manual Técnico — ExtreamFS
### Simulador de Sistema de Archivos EXT2
**MIA 1S2026 | Carnet: 202300540**

---

## Tabla de Contenidos

1. [Descripción General](#descripción-general)
2. [Arquitectura del Sistema](#arquitectura-del-sistema)
3. [Estructuras de Datos](#estructuras-de-datos)
4. [Comandos Implementados](#comandos-implementados)
5. [Reportes](#reportes)

---

## 1. Descripción General

ExtreamFS es una aplicación web que simula el funcionamiento interno de un sistema de archivos **EXT2** sobre archivos binarios `.mia` que representan discos virtuales. Permite crear discos, particionarlos, formatearlos y manejar archivos y usuarios, todo desde una interfaz de terminal web.

---

## 2. Arquitectura del Sistema

![Arquitectura del Sistema](/Docu/arquitectura.png)

### Módulos del Backend

| Módulo | Archivo | Responsabilidad |
|---|---|---|
| Analyzer | `Analyzer.cpp` | Parsea comandos con regex, despacha a módulos |
| DiskManagement | `DiskManagement.cpp` | MKDISK, RMDISK, FDISK, MOUNT, MOUNTED |
| FileSystem | `FileSystem.cpp` | MKFS, gestión de inodos y bloques |
| UserSession | `UserSession.cpp` | LOGIN, LOGOUT, MKGRP, MKUSR, etc. |
| FileOperations | `FileOperations.cpp` | MKDIR, MKFILE, CAT |
| Reports | `Reports.cpp` | Generación de reportes con Graphviz |
| Utilities | `Utilities.cpp` | I/O binario, fechas |

### Comunicación Frontend ↔ Backend

```
Frontend                          Backend
   │                                 │
   │  POST /execute                  │
   │  {"commands": "mkdisk ..."}     │
   │ ──────────────────────────────► │
   │                                 │  Analyzer::AnalyzeScript()
   │                                 │  → módulo correcto
   │                                 │  → escribe en .mia
   │  {"output": "Disco creado..."}  │
   │ ◄────────────────────────────── │
   │                                 │
   │  GET /reports/%2Fhome%2F...jpg  │
   │ ──────────────────────────────► │
   │                                 │  Lee archivo .jpg del disco
   │  [imagen binaria]               │
   │ ◄────────────────────────────── │
```

---

## 3. Estructuras de Datos

Todas las estructuras usan `#pragma pack(push, 1)` para garantizar que no haya padding entre campos. Esto es crítico para la correcta lectura/escritura binaria en el `.mia`.

### 3.1 Partition (36 bytes)

Representa una ranura de partición dentro del MBR. El MBR tiene 4 de estas.

```
┌────────┬────────┬────────┬───────────┬──────────┬──────────────────┬─────────────┬────────┐
│Status  │ Type   │  Fit   │   Start   │   Size   │      Name        │ Correlative │   Id   │
│ 1 byte │ 1 byte │ 2 bytes│  4 bytes  │  4 bytes │    16 bytes      │   4 bytes   │ 4 bytes│
└────────┴────────┴────────┴───────────┴──────────┴──────────────────┴─────────────┴────────┘
```

| Campo | Descripción |
|---|---|
| `Status[1]` | `'0'` = sin montar, `'1'` = montada |
| `Type[1]` | `'p'` = Primaria, `'e'` = Extendida, `'l'` = Lógica |
| `Fit[2]` | `'bf'` = Best Fit, `'ff'` = First Fit, `'wf'` = Worst Fit |
| `Start` | Byte donde inicia la partición en el `.mia` |
| `Size` | Tamaño en bytes |
| `Name[16]` | Nombre de la partición |
| `Correlative` | Número de montaje (1, 2, 3...) |
| `Id[4]` | ID asignado al montar (ej: `"401A"`) |

![Estructura MBR](/Docu/mbr_layout.png)

### 3.2 MBR (173 bytes)

Master Boot Record. Siempre en el byte 0 del archivo `.mia`.

```
┌──────────┬───────────────┬───────────┬──────┬─────────────────────────────────┐
│ MbrSize  │ CreationDate  │ Signature │ Fit  │         Partitions[4]           │
│  4 bytes │   19 bytes    │  4 bytes  │2 bytes│          4 × 36 bytes          │
└──────────┴───────────────┴───────────┴──────┴─────────────────────────────────┘
```

### 3.3 EBR (Extended Boot Record)

Para particiones lógicas dentro de una extendida. Forman una lista enlazada via el campo `Next`.

```
┌────────┬──────┬───────────┬──────────┬──────────┬──────────────────┐
│ Mount  │ Fit  │   Start   │   Size   │   Next   │      Name        │
│ 1 byte │2 bytes│  4 bytes │  4 bytes │  4 bytes │    16 bytes      │
└────────┴──────┴───────────┴──────────┴──────────┴──────────────────┘
```

![Cadena de EBRs](/Docu/ebr_chain.png)

### 3.4 SuperBloque (68 bytes)

Primer estructura escrita por MKFS. Contiene toda la contabilidad del sistema EXT2.

```
Byte 0 de la partición:
┌─────────────────────────────────────────────────────────┐
│                      SuperBloque                        │
│  s_filesystem_type  │ s_inodes_count │ s_blocks_count  │
│  s_free_inodes_count│ s_free_blocks  │ s_mtime          │
│  s_umtime           │ s_mnt_count    │ s_magic (0xEF53) │
│  s_inode_size       │ s_block_size   │ s_first_ino      │
│  s_first_blo        │ s_bm_inode_start│ s_bm_block_start│
│  s_inode_start      │ s_block_start                     │
└─────────────────────────────────────────────────────────┘
```

### 3.5 Layout del disco después de MKFS

![Layout del Disco](/Docu/disk_layout.png)

Donde `n = partSize / (sizeof(SB) + 4 + 4×sizeof(Inode) + 3×64)`

### 3.6 Inode (133 bytes)

Representa un archivo o carpeta. No contiene el contenido, solo metadatos y punteros.

```
┌───────┬───────┬────────┬──────────┬──────────┬──────────┬────────────────┬────────┬────────┐
│ i_uid │ i_gid │ i_size │ i_atime  │ i_ctime  │ i_mtime  │   i_block[15]  │ i_type │ i_perm │
│4 bytes│4 bytes│4 bytes │ 19 bytes │ 19 bytes │ 19 bytes │   60 bytes     │ 1 byte │ 3 bytes│
└───────┴───────┴────────┴──────────┴──────────┴──────────┴────────────────┴────────┴────────┘
```

| Campo | Descripción |
|---|---|
| `i_uid` | UID del propietario |
| `i_gid` | GID del grupo |
| `i_size` | Tamaño en bytes |
| `i_atime/ctime/mtime` | Fechas en formato `"YYYY-MM-DD HH:MM:SS"` |
| `i_block[0..11]` | Punteros directos a bloques de datos |
| `i_block[12]` | Puntero indirecto simple |
| `i_block[13]` | Puntero indirecto doble |
| `i_block[14]` | Puntero indirecto triple |
| `i_type[1]` | `'0'` = carpeta, `'1'` = archivo |
| `i_perm[3]` | Permisos en formato octal `"664"` |

![Estructura Inode](/Docu/inode_struct.png)

### 3.7 Bloques (64 bytes cada uno)

Todos los bloques tienen el mismo tamaño (64 bytes). La posición del bloque N es:
```
pos = s_block_start + (N × 64)
```

**FolderBlock** — almacena hasta 4 entradas de directorio:
```
┌─────────────────────────────────────────────────────────┐
│  FolderContent[0]  │  FolderContent[1]  │  ...         │
│  b_name[12] + b_inodo(4)  × 4 entradas = 64 bytes      │
└─────────────────────────────────────────────────────────┘
```

**FileBlock** — almacena hasta 64 bytes de contenido de archivo:
```
┌─────────────────────────────────────────────────────────┐
│                    b_content[64]                        │
└─────────────────────────────────────────────────────────┘
```

**PointerBlock** — almacena hasta 16 punteros a otros bloques:
```
┌─────────────────────────────────────────────────────────┐
│  b_pointers[16]  (16 × 4 bytes = 64 bytes)             │
└─────────────────────────────────────────────────────────┘
```

### 3.8 users.txt

Archivo especial creado por MKFS en inodo 1. Formato CSV:
```
GID,G,nombre_grupo
UID,U,nombre_usuario,grupo,contraseña
```

Ejemplo:
```
1,G,root
1,U,root,root,123
2,G,devs
2,U,alice,devs,abc
```

![Formato users.txt](/Docu/users_txt.png)

- GID/UID = `0` indica que fue eliminado (RMGRP/RMUSR)

---

## 4. Comandos Implementados

### 4.1 MKDISK

Crea un disco virtual `.mia` de tamaño fijo.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-size` | Obligatorio | Tamaño del disco |
| `-path` | Obligatorio | Ruta absoluta del archivo `.mia` |
| `-unit` | Opcional | `k` = KB, `m` = MB (default: `m`) |
| `-fit` | Opcional | `bf`, `ff`, `wf` (default: `ff`) |

**Ejemplo:**
```
mkdisk -size=50 -unit=M -fit=FF -path=/home/user/Discos/Disco1.mia
```

**Efecto:** Crea el archivo `.mia` llenándolo con ceros y escribe el MBR en el byte 0.

---

### 4.2 RMDISK

Elimina un disco virtual del sistema.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta del `.mia` a eliminar |

**Ejemplo:**
```
rmdisk -path=/home/user/Discos/Disco1.mia
```

---

### 4.3 FDISK

Crea una partición dentro de un disco existente.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-size` | Obligatorio | Tamaño de la partición |
| `-path` | Obligatorio | Ruta del disco |
| `-name` | Obligatorio | Nombre de la partición |
| `-type` | Opcional | `P` = Primaria, `E` = Extendida, `L` = Lógica (default: `P`) |
| `-fit` | Opcional | `BF`, `FF`, `WF` (default: `FF`) |
| `-unit` | Opcional | `b`, `k`, `m` (default: `k`) |

**Ejemplos:**
```
# Partición primaria de 10MB
fdisk -type=P -unit=M -name=Part1 -size=10 -path=/home/user/Disco1.mia -fit=BF

# Partición extendida de 5MB
fdisk -type=E -unit=k -name=PartExt -size=5120 -path=/home/user/Disco1.mia

# Partición lógica dentro de la extendida
fdisk -type=L -unit=k -name=PartLog -size=1024 -path=/home/user/Disco1.mia
```

**Restricciones:**
- Máximo 4 particiones en el MBR
- Solo puede haber 1 partición extendida por disco
- Las particiones lógicas se crean como EBR dentro de la extendida

---

### 4.4 MOUNT

Monta una partición y le asigna un ID único.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta del disco |
| `-name` | Obligatorio | Nombre de la partición |

**Ejemplo:**
```
mount -path=/home/user/Disco1.mia -name=Part1
```

**ID generado:** `[2 dígitos carnet][correlativo][letra disco]`
- Ejemplo: carnet `202300540`, primer disco → `401A`, segunda partición → `402A`

---

### 4.5 MOUNTED

Lista todas las particiones actualmente montadas en RAM.

**Ejemplo:**
```
mounted
```

**Salida:**
```
401A  |  Part11  |  /home/user/Disco1.mia
402A  |  Part12  |  /home/user/Disco1.mia
401B  |  Part31  |  /home/user/Disco3.mia
```

---

### 4.6 MKFS

Formatea una partición montada como EXT2.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-id` | Obligatorio | ID de la partición montada |
| `-type` | Opcional | `full` (default: `full`) |

**Ejemplo:**
```
mkfs -type=full -id=401A
```

**Efecto:**
1. Calcula n (número de inodos)
2. Escribe el SuperBloque
3. Inicializa bitmaps de inodos y bloques
4. Crea el directorio raíz `/` (inodo 0, bloque 0)
5. Crea `users.txt` (inodo 1, bloque 1) con usuario root

---

### 4.7 LOGIN

Inicia sesión en el sistema de archivos.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-user` | Obligatorio | Nombre de usuario |
| `-pass` | Obligatorio | Contraseña |
| `-id` | Obligatorio | ID de la partición |

**Ejemplo:**
```
login -user=root -pass=123 -id=401A
```

**Restricción:** Solo puede haber una sesión activa a la vez.

---

### 4.8 LOGOUT

Cierra la sesión activa.

**Ejemplo:**
```
logout
```

---

### 4.9 MKGRP

Crea un nuevo grupo de usuarios. Solo puede ejecutarlo `root`.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-name` | Obligatorio | Nombre del grupo |

**Ejemplo:**
```
mkgrp -name=devs
```

---

### 4.10 RMGRP

Elimina un grupo (marca su GID como 0). Solo `root`.

**Ejemplo:**
```
rmgrp -name=devs
```

---

### 4.11 MKUSR

Crea un nuevo usuario. Solo `root`.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-user` | Obligatorio | Nombre del usuario |
| `-pass` | Obligatorio | Contraseña |
| `-grp` | Obligatorio | Grupo al que pertenece |

**Ejemplo:**
```
mkusr -user=alice -pass=abc -grp=devs
```

---

### 4.12 RMUSR

Elimina un usuario (marca su UID como 0). Solo `root`.

**Ejemplo:**
```
rmusr -user=alice
```

---

### 4.13 CHGRP

Cambia el grupo de un usuario. Solo `root`.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-user` | Obligatorio | Nombre del usuario |
| `-grp` | Obligatorio | Nuevo grupo |

**Ejemplo:**
```
chgrp -user=alice -grp=admin
```

---

### 4.14 MKDIR

Crea un directorio en el sistema de archivos virtual.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta del directorio a crear |
| `-p` | Opcional | Crea directorios padres si no existen |

**Ejemplos:**
```
# Crear /bin (el padre / ya debe existir)
mkdir -path=/bin

# Crear árbol completo automáticamente
mkdir -p -path=/home/archivos/user/docs/usac
```

**Efecto interno:**
1. Asigna un inodo nuevo (tipo carpeta)
2. Asigna un bloque nuevo con entradas `.` y `..`
3. Agrega la entrada al directorio padre

---

### 4.15 MKFILE

Crea un archivo en el sistema de archivos virtual.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-path` | Obligatorio | Ruta del archivo |
| `-size` | Opcional | Genera contenido aleatorio de N bytes |
| `-cont` | Opcional | Ruta a archivo real del sistema Linux para copiar su contenido |
| `-r` | Opcional | Crea directorios padres si no existen |

**Ejemplos:**
```
# Archivo con 75 bytes aleatorios
mkfile -path=/home/user/Tarea.txt -size=75

# Archivo con contenido de un archivo real
mkfile -path=/home/user/Tarea3.txt -cont=/home/thedark004/NAME.txt

# Archivo recursivo (crea carpetas padres)
mkfile -r -path=/home/user/docs/proyectos/fase1/entrada.txt
```

---

### 4.16 CAT

Muestra el contenido de un archivo del sistema de archivos virtual.

**Parámetros:**

| Parámetro | Tipo | Descripción |
|---|---|---|
| `-file` o `-file1` | Obligatorio | Ruta del archivo |

**Ejemplo:**
```
cat -file1=/home/user/Tarea.txt
cat -file=/users.txt
```

---

## 5. Reportes

Todos los reportes se generan con el comando `rep`:

```
rep -id=<ID> -path=<ruta_salida> -name=<tipo> [-path_file_ls=<ruta>]
```

| Reporte | Descripción | Requiere `-path_file_ls` |
|---|---|---|
| `mbr` | Estructura del MBR con sus particiones y EBRs | No |
| `disk` | Diagrama proporcional del disco | No |
| `sb` | Campos del SuperBloque | No |
| `inode` | Todos los inodos usados con sus bloques | No |
| `block` | Todos los bloques usados con su contenido | No |
| `bm_inode` | Bitmap de inodos en texto plano | No |
| `bm_block` | Bitmap de bloques en texto plano | No |
| `tree` | Árbol completo del sistema de archivos | No |
| `file` | Contenido de un archivo específico | Sí (ruta del archivo) |
| `ls` | Lista de archivos de un directorio | Sí (ruta del directorio) |

**Ejemplos:**
```
rep -id=401A -path=/home/user/Reportes/mbr.jpg -name=mbr
rep -id=401A -path=/home/user/Reportes/tree.png -name=tree
rep -id=401A -path=/home/user/Reportes/bm_inode.txt -name=bm_inode
rep -id=401A -path=/home/user/Reportes/file.jpg -path_file_ls=/home/archivos/Tarea.txt -name=file
rep -id=401A -path=/home/user/Reportes/ls.jpg -path_file_ls=/home/archivos/user/docs -name=ls
```

Los reportes de imagen (`.jpg`, `.png`) se generan con **Graphviz** y se visualizan automáticamente en la galería del frontend. Los reportes de texto (`.txt`) se muestran en el visor de texto de la galería.

---

*Manual Técnico — ExtreamFS | MIA 1S2026 | Universidad de San Carlos de Guatemala*
