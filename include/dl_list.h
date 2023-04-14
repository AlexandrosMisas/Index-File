#ifndef DL_LIST_H
#define DL_LIST_H

typedef struct dl_list *Dl_list;
typedef struct dl_list_node *Dl_list_node;
typedef void (*Destructor)(void *a);
typedef void (*Visit)(void *a);
typedef int (*Comparator)(void *a, void *b);

Dl_list list_create(Destructor destroy_func);

int list_size(Dl_list list);

Dl_list_node list_first(Dl_list list);

Dl_list_node list_last(Dl_list list);

Dl_list_node list_next(Dl_list_node node);

Dl_list_node list_previous(Dl_list_node node);

void list_insert(Dl_list list, void *entry);

void list_delete(Dl_list list, Dl_list_node node);

Dl_list_node list_find(Dl_list list, void *value, Comparator compare);

void *list_value(Dl_list_node node);

void print_list(Dl_list list, Visit visit_func);

void list_destroy(Dl_list list);


#endif /* DL_LIST_H */