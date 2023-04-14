#include "acutest.h"
#include "hash_file.h"
#include "dl_list.h"

#define RECORDS_NUM 3000
#define BUCKETS 200
#define TO_DELETE 300

#define FILENAME "data.db"
#define FILENAME2 "data1.db"


const rec_attr attr[] = {
    ID,
    NAME,
    SURNAME,
    CITY
};


static int compare_records(Record *a, Record *b, rec_attr attr, size_t bucket) 
{
    return a->id == b->id
        && !strcmp(a->name, b->name)
        && !strcmp(a->surname, b->surname)
        && !strcmp(a->city, b->city)
        && hash_key(attr, get_rec_member(a, attr)) % BUCKETS == bucket;
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
	HT_Init();

	TEST_ASSERT(BF_Init(LRU) == BF_OK);
	TEST_ASSERT(HT_CreateFile(FILENAME, attr[r], BUCKETS) == 0);

	Hash_file *handle;
	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
	TEST_ASSERT(strcmp(handle->file_type, "hash") == 0);
	TEST_ASSERT(strcmp(handle->filename, FILENAME) == 0);
	TEST_ASSERT(handle->buckets == BUCKETS);
	TEST_ASSERT(handle->rec_count == 0);
	TEST_ASSERT(handle->attr == attr[r]);

	for (size_t i = 0; i < handle->buckets; ++i)
		TEST_ASSERT(handle->hash_table[i] == -1);

	TEST_ASSERT(HT_CloseFile(handle) == 0);
	TEST_ASSERT(BF_Close() == BF_OK);
	TEST_ASSERT(remove(FILENAME) == 0);

	HT_Close();
}


void test_insert() 
{
	srand(time(NULL) * getpid());

	HT_Init();
	TEST_ASSERT(BF_Init(LRU) == BF_OK);
	TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);

	Hash_file *handle;
	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);

	Dl_list *buckets = calloc(BUCKETS, sizeof(*buckets));
	for (int i = 0; i < RECORDS_NUM; i++) {
		Record rec = random_record();
		size_t bucket = hash_key(ID, &rec.id) % handle->buckets;
		TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, rec, NULL)));
		TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, ID, &rec.id, TMP_LIST)) == 1);
		if (buckets[bucket] == NULL)
			buckets[bucket] = list_create(free);

		list_insert(buckets[bucket], create_record(rec));
	}
		
	Record dummy_rec = { .id = 125 };
	TEST_ASSERT(!INSERTED(handle, HT_InsertEntry(handle, dummy_rec, NULL)));

	dummy_rec.id = 780;
	TEST_ASSERT(!INSERTED(handle, HT_InsertEntry(handle, dummy_rec, NULL)));


	TEST_ASSERT(HT_CloseFile(handle) == 0);

	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
	TEST_ASSERT(handle->rec_count == RECORDS_NUM);

	for (size_t i = 0; i < handle->buckets; ++i)
		if (buckets[i] != NULL)
			TEST_ASSERT(handle->hash_table[i] > 0);

	int block_t;
	BF_Block *block;
	BF_Block_Init(&block);
	for (size_t i = 0; i < handle->buckets; ++i) {
		if ((block_t = handle->hash_table[i]) == -1)
			continue;

		size_t rec_count = 0;
		Hash_block block_handle;
		Dl_list_node node = list_last(buckets[i]);
		while (block_t != -1) {
			TEST_ASSERT(BF_GetBlock(handle->file_desc, block_t, block) == BF_OK);
			char *data = BF_Block_GetData(block);
			memcpy(&block_handle, data, sizeof(Hash_block));

			rec_count += block_handle.rec_num;
			data += sizeof(Record) * (block_handle.rec_num - 1) + sizeof(Hash_block);
			for (size_t j = block_handle.rec_num; j > 0; j--, data -= sizeof(Record)) {
				Record rec;
				memcpy(&rec, data, sizeof(Record));
				TEST_ASSERT(compare_records(&rec, list_value(node), ID, i));
				node = list_previous(node);
			}
			block_t = block_handle.overf_block;
			TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
		}
		TEST_ASSERT(list_size(buckets[i]) == rec_count);
	}


	BF_Block_Destroy(&block);


	for (size_t i = 0; i < handle->buckets; ++i)
		if (buckets[i] != NULL)
			list_destroy(buckets[i]);
	free(buckets);

	TEST_ASSERT(HT_CloseFile(handle) == 0);
	TEST_ASSERT(BF_Close() == BF_OK);
	TEST_ASSERT(remove(FILENAME) == 0);

	HT_Close();
}


void test_delete() 
{
	HT_Init();

	srand(time(NULL) * getpid());
	TEST_ASSERT(BF_Init(LRU) == BF_OK);

	Hash_file *handle;
	TEST_ASSERT(HT_CreateFile(FILENAME, ID, BUCKETS) == 0);
	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);

	Dl_list *buckets = calloc(BUCKETS, sizeof(*buckets));
	for (int i = 0; i <= RECORDS_NUM; i++) {
		Record rec = random_record();
		size_t bucket = hash_key(ID, &rec.id) % handle->buckets;
		TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, rec, NULL)));
		if (buckets[bucket] == NULL)
			buckets[bucket] = list_create(free);
		list_insert(buckets[bucket], create_record(rec));
	}

	int *to_delete = random_numbers(TO_DELETE, 0, RECORDS_NUM);    
	for (int i = 0; i < TO_DELETE; i++) {
		Record dummy_rec = { .id = to_delete[i] };
		size_t bucket = hash_key(ID, &dummy_rec.id) % handle->buckets;
		TEST_ASSERT(DELETED(handle, HT_DeleteEntry(handle, &to_delete[i])));
		TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, ID, &to_delete[i], TMP_LIST)) == 0);
		list_delete(buckets[bucket], list_find(buckets[bucket], &dummy_rec, list_compare));
	}

	int dummy_id = 2 * RECORDS_NUM;
	TEST_ASSERT(!DELETED(handle, HT_DeleteEntry(handle, &dummy_id)));

	dummy_id = to_delete[rand() % TO_DELETE];
	TEST_ASSERT(!DELETED(handle, HT_DeleteEntry(handle, &dummy_id)));

	TEST_ASSERT(HT_CloseFile(handle) == 0);

	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);
	TEST_ASSERT(handle->rec_count == (RECORDS_NUM + 1 - TO_DELETE));

	int block_t;
	BF_Block *block;
	BF_Block_Init(&block);
	for (int i = 0; i < handle->buckets; ++i) {
		if ((block_t = handle->hash_table[i]) == -1)
			continue;
		
		size_t rec_count = 0;
		Hash_block block_handle;
		Dl_list_node node = list_last(buckets[i]);
		while (block_t != -1) {
			TEST_ASSERT(BF_GetBlock(handle->file_desc, block_t, block) == BF_OK);
			char *data = BF_Block_GetData(block);
			memcpy(&block_handle, data, sizeof(Hash_block));

			rec_count += block_handle.rec_num;
			data += sizeof(Record) * (block_handle.rec_num - 1) + sizeof(Hash_block);
			for (size_t j = block_handle.rec_num; j > 0; j--, data -= sizeof(Record)) {
				Record rec;
				memcpy(&rec, data, sizeof(Record));
				TEST_ASSERT(compare_records(&rec, list_value(node), ID, i));
				node = list_previous(node);
			}
			block_t = block_handle.overf_block;
			TEST_ASSERT(BF_UnpinBlock(block) == BF_OK);
		}
		TEST_ASSERT(list_size(buckets[i]) == rec_count);
	}
	BF_Block_Destroy(&block);

	for (size_t i = 0; i < handle->buckets; ++i)
		if (buckets[i] != NULL)
			list_destroy(buckets[i]);
	free(buckets);
	free(to_delete);


	TEST_ASSERT(HT_CloseFile(handle) == 0);
	TEST_ASSERT(BF_Close() == BF_OK);
	TEST_ASSERT(remove(FILENAME) == 0);

	HT_Close();
}


void test_find() 
{
	HT_Init();

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

	TEST_ASSERT(HT_CreateFile(FILENAME, SURNAME, BUCKETS) == 0);

	Hash_file *handle;
	TEST_ASSERT((handle = HT_OpenFile(FILENAME)) != NULL);

	for (int i = 0; i < array_size(rec_array); i++)
		TEST_ASSERT(INSERTED(handle, HT_InsertEntry(handle, rec_array[i], NULL)));

	Record find = { .id = -1 };
	for (int i = 0; i < array_size(rec_array); i++) {
		size_t bucket = hash_key(STRING, rec_array[i].surname) % BUCKETS;
		TEST_ASSERT(HT_GetEntry(handle, rec_array[i].surname, &find) == 0);
		TEST_ASSERT(compare_records(&find, &rec_array[i], SURNAME, bucket));
	}

	TEST_ASSERT(HT_GetEntry(handle, "Hilbert", &find) == 0);
	TEST_ASSERT(find.id == -1);

	TEST_ASSERT(HT_GetEntry(handle, "Wittgenstein", &find) == 0);
	TEST_ASSERT(find.id == -1);

	TEST_ASSERT(HT_CreateFile(FILENAME2, ID, BUCKETS) == 0);

	Hash_file *handle_;
	TEST_ASSERT((handle_ = HT_OpenFile(FILENAME2)) != NULL);

	int count_n = 0;
	int count_s = 0;
	Record rec = random_record();
	char *name = strdup(rec.name);
	char *surname = strdup(rec.surname);
	for (int i = 0; i < RECORDS_NUM; i++) {
		TEST_ASSERT(INSERTED(handle_, HT_InsertEntry(handle_, rec, NULL)));
		count_n += strcmp(name, rec.name) == 0;
		count_s += strcmp(surname, rec.surname) == 0;
		rec = random_record();
	}


	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle_, NAME, name, TMP_LIST)) == count_n);
	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle_, SURNAME, surname, TMP_LIST)) == count_s);

	int id = rec_array[rand() % array_size(rec_array)].id;
	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, ID, &id, TMP_LIST)) == 1);

	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, NAME, "Andrei", TMP_LIST)) == 2);
	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, CITY, "Moscow", TMP_LIST)) == 2);

	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle_, ID, &id, TMP_LIST)) == 1);
	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, NAME, name, TMP_LIST)) == 0);
	TEST_ASSERT(GET_NUM_ENTRIES(HT_GetAllEntries(handle, SURNAME, surname, TMP_LIST)) == 0);


	TEST_ASSERT(HT_CloseFile(handle_) == 0);
	TEST_ASSERT(HT_CloseFile(handle) == 0);
	TEST_ASSERT(remove(FILENAME) == 0);
	TEST_ASSERT(remove(FILENAME2) == 0);
	TEST_ASSERT(BF_Close() == BF_OK);

	free(name);
	free(surname);

	HT_Close();
}


TEST_LIST = {
    { "test_create", test_create },
    { "test_insert", test_insert },
    { "test_delete", test_delete },
    { "test_find",   test_find   },

    { NULL, NULL }
};
