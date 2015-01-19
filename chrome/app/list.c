#include "list.h"

List *List_new (void)
{
    List *l = calloc (1, sizeof (List));
    l->lock = calloc (1, sizeof (pthread_mutex_t));
    pthread_mutex_init (l->lock, NULL);
    return l;
}

ListElem *List_newelem (void * data)
{
    ListElem *el = calloc (1, sizeof (ListElem));
    if (el) el->data = data;

    return el;
}

void List_free (List *l, List_free_data free_d)
{
    if (!l) return;
    pthread_mutex_lock (l->lock);

    ListElem *el, *o = NULL;
    for (el = l->first; el != l->last; el = el->next) {
        if (free_d) free_d (el->data);
        free (o);
        o = el;
    }
    if (free_d && l->last) free_d (l->last->data);
    free (l->last);

    pthread_mutex_unlock (l->lock);
    free (l);
}

void List_append (List *l, void *data)
{
    if (!l) return;
    pthread_mutex_lock (l->lock);

    ListElem *el = List_newelem (data);

    if (l->first == NULL) {
        l->first = el;
    } else {
        l->last->next = el;
    }
    l->last = el;

    pthread_mutex_unlock (l->lock);
}

void List_insert (List *l, void *data)
{
    if (!l) return;
    pthread_mutex_lock (l->lock);

    ListElem *el = List_newelem (data);
    if (l->first == NULL) {
        l->last = el;
    }  else {
        el->next = l->first;
    }
    l->first = el;

    pthread_mutex_unlock (l->lock);
}

void List_take (List *l, void **data)
{
    if (!l || !l->first) return;
    pthread_mutex_lock (l->lock);

    ListElem *el = l->first;
    if (l->first == l->last) {
        l->last = NULL;
    }
    l->first = el->next;

    *data = el->data;

    free (el);
    pthread_mutex_unlock (l->lock);
}
