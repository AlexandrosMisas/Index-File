#include "record.h"
#include "common.h"

const char *names[] = {
	"Yannis",
	"Christofos",
	"Sofia",
	"Marianna",
	"Vagelis",
	"Maria",
	"Iosif",
	"Dionisis",
	"Konstantina",
	"Theofilos",
	"Giorgos",
	"Dimitris"
};

const char *surnames[] = {
	"Ioannidis",
	"Svingos",
	"Karvounari",
	"Rezkalla",
	"Nikolopoulos",
	"Berreta",
	"Koronis",
	"Gaitanis",
	"Oikonomou",
	"Mailis",
	"Michas",
	"Halatsis"
};

const char *cities[] = {
	"Athens",
	"San Francisco",
	"Los Angeles",
	"Amsterdam",
	"London",
	"New York",
	"Tokyo",
	"Hong Kong",
	"Munich",
	"Miami",
	"Moscow",
	"St. Petersburg"
};



Record random_record(void) 
{
	static int id = 0;
	int r = rand() % 12;
	Record record = { .id = id++ };

    COPY(
		names[r], 
		record.name, 
		strlen(names[r]), 
		sizeof_field(Record, name)
	);

	r = rand() % 12;
	COPY(
		surnames[r], 
		record.surname, 
		strlen(surnames[r]), 
		sizeof_field(Record, surname)
	);

	r = rand() % 12;
	COPY(
		cities[r], 
		record.city,
		strlen(cities[r]), 
		sizeof_field(Record, city)
	);

    return record;
}


SRecord create_srecord(void *value, int block_id, rec_attr attr) 
{
	SRecord srec = { .counter = 1 };
	srec.block_id = block_id;
	memcpy(
		get_attr_type(attr) == INT
			? (void*)&srec.key.ikey
			: (void*)srec.key.skey,
		value,
		get_attr_size(attr)
	);
	return srec;
}


void print_record(Record *rec) 
{
	printf(
		"Id: %d\n"
		"Name: %s\n"
		"Surname: %s\n"
		"City: %s\n\n",
		rec->id, rec->name,
		rec->surname, rec->city
	);
}


int get_attr_offset(rec_attr attr) 
{
	return
		attr == ID      ? offsetof(Record, id)      :
		attr == NAME    ? offsetof(Record, name)    :
		attr == SURNAME ? offsetof(Record, surname) :
		offsetof(Record, city);
}


int get_attr_size(rec_attr attr) 
{
	return
		attr == ID      ? sizeof_field(Record, id)      :
		attr == NAME    ? sizeof_field(Record, name)    :
		attr == SURNAME ? sizeof_field(Record, surname) :
		sizeof_field(Record, city);
}

attr_type get_attr_type(rec_attr attr) 
{
	return
		attr == ID		? INT		:
		attr == NAME	? STRING	:
		attr == SURNAME ? STRING	:
		STRING;
}

void *get_rec_member(Record *rec, rec_attr attr) 
{
	return
		attr == ID      ? (void*)&rec->id      :
		attr == NAME    ? (void*)rec->name     :
		attr == SURNAME ? (void*)rec->surname  :
		(void*)rec->city;
	
}
