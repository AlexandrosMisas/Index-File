#include "hash_file.h"
#include "shash_file.h"

#define RECORDS_CAPACITY (BF_BLOCK_SIZE - sizeof(Hash_block)) / sizeof(Record) 
#define BUCKETS_PER_BLOCK (BF_BLOCK_SIZE / sizeof(int))
#define HT_INFO_SIZE (sizeof(Hash_file) - sizeof(int*))

static size_t hash_strings(const void *key);
static int HT_FindEntry(Hash_file *handle, void *value, Record_pos *rec_pos, 
                                                        int *empty_block,
										                Record *rec);
static void update_data(char *data, char *action, void *value);

Hash_map file_map;

void HT_Init(void) 
{
	file_map = hash_map_create(0, (Comparator)strcmp, 
							      (Hash_func)hash_strings, 
								  free, NULL);
}


void HT_Close(void) 
{
	hash_map_destroy(file_map);
}


int HT_CreateFile(const char *filename, rec_attr attr, int buckets) 
{
	if (strlen(filename) > MAX_FILENAME) {
		fprintf(stderr,
			"Error! Filename exceeds the maximum length\n"
			"Maximum length = %d characters\n", 
			MAX_FILENAME
		);
		return -1;
	}


    int fd;
    CALL_BF(BF_CreateFile(filename), error);
    CALL_BF(BF_OpenFile(filename, &fd), delete_file);

    int last_block = 0, _buckets = buckets;
    BF_Block *block, *buckets_;

    BF_Block_Init(&block);
    BF_Block_Init(&buckets_);
    CALL_BF(BF_AllocateBlock(fd, block), bf_cleanup);

    do {
        last_block++;
        CALL_BF(BF_AllocateBlock(fd, buckets_), bf_cleanup);
        _buckets -= BUCKETS_PER_BLOCK;
        memset(BF_Block_GetData(buckets_), -1, BF_BLOCK_SIZE);
        BF_Block_SetDirty(buckets_);
        CALL_BF(BF_UnpinBlock(buckets_), bf_cleanup);
    } while (_buckets > 0);


    Hash_file handle = {
        .buckets       = buckets,
        .rec_capacity  = RECORDS_CAPACITY,
        .attr          = attr,
        .last_block_id = last_block,
		.file_type     = "hash"
    };


	for (rec_attr attr_ = NAME; attr_ <= CITY; attr_++) {
		memset(handle.index_files[attr_ - 1].filename, 0, MAX_FILENAME + 1);
		handle.index_files[attr_ - 1].attr = attr_;
	}

	COPY(filename, handle.filename, strlen(filename), MAX_FILENAME + 1);
    COPY(&handle, BF_Block_GetData(block), HT_INFO_SIZE, BF_BLOCK_SIZE);
    BF_Block_SetDirty(block);

    CALL_BF(BF_UnpinBlock(block), bf_cleanup);
    BF_Block_Destroy(&block);
    BF_Block_Destroy(&buckets_);
    CALL_BF(BF_CloseFile(fd), error);
	
    return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
	    BF_Block_Destroy(&buckets_);
		CALL_BF(BF_CloseFile(fd), error);

	delete_file:
		if (remove(filename) == -1)
			fprintf(stderr, "%s\n", strerror(errno));

	error:
		return -1;
}

Hash_file *HT_OpenFile(const char *filename) 
{
    int fd;
	CALL_BF(BF_OpenFile(filename, &fd), error);

    BF_Block *metadata_block;
    BF_Block *buckets_block;

    BF_Block_Init(&buckets_block);
    BF_Block_Init(&metadata_block);

	CALL_BF(BF_GetBlock(fd, 0, metadata_block), bf_cleanup);
    char *data = BF_Block_GetData(metadata_block);

    if (strncmp(data, "hash", strlen("hash") + 1)) {
        fprintf(stderr, 
            "Error! "
            "No proper hashfile was given\n"
            "Exiting...\n"
        );
		CALL_BF(BF_UnpinBlock(metadata_block), bf_cleanup);
    }

    Hash_file *handle = malloc(sizeof(*handle));
    memcpy(handle, BF_Block_GetData(metadata_block), HT_INFO_SIZE);
    handle->file_desc = fd;


    handle->hash_table = malloc(sizeof(int) * handle->buckets);
    int buckets = handle->buckets;
    for (int i = 1; i <= handle->last_block_id; ++i) {
	    CALL_BF(BF_GetBlock(fd, i, buckets_block), bf_cleanup);
        memcpy(
        	handle->hash_table + (i - 1) * BUCKETS_PER_BLOCK, 
            BF_Block_GetData(buckets_block), 
            buckets >= BUCKETS_PER_BLOCK 
                ? BF_BLOCK_SIZE 
                : buckets * sizeof(int)
        );
        buckets -= BUCKETS_PER_BLOCK;
		CALL_BF(BF_UnpinBlock(buckets_block), bf_cleanup);
    }

	hash_map_insert(file_map, strdup(handle->filename), handle);
    BF_Block_Destroy(&buckets_block);
	BF_Block_Destroy(&metadata_block);
    return handle;

	bf_cleanup:
		BF_Block_Destroy(&buckets_block);
		BF_Block_Destroy(&metadata_block);

	error:
		return NULL;
}

int HT_CloseFile(Hash_file *handle) 
{
    BF_Block *block;
    BF_Block_Init(&block);

    CALL_BF(BF_GetBlock(handle->file_desc, 0, block), bf_cleanup);
    char *data = BF_Block_GetData(block);

    memcpy(
        data + offsetof(Hash_file, rec_count),
        &handle->rec_count,
        sizeof_field(Hash_file, rec_count)
    );

	memcpy(
		data + offsetof(Hash_file, index_files),
		handle->index_files,
		sizeof_field(Hash_file, index_files)
	);


    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block), bf_cleanup);

    for (int i = 1; i <= handle->last_block_id; ++i) {
        CALL_BF(BF_GetBlock(handle->file_desc, i, block), bf_cleanup);
        memcpy(
            BF_Block_GetData(block),
            handle->hash_table + (i - 1) * BUCKETS_PER_BLOCK,
            i != handle->last_block_id 
                ? BF_BLOCK_SIZE 
                : sizeof(int) * handle->buckets
        );
        handle->buckets -= BUCKETS_PER_BLOCK;
        BF_Block_SetDirty(block);
        CALL_BF(BF_UnpinBlock(block), bf_cleanup);
    }

    BF_Block_Destroy(&block);
    CALL_BF(BF_CloseFile(handle->file_desc), error);
	hash_map_delete(file_map, handle->filename);
    free(handle->hash_table);
    free(handle);

    return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		CALL_BF(BF_CloseFile(handle->file_desc), error);
	error:
		hash_map_delete(file_map, handle->filename);
		free(handle->hash_table);
		free(handle);
		return -1;
}


int HT_InsertEntry(Hash_file *handle, Record record, int *block_id) 
{
	int empty_block = -1;
	Record_pos tmp_pos = { .block_id = -1 };
	void *value = get_rec_member(&record, handle->attr);
	BF_Block* block;


	if (HT_FindEntry(handle, value, &tmp_pos, &empty_block, NULL)) {
		if (block_id != NULL)
			*block_id = tmp_pos.block_id;

		return tmp_pos.block_id < 0 ? -1 : 0;
	}

	BF_Block_Init(&block);
	if (empty_block > 0)
		CALL_BF(
			BF_GetBlock(
				handle->file_desc,
				empty_block,
				block
			),
			error
		);
	else 
		CALL_BF(
			BF_AllocateBlock(
				handle->file_desc,
				block
			),
			error
		);
	

	char *data = BF_Block_GetData(block);
	int bucket = hash_key(get_attr_type(handle->attr), value) % handle->buckets;
	if (empty_block < 0) {
		Hash_block block_data = { 
			.overf_block = handle->hash_table[bucket],
			.rec_num = 0
		};
		CALL_BF(
			BF_GetBlockCounter(
				handle->file_desc,
				&handle->hash_table[bucket]
			),
			unpin
		);
		--handle->hash_table[bucket];
		memcpy(data, &block_data, sizeof(Hash_block));
	}
	update_data(data, "insert", &record);

	if (block_id != NULL)
		*block_id = empty_block < 0 
			? handle->hash_table[bucket]
			: empty_block;

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), error);
	BF_Block_Destroy(&block);
	handle->rec_count++;

	return 0;

	unpin:
		CALL_BF(BF_UnpinBlock(block), error);

	error:
		BF_Block_Destroy(&block);
		return -1;
}

int HT_DeleteEntry(Hash_file *handle, void *value) 
{
	int code;
	BF_Block *block;
	Record_pos rec_pos = { .block_id = -1 };
	Record rec;

	if ((code = HT_FindEntry(handle, value, &rec_pos, NULL, &rec)) <= 0)
		return code;


	BF_Block_Init(&block);
	CALL_BF(
		BF_GetBlock(
			handle->file_desc,
			rec_pos.block_id,
			block	
		),
		bf_cleanup
	);

	update_data(
		BF_Block_GetData(block),
		"delete",
		&rec_pos.pos
	);

	for (rec_attr attr_ = NAME; attr_ <= CITY; ++attr_) {
		if (strcmp("", handle->index_files[attr_ - 1].filename)) {
			Map_tuple tuple = hash_map_value(
				file_map, 
				handle->index_files[attr_ - 1].filename
			);

			SHash_file *shandle = tuple != NULL
				? (SHash_file*)map_tuple_value(tuple)
				: SHT_OpenFile(handle->index_files[attr_ - 1].filename);

			if (shandle == NULL)
				goto bf_cleanup;

			SHT_DeleteEntry(
				shandle, 
				get_rec_member(&rec, attr_), 
				rec_pos.block_id
			);
			if (tuple == NULL && SHT_CloseFile(shandle) < 0)
				goto bf_cleanup;
		}
	}

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);
	
	handle->rec_count--;
	return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;
}

int HT_GetEntry(Hash_file *handle, void *value, Record *rec) 
{
	int code = HT_FindEntry(handle, value, NULL, NULL, rec);
	return code < 0 ? -1 : 0;
}

int HT_GetAllEntries(Hash_file *handle, rec_attr attr, 
										void *value, 
										Dl_list records) 
{
	if (handle->attr == attr) {
		Record rec = {};
		int code = HT_GetEntry(handle, value, &rec);

		if (code < 0 || rec.id < 0)
			return code < 0 ? -1 : 0;

		Record *rec_ = malloc(sizeof(*rec_));
		list_insert(records, memcpy(rec_, &rec, sizeof(*rec_)));
		return 0; 
	}


	int offset = get_attr_offset(attr);
	int size = get_attr_type(attr) == STRING
		? strlen(value) + 1
		: get_attr_size(attr);

	BF_Block *block;
	Hash_block block_data;
	BF_Block_Init(&block);
	for (int i = 0; i < handle->buckets; ++i) {
		int block_t = handle->hash_table[i];
		while (block_t != -1) {
			CALL_BF(
				BF_GetBlock(
					handle->file_desc,
					block_t,
					block
				),
				error
			);
			char *data = BF_Block_GetData(block);
			memcpy(&block_data, data, sizeof(Hash_block));
			data += sizeof(Hash_block);

			for (int j = 0; j < block_data.rec_num; j++, data += sizeof(Record)) {
				if (memcmp(data + offset, value, size) == 0) {
					Record *tmp = malloc(sizeof(*tmp));
					list_insert(records, memcpy(tmp, data, sizeof(*tmp)));
				}
			}
			block_t = block_data.overf_block;
			CALL_BF(BF_UnpinBlock(block), error);
		}
	}
	BF_Block_Destroy(&block);
	return 0;

	error:
		BF_Block_Destroy(&block);
		return -1;

}

int HT_PrintFile(Hash_file *handle, FILE *stream) 
{
	BF_Block *block;
	BF_Block_Init(&block);

	for (int i = 0; i < handle->buckets; i++) {
		int block_t = handle->hash_table[i];

		if (block_t > 0)
			fprintf(stream, "Records in %d bucket\n\n", i);
		

		while (block_t != -1) {
			CALL_BF(
				BF_GetBlock(
					handle->file_desc, 
					block_t, 
					block
				),
				error
			);
			char *data = BF_Block_GetData(block);
			int rec_num;

			memcpy(
				&rec_num,
				data + offsetof(Hash_block, rec_num),
				sizeof_field(Hash_block, rec_num)
			);
			memcpy(
				&block_t,
				data + offsetof(Hash_block, overf_block),
				sizeof_field(Hash_block, overf_block)
			);

			data += sizeof(Hash_block);
			for (int j = 0; j < rec_num; j++, data += sizeof(Record)) {
				Record rec;
				memcpy(&rec, data, sizeof(Record));
				fprintf(stream,
					"Id: %d\n"
					"Name: %s\n"
					"Surname: %s\n"
					"City: %s\n\n",
					rec.id, rec.name, 
					rec.surname, rec.city
				);				
			}
			CALL_BF(BF_UnpinBlock(block), error);
		}
	}
	BF_Block_Destroy(&block);
	
	return 0;

	error:
		BF_Block_Destroy(&block);
		return -1;
}


static int HT_FindEntry(Hash_file *handle, void *value, Record_pos *rec_pos, 
                                                        int *empty_block,
													    Record *rec) 
{
	int bucket = hash_key(get_attr_type(handle->attr), value) % handle->buckets;
	int block_t = handle->hash_table[bucket];
	bool found = false;
	Hash_block block_data;
	BF_Block *block;


	int offset = get_attr_offset(handle->attr);
	int size = get_attr_type(handle->attr) == STRING
		? strlen(value) + 1
		: get_attr_size(handle->attr);

	BF_Block_Init(&block);
	while (block_t != -1) {
		CALL_BF(
			BF_GetBlock(
				handle->file_desc,
				block_t,
				block
			),
			error	
		);
		char *data = BF_Block_GetData(block);
		memcpy(&block_data, data, sizeof(Hash_block));

		if (empty_block != NULL && *empty_block < 0
		 && block_data.rec_num != handle->rec_capacity)
		 	*empty_block = block_t;

		data += sizeof(Hash_block);
		for (int i = 0; i < block_data.rec_num; i++, data += sizeof(Record)) {
			if (memcmp(data + offset, value, size) == 0) {
				found = true;
				if (rec_pos != NULL) {
					rec_pos->block_id = block_t;
					rec_pos->pos = i;
				}
				if (rec != NULL)
					memcpy(rec, data, sizeof(Record));
				break;
			}
		}
		CALL_BF(BF_UnpinBlock(block), error);
		if (found) {
			BF_Block_Destroy(&block);
			return 1;
		}
		block_t = block_data.overf_block;
	}
	BF_Block_Destroy(&block);

	if (rec != NULL)
		rec->id = -1;

	return 0;

	error:
		BF_Block_Destroy(&block);
		return -1;
}

static void update_data(char *data, char *action, void *value) 
{
	int rec_num, new;
	bool is_delete = !strcmp(action, "delete");
	
	memcpy(
		&rec_num, 
		data + offsetof(Hash_block, rec_num),
		sizeof_field(Hash_block, rec_num)
	);
	
	new = rec_num + (is_delete ? -1 : 1);
	memcpy(
		data + offsetof(Hash_block, rec_num), 
		&new, 
		sizeof_field(Hash_block, rec_num)
	);
	
	int offset = is_delete ? *(int*)value : rec_num;
	data += offset * sizeof(Record) + sizeof(Hash_block);

	if (!is_delete)
		memcpy(data, value, sizeof(Record));
	else
		memmove(
			data,
			data + sizeof(Record),
			(rec_num - offset - 1) * sizeof(Record) 
		);
}


static size_t hash_ints(const void *key) 
{
    size_t value = *(int*)key;
    value = ((value >> 16) ^ value) * 0x45d9f3b;
    value = ((value >> 16) ^ value) * 0x45d9f3b;
    value = ((value >> 16) ^ value);
    return value;
}

static size_t hash_strings(const void *key) 
{
    size_t hash = 5381;
    size_t c;
    char *s = (char*)key;
    while ((c = *s++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

size_t hash_key(attr_type type, const void *key) 
{
    return type == INT ? hash_ints(key) : hash_strings(key);
}



