#ifndef FOC_FS_H
#define FOC_FS_H

#include "foc/foc.h"

/* Ruta absoluta de un fichero. */
char *FOC_fs_absolute(const char *path);
/* Une dos componentes de ruta. */
char *FOC_fs_join(const char *dir, const char *name);
/* Directorio de una ruta. */
char *FOC_fs_dirname(const char *path);
/* Nombre del fichero sin directorio ni extension. */
char *FOC_fs_basename(const char *path);
/* Sustituye la extension de un nombre. */
char *FOC_fs_replace_extension(const char *path, const char *extension);
/* Escribe un fichero, creando los directorios intermedios. */
bool FOC_fs_write_file(const char *path, const char *data, size_t len);
/* Lee un fichero completo. */
char *FOC_fs_read_file(const char *path, size_t *out_len);
/* Comprueba si existe. */
bool FOC_fs_exists(const char *path);
/* Comprueba si existe y es un fichero regular. */
bool FOC_fs_is_file(const char *path);
/* Comprueba si existe y es un directorio. */
bool FOC_fs_is_dir(const char *path);
/* Crea un directorio (y sus padres). */
bool FOC_fs_mkdir(const char *path);

#endif /* FOC_FS_H */
