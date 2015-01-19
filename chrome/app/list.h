#include <stdlib.h>
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
} List;

typedef void (*List_free_data) (void *);

List *List_new (void);
void List_free (List *l, List_free_data free_d);
void List_append (List *l, void *data);
void List_insert (List *l, void *data);
void List_take (List *l,  void **data);
