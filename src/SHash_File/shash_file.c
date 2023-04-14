#include "shash_file.h"

#define RECORDS_CAPACITY (BF_BLOCK_SIZE - sizeof(SHash_block)) / sizeof(SRecord)
#define BUCKETS_PER_BLOCK (BF_BLOCK_SIZE / sizeof(int))
#define SHT_INFO_SIZE (sizeof(SHash_file) - sizeof(int*))

static int SHT_FindEntry(SHash_file *handle, SRecord rec, Record_pos *rec_pos, 
													      int *empty_block,
													      int *counter);

static int SHT_GetHTRecords(SHash_file *handle, Hash_file *ht_handle, 
												int block_id, 
												void *value, 
												Dl_list records);

static void update_data(char *data, char *action, void *value, bool is_dup);



int SHT_CreateFile(const char *sfilename, rec_attr attr,
										  const char *filename,
										  int buckets) 
{
	if (attr == ID) {
		fprintf(stderr, "Not a proper attribute was chosen\n");
		return -1;
	}

	if (strlen(sfilename) > MAX_FILENAME) {
		fprintf(stderr,
			"Error! Filename exceeds the maximum length\n"
			"Maximum length = %d characters\n", 
			MAX_FILENAME
		);		
		return -1;
	}

    int fd;
    CALL_BF(BF_CreateFile(sfilename), error);
    CALL_BF(BF_OpenFile(sfilename, &fd), delete_file);

	int last_block = 0, _buckets = buckets;
    BF_Block *block, *buckets_block;

    BF_Block_Init(&block);
    BF_Block_Init(&buckets_block);
    CALL_BF(BF_AllocateBlock(fd, block), bf_cleanup);

    do {
        last_block++;
        CALL_BF(BF_AllocateBlock(fd, buckets_block), bf_cleanup);
        _buckets -= BUCKETS_PER_BLOCK;
        memset(BF_Block_GetData(buckets_block), -1, BF_BLOCK_SIZE);
        BF_Block_SetDirty(buckets_block);
        CALL_BF(BF_UnpinBlock(buckets_block), bf_cleanup);
    } while (_buckets > 0);


    SHash_file handle = {
        .buckets       = buckets,
        .rec_capacity  = RECORDS_CAPACITY,
        .attr          = attr,
        .last_block_id = last_block,
		.file_type     = "sht"
    };

	
	Map_tuple tuple = hash_map_value(file_map, (void*)filename);
	Hash_file *ht_handle = tuple != NULL
		? (Hash_file*)map_tuple_value(tuple) 
		: HT_OpenFile(filename);
	
	if (ht_handle == NULL)
		goto bf_cleanup;

	COPY(sfilename, ht_handle->index_files[attr - 1].filename, strlen(sfilename), MAX_FILENAME + 1);
    COPY(sfilename, handle.filename, strlen(sfilename), MAX_FILENAME + 1);
	COPY(ht_handle->filename, handle.index_filename, strlen(ht_handle->filename), MAX_FILENAME + 1);
    COPY(&handle, BF_Block_GetData(block), SHT_INFO_SIZE, BF_BLOCK_SIZE);

	if (tuple == NULL && HT_CloseFile(ht_handle) < 0)
		goto bf_cleanup;

    
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block), bf_cleanup);
    BF_Block_Destroy(&block);
    BF_Block_Destroy(&buckets_block);
    CALL_BF(BF_CloseFile(fd), error);

    return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
        BF_Block_Destroy(&buckets_block);
		CALL_BF(BF_CloseFile(fd), delete_file);

    delete_file:
        if (remove(sfilename) == -1)
            fprintf(stderr, "%s\n", strerror(errno));
    error:
        return -1;
	
}

SHash_file *SHT_OpenFile(const char *sfilename) 
{
	int fd;
	CALL_BF(BF_OpenFile(sfilename, &fd), error);

    BF_Block *block, *buckets_block;
    BF_Block_Init(&block);
    BF_Block_Init(&buckets_block);


	CALL_BF(BF_GetBlock(fd, 0, block), bf_cleanup);
    char *data = BF_Block_GetData(block);


    if (strncmp(data, "sht", strlen("sht") + 1)) {
        fprintf(stderr, 
            "Error! "
            "No proper shashfile was given\n"
            "Exiting...\n"
        );
		CALL_BF(BF_UnpinBlock(block), bf_cleanup);
    }

    SHash_file *handle = malloc(sizeof(*handle));
    memcpy(handle, BF_Block_GetData(block), SHT_INFO_SIZE);
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
	BF_Block_Destroy(&block);

    return handle;

	bf_cleanup:
		BF_Block_Destroy(&block);
		BF_Block_Destroy(&buckets_block);
		CALL_BF(BF_CloseFile(fd), error);

	error:
		return NULL;
}

int SHT_CloseFile(SHash_file *handle) 
{
	BF_Block *block;
    BF_Block_Init(&block);

    CALL_BF(BF_GetBlock(handle->file_desc, 0, block), bf_cleanup);
    char *data = BF_Block_GetData(block);

    memcpy(
		data + offsetof(SHash_file, rec_count),
        &handle->rec_count,
        sizeof_field(SHash_file, rec_count)
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
		free(handle->hash_table);
		free(handle);
		return -1;
}


int SHT_InsertEntry(SHash_file *handle, Record record, int block_id) 
{
	int empty_block = -1;
	Record_pos tmp_pos = { .block_id = -1 };
	void *value  = get_rec_member(&record, handle->attr);
	SRecord srec = create_srecord(value, block_id, handle->attr);

	if (SHT_FindEntry(handle, srec, &tmp_pos, &empty_block, NULL) < 0)
		return -1;

	BF_Block* block;
	BF_Block_Init(&block);

	if (empty_block < 0 && tmp_pos.block_id < 0)
		CALL_BF(
			BF_AllocateBlock(
				handle->file_desc,
				block
			),
			error
		);
	else 
		CALL_BF(
			BF_GetBlock(
				handle->file_desc,
				empty_block < 0
					? tmp_pos.block_id
					: empty_block,
				block
			),
			error
		);
	

	char *data = BF_Block_GetData(block);
	if (empty_block < 0 && tmp_pos.block_id < 0) {
		int bucket = hash_key(get_attr_type(handle->attr), value) % handle->buckets;
		SHash_block block_data = { 
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
		memcpy(data, &block_data, sizeof(SHash_block));
	}
	update_data(
		data, 
		"insert",
		tmp_pos.block_id < 0
			? (void*)&srec  
			: (void*)&tmp_pos.pos,
		tmp_pos.block_id > 0
	);

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), error);
	BF_Block_Destroy(&block);
	handle->rec_count += tmp_pos.block_id < 0;

	return 0;

	unpin:
		CALL_BF(BF_UnpinBlock(block), error);

	error:
		BF_Block_Destroy(&block);
		return -1;
}

int SHT_DeleteEntry(SHash_file *handle, void *value, int block_id) 
{
	int counter, code;
	Record_pos rec_pos = { .block_id = -1 };
	SRecord rec = create_srecord(value, block_id, handle->attr);

	if ((code = SHT_FindEntry(handle, rec, &rec_pos, NULL, &counter)) <= 0)
		return code;

	BF_Block *block;
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
		&rec_pos.pos,
		counter != 1
	);

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);
	
	handle->rec_count -= counter == 1;
	return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;
}


int SHT_GetEntries(SHash_file *handle, void *value, Dl_list records) 
{
	int bucket = hash_key(get_attr_type(handle->attr), value) % handle->buckets;
	int size = get_attr_type(handle->attr) == STRING 
		? strlen(value) + 1 
		: get_attr_size(handle->attr);

	BF_Block *block;
	BF_Block_Init(&block);

	Map_tuple tuple = hash_map_value(file_map, handle->index_filename);
	Hash_file *ht_handle = tuple != NULL 
		? (Hash_file*)map_tuple_value(tuple) 
		: HT_OpenFile(handle->index_filename);
	
	if (ht_handle == NULL)
		goto  error;

	int block_t = handle->hash_table[bucket];
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
		SHash_block block_data;
		memcpy(&block_data, data, sizeof(SHash_block));
		data += sizeof(SHash_block);
		for (int i = 0; i < block_data.rec_num; i++, data += sizeof(SRecord)) {
			SRecord srec;
			memcpy(&srec, data, sizeof(SRecord));
			void *value_ = get_attr_type(handle->attr) == STRING
				? (void*)srec.key.skey
				: (void*)&srec.key.ikey;

			
			if (memcmp(value_, value, size) == 0) {
				int code = SHT_GetHTRecords(handle, ht_handle, 
											srec.block_id, value, 
											records);
				if (code < 0)
					goto unpin;
			}
		}
		block_t = block_data.overf_block;
		CALL_BF(BF_UnpinBlock(block), error);	
	}
	
	if (tuple == NULL && HT_CloseFile(ht_handle) < 0)
		goto error;

	BF_Block_Destroy(&block);
	return 0;

	unpin:
		BF_UnpinBlock(block);
		if (tuple == NULL)
			HT_CloseFile(ht_handle);
	error:
		BF_Block_Destroy(&block);
		return -1;
}


static int SHT_FindEntry(SHash_file *handle, SRecord rec, Record_pos *rec_pos, 
											 			  int *empty_block, 
											              int *counter) 
{
	BF_Block *block;
	void *value = get_attr_type(handle->attr) == INT 
		? (void*)&rec.key.ikey 
		: (void*)rec.key.skey;

	int size = get_attr_type(handle->attr) == STRING 
		? strlen(value) + 1 
		: get_attr_size(handle->attr);

	int bucket = hash_key(get_attr_type(handle->attr), value) % handle->buckets;
	int block_t = handle->hash_table[bucket];
	bool found = false;
	SHash_block block_data;

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
		memcpy(&block_data, data, sizeof(SHash_block));
		data += sizeof(SHash_block);

		if (empty_block != NULL && *empty_block < 0
		 && block_data.rec_num != handle->rec_capacity)
		 	*empty_block = block_t;

		for (int i = 0; i < block_data.rec_num; i++, data += sizeof(SRecord)) {
			SRecord rec_;
			memcpy(&rec_, data, sizeof(SRecord));
			void *value_ = get_attr_size(handle->attr) == STRING
				? (void*)rec_.key.skey
				: (void*)&rec_.key.ikey;

			if (memcmp(value_, value, size) == 0
			 && rec_.block_id == rec.block_id) {
				found = true;
				if (rec_pos != NULL) {
					rec_pos->block_id = block_t;
					rec_pos->pos = i;
				}
				if (counter != NULL)
					*counter = rec_.counter;
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
	return 0;

	error:
		BF_Block_Destroy(&block);
		return -1;
}

static int SHT_GetHTRecords(SHash_file *handle, Hash_file *ht_handle, 
												int block_id, void *value, 
												Dl_list records) 
{
	BF_Block *block;
	BF_Block_Init(&block);
	CALL_BF(BF_GetBlock(ht_handle->file_desc, block_id, block), error);
	char *data = BF_Block_GetData(block);
	Hash_block block_data;
	memcpy(&block_data, data, sizeof(Hash_block));
	data += sizeof(Hash_block);
	
	int offset = get_attr_offset(handle->attr);
	int size = get_attr_type(handle->attr) == STRING 
		? strlen(value) + 1
		: get_attr_size(handle->attr);

	for (int j = 0; j < block_data.rec_num; j++, data += sizeof(Record)) {
		if (memcmp(data + offset, value, size) == 0) {
			Record *rec = malloc(sizeof(*rec));
			list_insert(records, memcpy(rec, data, sizeof(*rec)));
		}
	}
	CALL_BF(BF_UnpinBlock(block), error);

	BF_Block_Destroy(&block);
	return 0;

	error:
		BF_Block_Destroy(&block);
		return -1;

}


static void update_data(char *data, char *action, void *value, bool is_dup) 
{
	int rec_num, new;
	bool is_delete = strcmp(action, "delete") == 0;

	if (is_dup)
		goto write_data;

	memcpy(
		&rec_num,
		data + offsetof(SHash_block, rec_num),
		sizeof_field(SHash_block, rec_num)
	);

	new = rec_num + (is_delete ? -1 : 1);
	memcpy(
		data + offsetof(SHash_block, rec_num),
		&new,
		sizeof_field(SHash_block, rec_num)
	);


	write_data: {
		int offset = is_dup || is_delete ? *(int*)value : rec_num;
		data += offset * sizeof(SRecord) + sizeof(SHash_block);
	
		if (is_dup) {
			SRecord rec;
			memcpy(&rec, data, sizeof(SRecord));
			rec.counter += is_delete ? -1 : 1;
			memcpy(data, &rec, sizeof(SRecord));
		} else if (!is_delete) {
			memcpy(data, value, sizeof(SRecord));
		} else {
			memmove(
				data,
				data + sizeof(SRecord),
				(rec_num - offset - 1) * sizeof(SRecord)
			);
		}
	}
}