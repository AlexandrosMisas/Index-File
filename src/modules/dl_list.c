#include "common.h"
#include "dl_list.h"


struct dl_list {
	Dl_list_node first;
	Dl_list_node last;
	Destructor destroy_func;
	int size;
};


struct dl_list_node {
	Dl_list_node prev;
	Dl_list_node next;
	void *value;
};


Dl_list list_create(Destructor destroy_func) 
{
	Dl_list list = calloc(1, sizeof(*list));
	list->destroy_func = destroy_func;
	return list;
}


int list_size(Dl_list list) 
{
	return list != NULL ? list->size : -1;
}


Dl_list_node list_first(Dl_list list)
{
	return list != NULL ? list->first : NULL;
}


Dl_list_node list_last(Dl_list list) 
{
	return list != NULL ? list->last : NULL;
}


Dl_list_node list_next(Dl_list_node node) 
{
	return node != NULL ? node->next : NULL;
}


Dl_list_node list_previous(Dl_list_node node) 
{
	return node != NULL ? node->prev : NULL;
}


void *list_value(Dl_list_node node) 
{
	return node != NULL ? node->value : NULL;
}


void list_insert(Dl_list list, void *entry) 
{
	Dl_list_node new = calloc(1, sizeof(*new));
	new->value = entry;

	if (!list->size) {
		list->first = list->last = new;
	} else {
		list->last->next = new;
		new->prev = list->last;
		list->last = new;
	}
	list->size++;
}


void list_delete(Dl_list list, Dl_list_node node) 
{
	if (!list->size)
		return;
	
	if (node->prev == NULL)
		list->first = node->next;
	else
		node->prev->next = node->next;
	
	if (node->next == NULL)
		list->last = node->prev;
	else
		node->next->prev = node->prev;
	
	if (list->destroy_func != NULL)
		list->destroy_func(node->value);

	free(node); 
	list->size--;
}


void list_destroy(Dl_list list) 
{
	while (list->size--) {
		Dl_list_node temp = list->first;
		list->first = list->first->next;

		if (list->destroy_func != NULL)
			list->destroy_func(temp->value);
		free(temp);
	}
	free(list);
}

Dl_list_node list_find(Dl_list list, void *value, Comparator compare) 
{
	Dl_list_node node = list->first;
	while (node != NULL) {
		if (compare(node->value, value) == 0)
			return node;
		node = node->next;
	}
	return NULL;
}

void print_list(Dl_list list, Visit visit_func) 
{
	Dl_list_node tmp = list->first;
	for (;;) {
		visit_func(tmp->value);
		if (tmp == list->last)
			break;
		tmp = tmp->next;
	}
}