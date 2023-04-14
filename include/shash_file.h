#ifndef SHASH_FILE_H
#define SHASH_FILE_H

#include "hash_file.h"
#include "dl_list.h"

typedef struct {
    char file_type[5];
    char filename[MAX_FILENAME + 1];
    char index_filename[MAX_FILENAME + 1];
    int file_desc;
    int rec_capacity;
    int rec_count;
    int buckets;
    int last_block_id;
    rec_attr attr;
    int *hash_table;
} SHash_file;


int SHT_CreateFile(const char *sfilename, rec_attr attr, 
                                          const char *filename,
                                          int buckets);

SHash_file *SHT_OpenFile(const char *sfilename);

int SHT_CloseFile(SHash_file *handle);

int SHT_InsertEntry(SHash_file *handle, Record record, int block_id);

int SHT_DeleteEntry(SHash_file *handle, void *value, int block_id);

int SHT_GetEntries(SHash_file *handle, void *value, Dl_list records);


typedef struct {
    int rec_num;
    int overf_block;
} SHash_block;


#endif /* SHASH_FILE_H */
