#ifndef HASH_FILE_H
#define HASH_FILE_H

#include "common.h"
#include "hash_map.h"
#include "dl_list.h"
#include "record.h"

#define MAX_FILENAME 50


extern Hash_map file_map;


typedef struct {
    char filename[MAX_FILENAME + 1];
    rec_attr attr;
} Index_info;

typedef struct {
    char file_type[5];
    char filename[MAX_FILENAME + 1];
    int file_desc;
    int rec_capacity;
    int rec_count;
    int buckets;
    int last_block_id;
    rec_attr attr;
    Index_info index_files[INDEX_ATTR];
    int *hash_table;
} Hash_file;

void HT_Init();

void HT_Close();

int HT_CreateFile(const char *filename, rec_attr attr, int buckets);

Hash_file *HT_OpenFile(const char *filename);

int HT_CloseFile(Hash_file *handle);

int HT_InsertEntry(Hash_file* info, Record record, int *block_id);

int HT_DeleteEntry(Hash_file *handle, void *value);

int HT_GetAllEntries(Hash_file *handle, rec_attr attr, void *value, Dl_list records);

int HT_PrintFile(Hash_file *handle, FILE *stream);

int HT_GetEntry(Hash_file *handle, void *value, Record *rec);

size_t hash_key(attr_type type, const void *key);


typedef struct {
    int rec_num;
    int overf_block;
} Hash_block;

#endif /* HASH_FILE_H */