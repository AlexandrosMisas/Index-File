#ifndef RECORD_H
#define RECORD_H

#define INDEX_ATTR 3



typedef struct {
	int block_id;
	int pos;
} Record_pos;

typedef struct __attribute__((__packed__)) {
	int id;
	char name[15];
	char surname[20];
	char city[20];
} Record;

typedef struct __attribute__((__packed__)) {
	int counter;
	int block_id;
	union {
		int  ikey;
		char skey[20];
	} key;
} SRecord;

typedef enum {
    ID = 0,
    NAME,
    SURNAME,
    CITY
} rec_attr;

typedef enum {
	INT,
	STRING
} attr_type;



Record random_record(void);

SRecord create_srecord(void *value, int block_id, rec_attr attr);

void print_record(Record *rec);

int get_attr_offset(rec_attr attr);

int get_attr_size(rec_attr attr);

attr_type get_attr_type(rec_attr attr);

void *get_rec_member(Record *rec, rec_attr attr);

#endif /* RECORD_H */
