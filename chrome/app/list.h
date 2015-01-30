#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

struct ListElem;
typedef struct ListElem {
    void *data;
    struct ListElem *next;
} ListElem;

typedef struct {
    ListElem *first;
    ListElem *last;
    pthread_mutex_t *lock;
    unsigned int len;
} List;

typedef void (*List_free_data) (void *);

List *List_new ();
void List_free (List *l, List_free_data free_d);
void List_append (List *l, void *data);
void List_insert (List *l, void *data);
void List_take (List *l,  void **data);
void List_append_locked (List *l, void *data);
void List_insert_locked (List *l, void *data);
void List_take_locked (List *l,  void **data);
