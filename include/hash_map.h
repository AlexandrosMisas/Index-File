#ifndef HASH_MAP_H
#define HASH_MAP_H

typedef struct hash_map *Hash_map;
typedef struct map_tuple *Map_tuple;


typedef size_t (*Hash_func)(void *a);
typedef int (*Comparator)(void *a, void *b);
typedef void (*Destructor)(void *a);

Hash_map hash_map_create(int capacity, Comparator compare_keys, 
                                       Hash_func hash_func,
                                       Destructor destroy_key,
                                       Destructor destroy_value);

void hash_map_insert(Hash_map map, void *key, void *value);

Map_tuple hash_map_value(Hash_map map, void *key);

void hash_map_delete(Hash_map map, void *key);

void *map_tuple_key(Map_tuple tuple);

void *map_tuple_value(Map_tuple tuple);

int hash_map_size(Hash_map map);

void hash_map_destroy(Hash_map map);

void hash_map_replace(Hash_map map, void *key, void *new_value);

#endif /* HASH_MAP_H */