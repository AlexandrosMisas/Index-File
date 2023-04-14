#ifndef HEAP_FILE_H
#define HEAP_FILE_H

#include "common.h"
#include "dl_list.h"
#include "record.h"

typedef struct {
    char file_type[5];
    int file_desc;
    int last_block_id;
    int rec_capacity;
    int rec_count;
    rec_attr attr;
} Heap_file;


int HP_CreateFile(const char *filename, rec_attr attr);

Heap_file *HP_OpenFile(const char *filename);

int HP_CloseFile(Heap_file *handle);

int HP_InsertEntry(Heap_file *handle, Record rec);

int HP_DeleteEntry(Heap_file *handle, void *value);

int HP_GetAllEntries(Heap_file *handle, rec_attr attr, void *value, Dl_list records);

int HP_GetEntry(Heap_file *handle, void *value, Record *rec);

int HP_PrintFile(Heap_file *handle, FILE *stream);

#endif /* HEAP_FILE_H */
