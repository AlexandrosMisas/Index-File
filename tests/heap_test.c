#include "heap_file.h"
#include "common.h"
#include "acutest.h"
#include "dl_list.h"


#define FILENAME "data1.db"
#define FILENAME2 "data2.db"
#define RECORDS_NUM 2000
#define TO_DELETE 20


const rec_attr attr[] = {
    ID,
    NAME,
    SURNAME,
    CITY
};


static int compare_records(Record *a, Record *b) 
{
    return a->id == b->id 
        && !strcmp(a->name, b->name)
        && !strcmp(a->surname, b->surname)
        && !strcmp(a->city, b->city); 
}


static int list_compare(void *a, void *b) 
{
    return ((Record*)a)->id - ((Record*)b)->id;
}


static Record *create_record(Record rec) 
{
    Record *rec_ = malloc(sizeof(*rec_));
    return memcpy(rec_, &rec, sizeof(Record));
}


static int *random_numbers(int size, int min, int max) 
{
    int *array = malloc(size * sizeof(int));
    bool exists;
    int new;

    for (size_t i = 0; i < size; ++i) {
        do {
            exists = true;
            new = rand() % (max - min + 1) + min;
            for (size_t j = 0; j < i; j++) {
                if (array[j] == new) {
                    exists = false;
                    break;
                }
            }
        } while (!exists);
        array[i] = new;
    }
    return array;
}


void test_create() 
{
    srand(time(NULL) * getpid());
    int r = rand() % array_size(attr);

    TEST_ASSERT(BF_Init(LRU) == BF_OK);
    TEST_ASSERT(HP_CreateFile(FILENAME, attr[r]) == 0);

    Heap_file *handle = HP_OpenFile(FILENAME);

    TEST_ASSERT(handle != NULL);
    TEST_ASSERT(strcmp(handle->file_type, "heap") == 0);
    TEST_ASSERT(handle->last_block_id == 0);
    TEST_ASSERT(handle->rec_count == 0);
    TEST_ASSERT(handle->attr == attr[r]);

    TEST_ASSERT(HP_CloseFile(handle) == 0);
    TEST_ASSERT(BF_Close() == BF_OK);
    TEST_ASSERT(remove(FILENAME) == 0);
}


void test_insert() 
{   
    srand(time(NULL) * getpid());

    int rec_counter = 0, rec_num;
    char *data;
    BF_Block *block;
    Heap_file *handle;

    TEST_ASSERT(BF_Init(LRU) == BF_OK);
    TEST_ASSERT(HP_CreateFile(FILENAME, ID) == 0);
    TEST_ASSERT((handle = HP_OpenFile(FILENAME)) != NULL);

    BF_Block_Init(&block);
    Record *rec = calloc(RECORDS_NUM + 1, sizeof(*rec));


    for (int i = 0; i <= RECORDS_NUM; ++i) {
        rec[i] = random_record();
        TEST_ASSERT(INSERTED(handle, HP_InsertEntry(handle, rec[i])));

        if (i % handle->rec_capacity == 0) {
            rec_counter = 0;
            if (handle->last_block_id > 1) {
                memcpy(&rec_num, data, sizeof(int));
                TEST_ASSERT(rec_num == handle->rec_capacity);
                TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
            }
        }
        TEST_ASSERT(BF_GetBlock(handle->file_desc, handle->last_block_id, block) == BF_OK);
        memcpy(&rec_num, (data = BF_Block_GetData(block)), sizeof(int));
        TEST_ASSERT(++rec_counter == rec_num);
    }

    Record dummy_rec = { .id = 125 };
    TEST_ASSERT(!INSERTED(handle, HP_InsertEntry(handle, dummy_rec)));

    dummy_rec.id = 780;
    TEST_ASSERT(!INSERTED(handle, HP_InsertEntry(handle, dummy_rec)));

    TEST_ASSERT(HP_CloseFile(handle) == 0);

    TEST_ASSERT((handle = HP_OpenFile(FILENAME)) != NULL);
    TEST_ASSERT(handle->rec_count == RECORDS_NUM + 1);


    int counter = 0;
    for (int i = 1; i <= handle->last_block_id; ++i) {
        TEST_ASSERT(BF_GetBlock(handle->file_desc, i, block) == BF_OK);
        
        memcpy(&rec_num, (data = BF_Block_GetData(block)), sizeof(int));
        TEST_ASSERT(i != handle->last_block_id
            ? rec_num == handle->rec_capacity
            : rec_num <= handle->rec_capacity
        );

        data += sizeof(int);
        for (int j = 0; j < rec_num; j++, data += sizeof(Record)) {
            Record _rec;
            memcpy(&_rec, data, sizeof(Record));
            TEST_ASSERT(compare_records(&_rec, &rec[counter++]));
        }
        TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
    }
    free(rec);
    BF_Block_Destroy(&block);

    TEST_ASSERT(HP_CloseFile(handle) == 0);
    TEST_ASSERT(remove(FILENAME) == 0);
    TEST_ASSERT(BF_Close() == BF_OK);
}


void test_delete() 
{
    srand(time(NULL) * getpid());
    TEST_ASSERT(BF_Init(LRU) == BF_OK);

    TEST_ASSERT(HP_CreateFile(FILENAME, ID) == 0);

    Heap_file *handle;
    char *data;
    Dl_list list = list_create(free);

    TEST_ASSERT((handle = HP_OpenFile(FILENAME)) != NULL);


    for (int i = 0; i <= RECORDS_NUM >> 1; i++) {
        Record rec = random_record();
        TEST_ASSERT(INSERTED(handle, HP_InsertEntry(handle, rec)));
        list_insert(list, create_record(rec));
    }

    int *to_delete = random_numbers(TO_DELETE, 0, RECORDS_NUM >> 1);
    for (int i = 0; i < TO_DELETE; i++) {
        Record dummy_rec = { .id = to_delete[i] };
        TEST_ASSERT(DELETED(handle, HP_DeleteEntry(handle, &to_delete[i])));
        TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle, ID, &to_delete[i], TMP_LIST)) == 0);
        list_delete(list, list_find(list, &dummy_rec, list_compare));
    }

    int dummy_id = RECORDS_NUM;
    TEST_ASSERT(!DELETED(handle, HP_DeleteEntry(handle, &dummy_id)));

    dummy_id = to_delete[rand() % TO_DELETE];
    TEST_ASSERT(!DELETED(handle, HP_DeleteEntry(handle, &dummy_id)));

    TEST_ASSERT(HP_CloseFile(handle) == 0);

    TEST_ASSERT((handle = HP_OpenFile(FILENAME)) != NULL);
    TEST_ASSERT(list_size(list) == handle->rec_count);

    BF_Block *block;
    BF_Block_Init(&block);

    Dl_list_node node = list_first(list);
    for (int i = 1; i <= handle->last_block_id; i++) {
        TEST_ASSERT(BF_GetBlock(handle->file_desc, i, block) == BF_OK);

        int rec_num;		
        memcpy(&rec_num, (data = BF_Block_GetData(block)), sizeof(int));
        data += sizeof(int);

        for (int j = 0; j < rec_num; j++, data += sizeof(Record)) {
            Record rec;
            memcpy(&rec, data, sizeof(Record));
            TEST_ASSERT(compare_records(&rec, list_value(node)));
            node = list_next(node);
        }
        TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
    }


    free(to_delete);
    list_destroy(list);
    BF_Block_Destroy(&block);

    TEST_ASSERT(HP_CloseFile(handle) == 0);
    TEST_ASSERT(BF_Close() == BF_OK);
    TEST_ASSERT(remove(FILENAME) == 0);
}


void test_find() 
{
    srand(time(NULL) * getpid());

    TEST_ASSERT(BF_Init(LRU) == 0);

    Record rec_array[] = {
        { .id = 1912,    .name = "Alan",     .surname = "Turing",     .city = "Maida Vale"     },
        { .id = 1903,    .name = "Alonzo",   .surname = "Church",     .city = "Washington DC"  },
        { .id = 1923,    .name = "Edgar",    .surname = "Codd",       .city = "Fortuneswell"   },
        { .id = 1920,    .name = "Kenneth",  .surname = "Iverson",    .city = "Camrose"        },
        { .id = 1815,    .name = "Ada",    	 .surname = "Lovelace",   .city = "London"         },
        { .id = 1930,    .name = "Edsger",   .surname = "Dijkstra",   .city = "Rotterdam"      },
        { .id = 1941,    .name = "Dennis",   .surname = "Ritchie",    .city = "Bronxville"     },
        { .id = 1969,    .name = "Linus",    .surname = "Torvalds",   .city = "Helsinki"       },
        { .id = 1910,    .name = "Konrad",   .surname = "Zuse",       .city = "Berlin"         },
        { .id = 1933,    .name = "Andrei",   .surname = "Kolmogorov", .city = "Tambov"         },
        { .id = 1931,    .name = "Andrei",   .surname = "Yershov", 	  .city = "Moscow"         },
        { .id = 1906, 	 .name = "Kurt",   	 .surname = "Godel",      .city = "Brno"      	   },
        { .id = 1909, 	 .name = "Vladimir", .surname = "Kondrashov", .city = "Moscow"         }
    };

    TEST_ASSERT(HP_CreateFile(FILENAME, SURNAME) == 0);

    Heap_file *handle;
    TEST_ASSERT((handle = HP_OpenFile(FILENAME)) != NULL);

    for (int i = 0; i < array_size(rec_array); i++)
        TEST_ASSERT(INSERTED(handle, HP_InsertEntry(handle, rec_array[i])));
        

    Record find = { .id = -1 };
    for (int i = 0; i < array_size(rec_array); i++) {
        TEST_ASSERT(HP_GetEntry(handle, rec_array[i].surname, &find) == 0);
        TEST_ASSERT(compare_records(&find, &rec_array[i]));
    }

    TEST_ASSERT(HP_GetEntry(handle, "Hilbert", &find) == 0);
    TEST_ASSERT(find.id == -1);

    TEST_ASSERT(HP_GetEntry(handle, "Wittgenstein", &find) == 0);
    TEST_ASSERT(find.id == -1);

    TEST_ASSERT(HP_CreateFile(FILENAME2, ID) == 0);

    Heap_file *handle_;
    TEST_ASSERT((handle_ = HP_OpenFile(FILENAME2)) != NULL);

    int count_n = 0;
    int count_s = 0;
    Record rec = random_record();
    char *name = strdup(rec.name);
    char *surname = strdup(rec.surname);
    for (int i = 0; i < RECORDS_NUM; i++) {
        TEST_ASSERT(INSERTED(handle_, HP_InsertEntry(handle_, rec)));
        count_n += strcmp(name, rec.name) == 0;
        count_s += strcmp(surname, rec.surname) == 0;
        rec = random_record();
    }

    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle_, NAME, name, TMP_LIST)) == count_n);

    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle_, SURNAME, surname, TMP_LIST)) == count_s);

    int id = rec_array[rand() % array_size(rec_array)].id;
    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle, ID, &id, TMP_LIST)) == 1);

    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle, NAME, "Andrei", TMP_LIST)) == 2);
    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle, CITY, "Moscow", TMP_LIST)) == 2);
    
    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle_, ID, &id, TMP_LIST)) == 1);
    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle, NAME, name, TMP_LIST)) == 0);
    TEST_ASSERT(GET_NUM_ENTRIES(HP_GetAllEntries(handle, SURNAME, surname, TMP_LIST)) == 0);



    TEST_ASSERT(HP_CloseFile(handle_) == 0);
    TEST_ASSERT(HP_CloseFile(handle) == 0);
    TEST_ASSERT(remove(FILENAME) == 0);
    TEST_ASSERT(remove(FILENAME2) == 0);
    TEST_ASSERT(BF_Close() == BF_OK);
    free(name);
    free(surname);	
}


TEST_LIST = {
    { "test_create", test_create },
    { "test_insert", test_insert },
    { "test_delete", test_delete },
    { "test_find",   test_find   },

    { NULL, NULL }
};
