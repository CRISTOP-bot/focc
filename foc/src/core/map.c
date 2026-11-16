#include "foc/map.h"

#include <stdlib.h>
#include <string.h>

static uint64_t hash_bytes(const char *data, size_t len) {
    /* FNV-1a de 64 bits. */
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (unsigned char)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

void FOC_map_init(FOCMap *map) {
    map->entries = NULL;
    map->cap = 0;
    map->len = 0;
}

void FOC_map_free(FOCMap *map) {
    free(map->entries);
    FOC_map_init(map);
}

static void map_grow(FOCMap *map) {
    size_t new_cap = map->cap ? map->cap * 2 : 32;
    FOCMapEntry *entries = (FOCMapEntry *)calloc(new_cap, sizeof(FOCMapEntry));
    if (!entries) {
        fprintf(stderr, "foc: agotamiento de memoria\n");
        abort();
    }
    for (size_t i = 0; i < map->cap; i++) {
        FOCMapEntry *old = &map->entries[i];
        if (!old->used) {
            continue;
        }
        size_t idx = (size_t)(hash_bytes(old->key, old->key_len) & (new_cap - 1));
        while (entries[idx].used) {
            idx = (idx + 1) & (new_cap - 1);
        }
        entries[idx] = *old;
    }
    free(map->entries);
    map->entries = entries;
    map->cap = new_cap;
}

void *FOC_map_get(const FOCMap *map, const char *key, size_t key_len) {
    if (!map->cap) {
        return NULL;
    }
    size_t idx = (size_t)(hash_bytes(key, key_len) & (map->cap - 1));
    while (map->entries[idx].used) {
        FOCMapEntry *entry = &map->entries[idx];
        if (entry->key_len == key_len && memcmp(entry->key, key, key_len) == 0) {
            return entry->value;
        }
        idx = (idx + 1) & (map->cap - 1);
    }
    return NULL;
}

void FOC_map_put(FOCMap *map, const char *key, size_t key_len, void *value) {
    if ((map->len + 1) * 4 >= map->cap * 3) {
        map_grow(map);
    }
    size_t idx = (size_t)(hash_bytes(key, key_len) & (map->cap - 1));
    while (map->entries[idx].used) {
        FOCMapEntry *entry = &map->entries[idx];
        if (entry->key_len == key_len && memcmp(entry->key, key, key_len) == 0) {
            entry->value = value;
            return;
        }
        idx = (idx + 1) & (map->cap - 1);
    }
    map->entries[idx].used = true;
    map->entries[idx].key = key;
    map->entries[idx].key_len = key_len;
    map->entries[idx].value = value;
    map->len++;
}

void FOC_map_remove(FOCMap *map, const char *key, size_t key_len) {
    if (!map->cap) {
        return;
    }
    size_t idx = (size_t)(hash_bytes(key, key_len) & (map->cap - 1));
    while (map->entries[idx].used) {
        FOCMapEntry *entry = &map->entries[idx];
        if (entry->key_len == key_len && memcmp(entry->key, key, key_len) == 0) {
            /* Rehash el resto de la cadena para no romper las busquedas. */
            map->entries[idx].used = false;
            map->entries[idx].key = NULL;
            map->entries[idx].value = NULL;
            map->len--;
            size_t probe = (idx + 1) & (map->cap - 1);
            while (map->entries[probe].used) {
                FOCMapEntry moved = map->entries[probe];
                map->entries[probe].used = false;
                map->entries[probe].key = NULL;
                map->entries[probe].value = NULL;
                map->len--;
                FOC_map_put(map, moved.key, moved.key_len, moved.value);
                probe = (probe + 1) & (map->cap - 1);
            }
            return;
        }
        idx = (idx + 1) & (map->cap - 1);
    }
}

size_t FOC_map_count(const FOCMap *map) { return map->len; }
