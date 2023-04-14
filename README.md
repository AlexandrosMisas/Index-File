
## Collaborators
* [Alexandros Misas](https://github.com/AlexandrosMisas)
* [Marios Achilias](https://github.com/MariosAchilias)





# Index
- [Data format](#data-format)
- [Building](#compile)
- [Usage](#usage)
- [Macros](#macros)
- [Heap File Module Interface](#hp)
- [Hash File Module Interface](#ht)
- [Secondary Hash File Module Interface](#sht)
- [Doubly Linked List Module Interface](#dll)

# Data format <a name="data-format"></a>

```c
typedef enum {
    ID,
    NAME,
    SURNAME,
    CITY
} rec_attr
```
```c
typedef struct {
    int  id;
    char name[15];
    char surname[20];
    char city[20];
} Record
```

Record data fields can be removed or added by modifying record.h and record.c.

Supported field data types are integer and fixed-length string.

Duplicate records (i.e. records with the same value in field set as primary key) are not inserted.

---
# Building <a name="compile"></a>

Makefile options:
- hash_file
- heap_file
- shash_file
- all
- clean
- run
- gdb
- valgrind

Modules are built along with their unit tests.

Object files are stored in bin/.

``make run``

Runs all module unit tests that have already been built

``make gdb``

Debug all built unit tests with gdb

``make valgrind``

Run valgrind on all built unit tests

# Usage <a name="usage"></a>
Before calling any module function, BF_Init must be called.

BF_Close should be called when module functions will not be used anymore.

Before calling HT or SHT module functions, HT_Init must also be called.

HT_Close should be called when HT and SHT module functions will not be used anymore.

There can only be one valid handle to a file at a time. Concurrent access is not allowed.

---

# Macros <a name="macros"></a>
For heap file and hash file modules:
```c
INSERTED(handle, call)
```

Used to check if a record was actually inserted (or if given record was a duplicate)

```c
DELETED(handle, call)
```

Used to check if a record was actually deleted (or if given record wasn't found) 

E.g.

```c
bool inserted = INSERTED(handle, HT_InsertEntry(handle, rec, NULL));
bool deleted  = DELETED(handle, HT_DeleteEntry(handle, val));
```

```c
GET_NUM_ENTRIES(call)
```

Used to get number of records found by HP/HT_GetAllEntries when storing them is not needed.

TMP_LIST must be given as list argument.

E.g.

```c
int rec_cnt = GET_NUM_ENTRIES(HT_GetAllEntries(handle, attr, value, TMP_LIST))
```

# Heap File Module Interface <a name="hp"></a>
---

```c
int HP_CreateFile(const char *filename, rec_attr attr)
```
Create a new heap file.

Returns 0 on success, -1 on error.

### Parameters
`const char *filename`

Name of file to create

`rec_attr attr`

Attribute to use as primary key

---
```c
Heap_file *HP_OpenFile(const char *filename)
```
Open existing heap file.

Returns heap file handle on success, or NULL on error.

### Parameters
`const char *filename`

Name of file to open

---
```c
int HP_CloseFile(Heap_file *handle)
```
Close heap file.

Returns 0 on success, -1 on error.

### Parameters
`Heap_file *handle`

Handle of heap file to close

---
```c
int HP_InsertEntry(Heap_file *handle, Record rec)
```
Insert a record (duplicates are not inserted).

Returns 0 on success (record was inserted or already existed), or -1 on error.

Whether the record was inserted or was a duplicate can be determined using the INSERTED macro.

### Parameters
`Heap_file *handle`

Heap file handle

`Record rec`

Record to insert

```c
int HP_DeleteEntry(Heap_file *handle, void *value)
```
Delete the record with primary key attribute equal to given value.

Returns 0 on success (record was deleted or didn't exist), or -1 on error.

Whether the record was deleted or didn't exist can be determined using the DELETED macro.

### Parameters

`Heap_file *handle`

Heap file handle

`void *value`

Pointer to value to compare against primary key attribute

---
```c
int HP_GetAllEntries(Heap_file *handle, rec_attr attr, void *value, Dl_list records)
```
Add all records with given attribute equal to given value to list.

Returns 0 on success, or -1 on error.

The number of records whose given attribute matches given value can be determined by comparing list's size before and after calling function.

### Parameters
`Heap_file *handle`

Heap file handle

`rec_attr attr`

Attribute of record to compare with value

`void *value`

Pointer to value

`Dl_list records`

A handle to a doubly linked list (must be already initialized) in which records are inserted

---
```c
int HP_GetEntry(Heap_file *handle, void *value, Record *rec)
```

Get record with primary key attribute equal to value.

Returns 0 on success, or -1 on error.

### Parameters

`Heap_file *handle`

Heap file handle

`void *value`

Pointer to value to compare against primary key attribute

`Record *rec`

Address where to store record

---
```c
int HP_PrintFile(Heap_file *handle, FILE *stream)
```

Print all records in file to given stream.

Returns 0 on success, or -1 on error.

### Parameters

`Heap_file *handle`

Pointer to heap file

`FILE *stream`

File pointer of stream to print to

---

# Hash File Module Interface <a name="ht"></a>

Implements static hashing (number of buckets is given at time of creation and remains constant).

`void HT_Init()` must be called before any use of hash file or secondary hash file functions.

The filenames of associated secondary indexes (SHT files created with HT file as primary index) are stored as metadata (renaming them will cause problems).

When deleting a record from a primary index, it is also deleted in all associated secondary indexes.

---

```c
extern Hash_map file_map
```

Global map with open HT and SHT filenames as keys and HT or SHT handles (pointers) as values.

---

```c
void HT_Init()
```

Initializes global map Hash_map. Must be called before any other HT or SHT functions.

---
```c
int HT_CreateFile(const char *filename, rec_attr attr, int buckets)
```

Create a new hash file.

Returns 0 on success, or -1 on error.

### Parameters

`const char *filename`

Name of file to create

`rec_attr attr`

Record attribute to use as primary key

`int buckets`

Number of buckets

---
```c
Hash_file *HT_OpenFile(const char *filename)
```

Open existing hash file.

Returns hash file handle on success, or NULL on error.

### Parameters

`const char *filename`

Name of file to open

---
```c
int HT_CloseFile(Hash_file *handle);
```

Close hash file.

Returns 0 on success, or -1 on error.

### Parameters

`Hash_file *handle`

Handle of file to close

---
```c
int HT_InsertEntry(Hash_file *handle, Record record, int *block_id)
```

Insert new record (duplicates are ignored).

Whether the record was inserted or was a duplicate can be determined using the INSERTED macro.

Returns 0 on success, or -1 on error.

### Parameters

`Hash_file *handle`

Hash file handle

`Record record`

Record to insert

`int *block_id`

Address to store id of block where the record was inserted

---
```c
int HT_DeleteEntry(Hash_file *handle, void *value)
```

Delete the record with primary key attribute equal to given value.

Also removed from all associated secondary indexes.

Returns 0 on success (record was deleted or didn't exist), or -1 on error.

Whether the record was deleted or did not exist can be determined by using the DELETED macro. 

### Parameters

`Hash_file *handle`

Hash file handle

`void *value`

Pointer to value to compare against primary key attribute

---
```c
int HT_GetAllEntries(Hash_file *handle, rec_attr attr, void *value, Dl_list records)
```

Add all records with given attribute equal to given value to list.

Returns 0 on success, or -1 on error.

The number of records whose given attribute matches given value can be determined by comparing list's size before and after calling function.

### Parameters

`Hash_file *handle`

Hash file handle

`rec_attr attr`

Record field to compare with value

`void *value`

Pointer to value

`Dl_list records`

A handle to a doubly linked list (must be initialized) in which records are inserted

---
```c
int HT_PrintFile(Hash_file *handle, FILE *stream);
```

Print all records in file to given stream.

### Parameters

`Hash_file *handle`

Hash file handle

`FILE *stream`

File pointer to print to

---
```c
int HT_GetEntry(Hash_file *handle, void *value, Record *rec)
```

Find record with primary key attribute equal to value.

### Parameters

`Hash_file *handle`

Hash file handle

`void *value`

Pointer to value to compare against primary key attribute

`Record *rec`

Address where to store record

---
# Secondary Hash File Module <a name="sht"></a>
---
Every secondary hash file is a secondary index on a hash file.

`void HT_Init()` must be called before any use of hash file or secondary hash file functions.

Renaming a secondary hash file is not allowed.

Records are automatically deleted from all associated secondary index files when deleted from primary index.

---
```c
int SHT_CreateFile(const char *sfilename, rec_attr attr, const char *filename, int buckets);
```

Create a new secondary hash file.

Returns 0 on success, or -1 on error.

### Parameters

`const char *sfilename`

Name of file to create

`rec_attr attr`

Record attribute to use as secondary key (non-unique) 

`const char *filename`

Name of (primary) hash file

`int buckets`

Number of buckets to use in secondary index (can be different from number of buckets in primary hash file)

---
```c
int SHT_CloseFile(SHash_file *handle)
```

Close secondary hash file.

Returns 0 on success, or -1 on error.

### Parameters

`SHash_file *handle`

Handle of file to close

---
```c
int SHT_InsertEntry(SHash_file *handle, Record record, int block_id)
```

Add entry to secondary hash file given id of block where the record was inserted in the primary index. Duplicates are allowed.

Returns 0 on success, or -1 on error.

### Parameters

`SHash_file *handle`

Secondary hash file handle

`Record record`

Record from which to take value of secondary key (must already exist in primary index).

`int block_id`

Block where record was inserted in the primary index

---
```c
int SHT_DeleteEntry(SHash_file *handle, void *value, int block_id)
```

Delete entry from secondary hash file with secondary key attribute equal to given value and was inserted into a specified block.

Returns 0 on success, or -1 on error.

### Parameters

`SHash_file *handle`

Secondary hash file handle

`void *value`

Pointer to value to compare against secondary key attribute

`int block_id`

Block where record was inserted in primary index

---
```c
SHT_GetEntries(SHash_file *handle, void *value, Dl_list records)
```

Returns 0 on success, or -1 on error.

Add all records with given attribute equal to given value to list

### Parameters

`SHash_file *handle`

Secondary hash file handle

`void *value`

Pointer to value to compare against secondary key attribute

`Dl_list records`

A handle to a doubly linked list (must be initialized) in which records are inserted

# Doubly Linked List Module Interface <a name="dll"></a>

Generic (non-intrusive) doubly linked list.

## Notes

The given pointers are stored in the list, the actual payloads are not copied. So pointers need to remain valid while they are in the list.

Pointers inserted should not be free'd manually if a Destructor function was set.

``Dl_list`` is a pointer (defined as ``typedef struct dl_list *Dl_list``) and no indirection is needed when passing a list to functions.

## Types

``(*Destructor)(void *a)``

Function called on node values when they are deleted (if values shouldn't be destroyed, set to NULL).

(free() is sufficient for simple data types)

``(*Comparator)(void *a, void *b)``

Function used to compare values when searching in the list.

``(*Visit)(void *a)``

Function to be called by print_list.

## Functions

```c
Dl_list list_create(Destructor destroy_func)
```

Create new list.

### Parameters

``Destructor destroy_func``

Pointer to function that will be called on values of deleted nodes. (can be NULL)

---

```c
int list_size(Dl_list list)
```

Get number of nodes in given list.

### Parameters

``Dl_list list``

Doubly linked list 

---
```c
Dl_list_node list_first(Dl_list list)
```
Get first node in given list.

Returns a valid node pointer on success, or NULL on error.

### Parameters

``Dl_list list``

Doubly linked list

---
```c
Dl_list_node list_last(Dl_list list)
```
Get last node in given list.

Returns a valid node pointer on success, or NULL on error.

### Parameters

``Dl_list list``

Doubly linked list

---
```c
Dl_list_node list_next(Dl_list_node node)
```

Get node next of given node.

Returns a valid node pointer on success, or NULL on error.

### Parameters

``Dl_list_node node``

List node

---
```c
Dl_list_node list_previous(Dl_list_node node)
```

Get node previous of given node.

Returns a valid node pointer on success, or NULL on error.

### Parameters

``Dl_list_node node``

List node

---
```c
void list_insert(Dl_list list, void *entry)
```

Insert new node with given value.

### Parameters

``Dl_list list``

Doubly linked list

``void *entry``

Value to insert

---
```c
void list_delete(Dl_list list, Dl_list_node node)
```

Delete given list node.

### Parameters

``Dl_list list``

Doubly linked list

``Dl_list_node node``

Node to delete

---
```c
Dl_list_node list_find(Dl_list list, void *value, Comparator compare)
```

Return node first node found that has a value equal (according to given Comparator function) to the given value.

Returns NULL if no such node was found.

### Parameters

``Dl_list list``

Doubly linked list

``void *value``

Value to compare against

``Comparator compare``

Pointer to function used to compare values

---
```c
void *list_value(Dl_list_node node)
```

Get node value.

Returns NULL if invalid node was given.

### Parameters

``Dl_list_node node``

Node to get value from

---
```c
void print_list(Dl_list list, Visit visit_func)
```

Traverse list and call given function with each value as argument.

Useful for printing, and other similar use cases.


### Parameters

``Dl_list list``

Doubly linked list

``Visit visit_func``

Function to call on all nodes

---
```c
void list_destroy(Dl_list list)
```

Destroy list (all contained values are also destroyed, unless destroy_func was set to NULL).

### Parameters

``Dl_list``

Doubly linked list
