#include <stdlib.h>

struct ListElem;
typedef struct ListElem {
    int seqno;
    void *data;
    struct ListElem *next;
} ListElem;

typedef struct {
    ListElem *first;
    ListElem *last;
} List;

typedef void (*List_free_data) (void *);

List *List_new (void);
void List_free (List *l, List_free_data free_d);
void List_append (List *l, void *data, int no);
void List_insert (List *l, void *data, int no);
void List_take (List *l, void **data, int *seqno);
