#include "acutest.h"
#include "shash_file.h"
#include "dl_list.h"
#include "common.h"

#define BUCKETS 200
#define RECORDS_NUM 2000
#define TO_DELETE 40


#define FILENAME "data.db"
#define INDEXNAME "data1.db"


#define UNIQUE_REC(i) ({                                        \
    char buffer[100];                                           \
    memset(buffer, 0, sizeof buffer);                           \
    Record rec = { .id = i };                                   \
                                                                \
    sprintf(buffer, "name_%d", i);                              \
    memcpy(rec.name, buffer, sizeof_field(Record, name));       \
                                                                \
    sprintf(buffer, "surname_%d", i);                           \
    memcpy(rec.surname, buffer, sizeof_field(Record, name));    \
                                                                \
    sprintf(buffer, "city_%d", i);                              \
    memcpy(rec.city, buffer, sizeof_field(Record, city));       \
    rec;                                                        \
})


static int compare_records(SRecord *a, SRecord *b, rec_attr attr, size_t bucket) 
{
    void *a_ = get_attr_type(attr) == INT 
        ? (void*)&a->key.ikey 
        : (void*)a->key.skey;

    void *b_ = get_attr_type(attr) == INT 
        ? (void*)&b->key.ikey 
        : (void*)b->key.skey;

    return !memcmp(a_, b_, get_attr_size(attr)) 
        && a->block_id == b->block_id 
        && a->counter  == b->counter 
        && hash_key(get_attr_type(attr), a_) % BUCKETS == bucket;
}


static SRecord *create_record(void *key, int block_id, rec_attr attr) 
{
    SRecord *rec_ = malloc(sizeof(*rec_));
    rec_->counter = 1;
    rec_->block_id = block_id;
    memcpy(
        get_attr_type(attr) == INT
            ? (void*)&rec_->key.ikey
            : (void*)rec_->key.skey,
        key,
        get_attr_size(attr)
    );
    return rec_;
}


static int list_compare(void *a, void* b) 
{
    SRecord *a_ = (SRecord*)a;
    SRecord *b_ = (SRecord*)b;

    return !(strcmp(a_->key.skey, b_->key.skey) == 0 
        && a_->block_id == b_->block_id);
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


const rec_attr attr[] = {
    NAME,
    SURNAME,
    CITY
};


void test_create() 
{
    srand(time(NULL) * getpid());

    HT_Init();
    TEST_ASSERT(BF_Init(LRU) == BF_OK);

    int r = rand() % array_size(attr);
    TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);
    TEST_ASSERT(SHT_CreateFile(INDEXNAME, attr[r], FILENAME, BUCKETS) == 0);

    Hash_file *handle;
    SHash_file *shandle;
    TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
    TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);

    TEST_ASSERT(shandle->buckets == BUCKETS);
    TEST_ASSERT(shandle->rec_count == 0);
    TEST_ASSERT(shandle->attr == attr[r]);
    TEST_ASSERT(strcmp("sht", shandle->file_type) == 0);

    for (size_t i = 0;  i < BUCKETS; ++i)
        TEST_ASSERT(shandle->hash_table[i] == -1);


    TEST_ASSERT(strcmp(FILENAME, shandle->index_filename) == 0);
    TEST_ASSERT(strcmp(handle->index_files[attr[r] - 1].filename, INDEXNAME) == 0);



    TEST_ASSERT(HT_CloseFile(handle) == 0);
    TEST_ASSERT(SHT_CloseFile(shandle) == 0);

    TEST_ASSERT(remove(FILENAME) == 0);
    TEST_ASSERT(remove(INDEXNAME) == 0);

    TEST_ASSERT(BF_Close() == BF_OK);
    HT_Close();
}


void test_insert() 
{
	srand(time(NULL) * getpid());

    HT_Init();
    TEST_ASSERT(BF_Init(LRU) == BF_OK);

    int r = rand() % array_size(attr);
    TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);
    TEST_ASSERT(SHT_CreateFile(INDEXNAME, attr[r], FILENAME, BUCKETS) == 0);

    Hash_file *handle;
    SHash_file *shandle;
    TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
    TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);

    Record rec = random_record();
    void *key = malloc(get_attr_size(attr[r]));
    memcpy(key, get_rec_member(&rec, attr[r]), get_attr_size(attr[r]));
    int counter = 0, block_id;
    for (int i = 0; i < RECORDS_NUM; ++i) {
        counter += !memcmp(key, get_rec_member(&rec, attr[r]), get_attr_size(attr[r]));
        TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, rec, &block_id)));
        TEST_ASSERT(SHT_InsertEntry(shandle, rec, block_id) == 0);
        rec = random_record();
    }

    TEST_ASSERT(GET_NUM_ENTRIES(SHT_GetEntries(shandle, key, TMP_LIST)) == counter);

    TEST_ASSERT(HT_CloseFile(handle) == 0);
    TEST_ASSERT(SHT_CloseFile(shandle) == 0);

    TEST_ASSERT(remove(FILENAME) == 0);
    TEST_ASSERT(remove(INDEXNAME) == 0);



    TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);
    TEST_ASSERT(SHT_CreateFile(INDEXNAME, attr[r], FILENAME, BUCKETS) == 0);

    TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
    TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);

    Dl_list *buckets = calloc(BUCKETS, sizeof(*buckets));
    for (int i = 0; i < RECORDS_NUM; ++i) {
        TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, (rec = UNIQUE_REC(i)), &block_id)));
        TEST_ASSERT(INSERTED(shandle, SHT_InsertEntry(shandle, rec, block_id)));
        TEST_ASSERT(GET_NUM_ENTRIES(SHT_GetEntries(shandle, get_rec_member(&rec, attr[r]), TMP_LIST)) == 1);
        size_t bucket = hash_key(get_attr_type(attr[r]), get_rec_member(&rec, attr[r])) % BUCKETS;

        if (buckets[bucket] == NULL)
            buckets[bucket] = list_create(free);
        list_insert(buckets[bucket], create_record(get_rec_member(&rec, attr[r]), block_id, attr[r]));
    }

    TEST_ASSERT(HT_CloseFile(handle) == 0);
    TEST_ASSERT(SHT_CloseFile(shandle) == 0);


    TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);
    TEST_ASSERT(shandle->rec_count == RECORDS_NUM);

    for (size_t i = 0; i < shandle->buckets; ++i)
        if (buckets[i] != NULL)
            TEST_ASSERT(shandle->hash_table[i] > 0);

    int block_t;
    BF_Block *block;
    BF_Block_Init(&block);
    for (size_t i = 0; i < shandle->buckets; ++i) {
        if ((block_t = shandle->hash_table[i]) == -1)
            continue;

        size_t rec_count = 0;
        SHash_block block_handle;
        Dl_list_node node = list_last(buckets[i]);
        while (block_t != -1) {
            TEST_ASSERT(BF_GetBlock(shandle->file_desc, block_t, block) == BF_OK);
            char *data = BF_Block_GetData(block);
            memcpy(&block_handle, data, sizeof(Hash_block));

            rec_count += block_handle.rec_num;
            data += sizeof(SRecord) * (block_handle.rec_num - 1) + sizeof(SHash_block);
            for (size_t j = block_handle.rec_num; j > 0; j--, data -= sizeof(SRecord)) {
                SRecord rec;
                memcpy(&rec, data, sizeof(SRecord));
                TEST_ASSERT(compare_records(&rec, list_value(node), attr[r], i));
                node = list_previous(node);
            }
            block_t = block_handle.overf_block;
            TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
        }
        TEST_ASSERT(list_size(buckets[i]) == rec_count);
    }


    BF_Block_Destroy(&block);

    for (size_t i = 0; i < shandle->buckets; ++i)
        if (buckets[i] != NULL)
            list_destroy(buckets[i]);
    free(buckets);
    free(key);


    TEST_ASSERT(SHT_CloseFile(shandle) == 0);
    TEST_ASSERT(remove(FILENAME) == 0);
    TEST_ASSERT(remove(INDEXNAME) == 0);
    TEST_ASSERT(BF_Close() == BF_OK);
    HT_Close();
}


void test_delete()
{
	HT_Init();

	srand(time(NULL) * getpid());
	TEST_ASSERT(BF_Init(LRU) == BF_OK);

	Hash_file *handle;
	SHash_file *shandle;

    TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);
    TEST_ASSERT(SHT_CreateFile(INDEXNAME, NAME, FILENAME, BUCKETS) == 0);
    TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
    TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);

	int block_id;
	Record rec;
	Dl_list *buckets = calloc(BUCKETS, sizeof(*buckets));
	int *block_ids = calloc(RECORDS_NUM, sizeof(int));
	for (int i = 0; i < RECORDS_NUM; ++i) {
		rec = UNIQUE_REC(i);
		size_t bucket = hash_key(STRING, rec.name) % BUCKETS;
		TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, rec, &block_id)));
		TEST_ASSERT(INSERTED(shandle, SHT_InsertEntry(shandle, rec, block_id)));
		block_ids[i] = block_id;

		if (buckets[bucket] == NULL)
			buckets[bucket] = list_create(free);
		list_insert(buckets[bucket], create_record(rec.name, block_id, NAME));
	}
	TEST_ASSERT(SHT_CloseFile(shandle) == 0);
	
	int *to_delete = random_numbers(TO_DELETE, 0, RECORDS_NUM - 1);
	for (int i = 0; i < TO_DELETE; ++i) {
		Record rec;
		TEST_ASSERT(HT_GetEntry(handle, &to_delete[i], &rec) == 0);
		size_t bucket = hash_key(STRING, rec.name) % BUCKETS;
		TEST_ASSERT(DELETED(handle, HT_DeleteEntry(handle, &to_delete[i])));
		SRecord srec = create_srecord(rec.name, block_ids[to_delete[i]], NAME);
		list_delete(buckets[bucket], list_find(buckets[bucket], &srec, list_compare));
	}
	TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);
	TEST_ASSERT(shandle->rec_count == (RECORDS_NUM - TO_DELETE));



	int block_t;
    BF_Block *block;
    BF_Block_Init(&block);
    for (int i = 0; i < shandle->buckets; ++i) {
        if ((block_t = shandle->hash_table[i]) == -1)
            continue;
        
        size_t rec_count = 0;
        SHash_block block_handle;
        Dl_list_node node = list_last(buckets[i]);
        while (block_t != -1) {
            TEST_ASSERT(BF_GetBlock(shandle->file_desc, block_t, block) == BF_OK);
            char *data = BF_Block_GetData(block);
            memcpy(&block_handle, data, sizeof(SHash_block));
            rec_count += block_handle.rec_num;
			
            data += sizeof(SRecord) * (block_handle.rec_num - 1) + sizeof(SHash_block);
            for (int j = block_handle.rec_num; j > 0; j--, data -= sizeof(SRecord)) {
                SRecord rec;
                memcpy(&rec, data, sizeof(SRecord));
                TEST_ASSERT(compare_records(&rec, list_value(node), NAME, i));
                node = list_previous(node);
            }
            block_t = block_handle.overf_block;
            TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
        }
    }
    BF_Block_Destroy(&block);

	for (int i = 0; i < BUCKETS; ++i)
		if (buckets[i] != NULL)
			list_destroy(buckets[i]);
	free(buckets);
	free(block_ids);
	free(to_delete);

	TEST_ASSERT(HT_CloseFile(handle) == 0);
	TEST_ASSERT(SHT_CloseFile(shandle) == 0);
	TEST_ASSERT(remove(FILENAME) == 0);
	TEST_ASSERT(remove(INDEXNAME) == 0);
	


	Record rec_dupl[] = {
		{ .id = 0,   .name = "Mary", .surname = "Smith",    .city = "New York" },
		{ .id = 399, .name = "John", .surname = "Johnson",  .city = "New York" },
		{ .id = 515, .name = "Alex", .surname = "Williams", .city = "New York" }
	};

	TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);
	TEST_ASSERT(SHT_CreateFile(INDEXNAME, CITY, FILENAME, BUCKETS) == 0);
	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
	TEST_ASSERT((shandle = SHT_OpenFile(INDEXNAME)) != NULL);

	int size = array_size(rec_dupl);
	for (size_t i = 0; i < size; ++i) {
		TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, rec_dupl[i], &block_id)));
		TEST_ASSERT(SHT_InsertEntry(shandle, rec_dupl[i], block_id) == 0);
		TEST_ASSERT(shandle->rec_count == 1);
		TEST_ASSERT(GET_NUM_ENTRIES(SHT_GetEntries(shandle, rec_dupl[i].city, TMP_LIST)) == i + 1);
	}
	TEST_ASSERT(HT_CloseFile(handle) == 0);

	for (size_t i = 0; i < size; ++i) {
		TEST_ASSERT(SHT_DeleteEntry(shandle, rec_dupl[i].city, block_id) == 0);
		TEST_ASSERT(shandle->rec_count == (i != size - 1));
		TEST_ASSERT(GET_NUM_ENTRIES(
			SHT_GetEntries(shandle, rec_dupl[i].city, TMP_LIST)) == (i == size - 1 ? 0 : size)
		);
	}


	TEST_ASSERT(SHT_CloseFile(shandle) == 0);
	TEST_ASSERT(remove(FILENAME) == 0);
	TEST_ASSERT(remove(INDEXNAME) == 0);
	TEST_ASSERT(BF_Close() == BF_OK);
	HT_Close();
}


TEST_LIST = {
    { "test_create", test_create },
	{ "test_insert", test_insert},
	{ "test_delete", test_delete },
    { NULL, NULL }
};
