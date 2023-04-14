#include "common.h"
#include "hash_map.h"
#include "dl_list.h"

#define LOAD_FACTOR 70
#define INITIAL_CAPACITY 32


static bool proper_capacity(int capacity) 
{
	return capacity >= INITIAL_CAPACITY 
		&& (capacity & (capacity - 1)) == 0;
}

struct hash_map {
	Dl_list *hash_table;
	Comparator compare_keys;
	Destructor destroy_key;
	Destructor destroy_value;
	Hash_func hash_func;
	int size;
	int capacity;
};

struct map_tuple {
	void *key;
	void *value;
};

static Dl_list_node hash_map_exists(Hash_map map, void *key);
static Map_tuple create_tuple(void *key, void *value);
static void hash_map_rehash(Hash_map map);
static void map_tuple_delete(Hash_map map, Map_tuple tuple);
static void delete_bucket(Hash_map map, Dl_list list);





Hash_map hash_map_create(int capacity, Comparator compare_keys, 
									   Hash_func hash_func,
									   Destructor destroy_key,
									   Destructor destroy_value) 
{
	Hash_map map       = calloc(1, sizeof(*map));

	map->capacity      = proper_capacity(capacity) ? capacity : INITIAL_CAPACITY;
	map->compare_keys  = compare_keys;
	map->hash_func     = hash_func;
	map->destroy_key   = destroy_key;
	map->destroy_value = destroy_value;
	map->hash_table    = calloc(map->capacity, sizeof(Dl_list));

	return map;

}



void hash_map_insert(Hash_map map, void *key, void *value) 
{
	size_t bucket = map->hash_func(key) % map->capacity;
	Dl_list_node node;

	if ((node = hash_map_exists(map, key)) != NULL) {
		Map_tuple tuple = list_value(node);
		if (map->destroy_key != NULL)
			map->destroy_key(tuple->key);

		if (map->destroy_value != NULL)
			map->destroy_value(tuple->value);

		tuple->key = key;
		tuple->value = value;
		return;
	}

	if (map->hash_table[bucket] == NULL)
		map->hash_table[bucket] = list_create(NULL);

	list_insert(map->hash_table[bucket], create_tuple(key, value));
	map->size++;

	if (100 * (map->size / map->capacity) > LOAD_FACTOR)
		hash_map_rehash(map);
}


Map_tuple hash_map_value(Hash_map map, void *key) 
{
	return map != NULL
		? list_value(hash_map_exists(map, key))
		: NULL;
}



void hash_map_delete(Hash_map map, void *key) 
{
	Dl_list_node node;
	size_t bucket = map->hash_func(key) % map->capacity;
	
	if ((node = hash_map_exists(map, key)) != NULL) {
		map_tuple_delete(map, list_value(node));
		list_delete(map->hash_table[bucket], node);
		map->size--;
	}
}

void *map_tuple_key(Map_tuple tuple) 
{
	return tuple != NULL ? tuple->key : NULL;
}

void *map_tuple_value(Map_tuple tuple) 
{
	return tuple != NULL ? tuple->value : NULL;
}


int hash_map_size(Hash_map map) 
{
	return map != NULL ? map->size : -1;
}



void hash_map_destroy(Hash_map map) 
{
	for (int i = 0; i < map->capacity; ++i)
		if (map->hash_table[i] != NULL)
			delete_bucket(map, map->hash_table[i]);

	free(map->hash_table);
	free(map);
}

void hash_map_replace(Hash_map map, void *key, void *new_value) 
{
	Map_tuple tuple;
	if ((tuple = hash_map_value(map, key)) != NULL) {
		if (map->destroy_value != NULL)
			map->destroy_value(tuple->value);
		tuple->value = new_value;
	}
}


static Dl_list_node hash_map_exists(Hash_map map, void *key) 
{
	size_t bucket = map->hash_func(key) % map->capacity;
	Dl_list_node node = list_first(map->hash_table[bucket]);

	while (node != NULL) {
		Map_tuple tuple = list_value(node);
		if (map->compare_keys(tuple->key, key) == 0)
			return node;
		
		node = list_next(node);
	}
	return NULL;
}


static void hash_map_rehash(Hash_map map) 
{
	int old_capacity = map->capacity;
	Dl_list *old_ht = map->hash_table;

	map->capacity *= 2;
	map->size = 0;
	map->hash_table = calloc(map->capacity, sizeof(Dl_list));

	for (int i = 0; i < old_capacity; ++i) {
		if (old_ht[i] == NULL)
			continue;
		
		Dl_list_node node = list_first(old_ht[i]);
		while (node != NULL) {
			Map_tuple tuple = list_value(node);
			hash_map_insert(map, tuple->key, tuple->value);
			free(tuple);
			node = list_next(node);
		}
		list_destroy(old_ht[i]);
	}
	free(old_ht);
}



static void delete_bucket(Hash_map map, Dl_list list) 
{
	while (list_size(list)) {
		map_tuple_delete(map, list_value(list_first(list)));
		list_delete(list, list_first(list));
	}
	free(list);
}


static void map_tuple_delete(Hash_map map, Map_tuple tuple) 
{
	if (map->destroy_key != NULL)
		map->destroy_key(tuple->key);

	if (map->destroy_value != NULL)
		map->destroy_value(tuple->value);
	free(tuple);
}


static Map_tuple create_tuple(void *key, void *value) 
{
	Map_tuple tuple = malloc(sizeof(*tuple));
	tuple->key = key;
	tuple->value = value;
	return tuple;
}

