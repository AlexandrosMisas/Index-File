#include "heap_file.h"

#define RECORDS_CAPACITY (BF_BLOCK_SIZE - sizeof(int)) / sizeof(Record)


static int HP_FindEntry(Heap_file *handle, void *value, Record_pos *rec_pos, int *empty_block);
static void update_data(char *data, char *action, void *value);


int HP_CreateFile(const char *filename, rec_attr attr) 
{
	int fd;
	CALL_BF(BF_CreateFile(filename), error);
	CALL_BF(BF_OpenFile(filename, &fd), delete_file);

	BF_Block *block;
	BF_Block_Init(&block);

	CALL_BF(BF_AllocateBlock(fd, block), bf_cleanup);

	Heap_file handle = {
		.rec_capacity = RECORDS_CAPACITY,
		.attr         = attr,
		.file_type    = "heap" 
	};
	
	COPY(
		&handle, 
		BF_Block_GetData(block), 
		sizeof(handle), 
		BF_BLOCK_SIZE
	);

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);

	CALL_BF(BF_CloseFile(fd), error);
	return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		CALL_BF(BF_CloseFile(fd), delete_file);

	delete_file:
		if (remove(filename) == -1)
			fprintf(stderr, "%s\n", strerror(errno));
	error:
		return -1;
}

Heap_file *HP_OpenFile(const char *filename) 
{
	int fd;
	CALL_BF(BF_OpenFile(filename, &fd), error);

	BF_Block *block;
	BF_Block_Init(&block);

	CALL_BF(BF_GetBlock(fd, 0, block), bf_cleanup);
	char *data = BF_Block_GetData(block);

	if (strncmp(data, "heap", strlen("heap") + 1)) {
		fprintf(stderr, 
			"Error! "
			"No proper heapfile was given\n"
			"Exiting...\n"
		);
		CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	}

	Heap_file *handle = malloc(sizeof(*handle));
	memcpy(handle, data, sizeof(*handle));
	handle->file_desc = fd;
	
	BF_Block_Destroy(&block);
	return handle;

	bf_cleanup:
		BF_Block_Destroy(&block);
		CALL_BF(BF_CloseFile(fd), error);

	error:
		return NULL;
}

int HP_CloseFile(Heap_file *handle) 
{
	BF_Block *block;
	BF_Block_Init(&block);

	CALL_BF(BF_GetBlock(handle->file_desc, 0, block), bf_cleanup);
	char *data = BF_Block_GetData(block);

	memcpy(
		data + offsetof(Heap_file, last_block_id), 
		&handle->last_block_id, 
		sizeof_field(Heap_file, last_block_id)
	);

	memcpy(
		data + offsetof(Heap_file, rec_count),
		&handle->rec_count, 
		sizeof_field(Heap_file, rec_count)
	);


	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);

	CALL_BF(BF_CloseFile(handle->file_desc), error);
	free(handle);
	return 0;


	bf_cleanup:
		BF_Block_Destroy(&block);
		CALL_BF(BF_CloseFile(handle->file_desc), error);

	error:
		free(handle);
		return -1;
}



int HP_InsertEntry(Heap_file *handle, Record rec) 
{
	BF_Block *block;
	int empty_block = -1;
	Record_pos rec_pos = { .block_id = -1 };
	void *value = get_rec_member(&rec, handle->attr);

	if (HP_FindEntry(handle, value, &rec_pos, &empty_block))
		return rec_pos.block_id < 0 ? -1 : 0;

	BF_Block_Init(&block);
	if (empty_block < 0) {
		CALL_BF(
			BF_AllocateBlock(
				handle->file_desc, 
				block
			), 
			bf_cleanup
		);
		handle->last_block_id++;
	} else {
		CALL_BF(
			BF_GetBlock(
				handle->file_desc, 
				empty_block, 
				block
			), 
			bf_cleanup
		);
	}
	update_data(
		BF_Block_GetData(block), 
		"insert", 
		&rec
	);
	
	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);

	handle->rec_count++;
	return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;
}

int HP_DeleteEntry(Heap_file *handle, void *value) 
{
	int code;
	BF_Block *block;
	Record_pos rec_pos = { .block_id = -1 };

	if ((code = HP_FindEntry(handle, value, &rec_pos, NULL)) <= 0) 
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
	
	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);

	handle->rec_count--;
	return 0;


	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;

}



int HP_GetAllEntries(Heap_file *handle, rec_attr attr, void *value, Dl_list records) 
{

	if (handle->attr == attr) {
		Record tmp = { .id = -1 };
		int code = HP_GetEntry(handle, value, &tmp);

		if (code < 0 || tmp.id < 0)
			return code < 0 ? -1 : 0;

		Record *rec = malloc(sizeof(*rec));
		list_insert(records, memcpy(rec, &tmp, sizeof(*rec)));
		return 0;
	}

	int rec_num = 0;
	int offset = get_attr_offset(attr);
	int size = get_attr_type(attr) == STRING 
		? strlen(value) + 1
		: get_attr_size(attr);


	BF_Block *block;
	BF_Block_Init(&block);
	for (int i = 1; i <= handle->last_block_id; i++) {
		CALL_BF(
			BF_GetBlock(
				handle->file_desc, 
				i, 
				block
			), 
			bf_cleanup
		);
		
		char *data = BF_Block_GetData(block);
		memcpy(&rec_num, data, sizeof(int));
		data += sizeof(int);
		for (int j = 0; j < rec_num; j++, data += sizeof(Record)) {
			if (memcmp(data + offset, value, size) == 0) {
				Record *tmp = malloc(sizeof(*tmp));
				list_insert(records, memcpy(tmp, data, sizeof(*tmp)));
			}
		}
		CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	}
	BF_Block_Destroy(&block);
	return 0;


	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;
}


int HP_GetEntry(Heap_file *handle, void *value, Record *rec) 
{
	Record_pos rec_pos = { .block_id = -1 };
	int code;
	
	if ((code = HP_FindEntry(handle, value, &rec_pos, NULL)) <= 0)
		return (rec->id = -1, code);

	BF_Block *block;
	BF_Block_Init(&block);

	CALL_BF(
		BF_GetBlock(
			handle->file_desc, 
			rec_pos.block_id, 
			block
		), 
		error
	);

	char *data = BF_Block_GetData(block)      + 
				 rec_pos.pos * sizeof(Record) +
				 sizeof(int);
	
	memcpy(rec, data, sizeof(Record));

	CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	BF_Block_Destroy(&block);

	return 0;


	bf_cleanup:
		BF_Block_Destroy(&block);

	error:
		return -1;
}




int HP_PrintFile(Heap_file *handle, FILE *stream) 
{
	BF_Block *block;
	BF_Block_Init(&block);

	for (int i = 1; i <= handle->last_block_id; i++) {
		CALL_BF(
			BF_GetBlock(
				handle->file_desc, 
				i, 
				block
			), 
			bf_cleanup
		);
		char *data = BF_Block_GetData(block);
		
		int rec_num;
		memcpy(&rec_num, data, sizeof(int));
		data += sizeof(int);

		if (rec_num > 0)
			fprintf(stream, "Records in %d block\n", i);

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
		CALL_BF(BF_UnpinBlock(block), bf_cleanup);
	}
	BF_Block_Destroy(&block);
	return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;
}




static int HP_FindEntry(Heap_file *handle, void *value, 
										   Record_pos *rec_pos, 
										   int *empty_block) 
{
	int rec_num = 0;
	bool found = false;
	
	BF_Block *block;
	BF_Block_Init(&block);

	int offset = get_attr_offset(handle->attr);
	int size = get_attr_type(handle->attr) == STRING
		? strlen(value) + 1
		: get_attr_size(handle->attr);

	for (int i = 1; i <= handle->last_block_id; i++) {
		CALL_BF(
			BF_GetBlock(
				handle->file_desc, 
				i, 
				block
			), 
			bf_cleanup
		);
		char *data = BF_Block_GetData(block);
		memcpy(&rec_num, data, sizeof(int));
		
		if (empty_block != NULL && *empty_block < 0
		 && rec_num != handle->rec_capacity)
			*empty_block = i;
			
		data += sizeof(int);	
		for (int j = 0; j < rec_num; j++, data += sizeof(Record)) {
			if (memcmp(data + offset, value, size) == 0) {
				found = true;
				rec_pos->block_id = i;
				rec_pos->pos = j;
				break;
			}
		}
		CALL_BF(BF_UnpinBlock(block), bf_cleanup);

		if (found) {
			BF_Block_Destroy(&block);
			return 1;
		}
	}
	BF_Block_Destroy(&block);
	return 0;

	bf_cleanup:
		BF_Block_Destroy(&block);
		return -1;
}



static void update_data(char *data, char *action, void *value) 
{
	int rec_num, new;
	bool is_delete = !strcmp(action, "delete");
	
	memcpy(&rec_num, data, sizeof(int));
	new = rec_num + (is_delete ? -1 : 1);
	memcpy(data, &new, sizeof(int));

	int offset = is_delete ? *(int*)value : rec_num;
	data += sizeof(int) + offset * sizeof(Record);

	if (!is_delete)
		memcpy(data, value, sizeof(Record));
	else
		memmove(
			data,
			data + sizeof(Record),
			(rec_num - offset - 1) * sizeof(Record)
		);
}
