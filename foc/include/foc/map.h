/* Tabla hash de cadenas (claves con longitud, sin copia). */
#ifndef FOC_MAP_H
#define FOC_MAP_H

#include "foc/foc.h"

typedef struct {
    const char *key;
    size_t key_len;
    void *value;
    bool used;
} FOCMapEntry;

typedef struct {
    FOCMapEntry *entries;
    size_t cap;
    size_t len;
} FOCMap;

void FOC_map_init(FOCMap *map);
void FOC_map_free(FOCMap *map);
void *FOC_map_get(const FOCMap *map, const char *key, size_t key_len);
void FOC_map_put(FOCMap *map, const char *key, size_t key_len, void *value);
void FOC_map_remove(FOCMap *map, const char *key, size_t key_len);
size_t FOC_map_count(const FOCMap *map);

#endif /* FOC_MAP_H */
