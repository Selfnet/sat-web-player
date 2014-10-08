#include "list.h"

List *List_new (void)
{
    List *l = calloc (1, sizeof (List));
    return l;
}

ListElem *List_newelem (void * data, int seqno)
{
    ListElem *el = calloc (1, sizeof (ListElem));
    if (el) {
        el->data = data;
        el->seqno = seqno;
    }

    return el;
}

void List_free (List *l, List_free_data free_d)
{
    if (!l) return;

    ListElem *el, *o = NULL;
    for (el = l->first; el != l->last; el = el->next) {
        if (free_d) free_d (el->data);
        free (o);
        o = el;
    }
    if (free_d && l->last) free_d (l->last->data);
    free (l->last);

    free (l);
}

void List_append (List *l, void *data, int no)
{
    if (!l) return;

    ListElem *el = List_newelem (data, no);

    if (l->first == NULL) {
        l->first = el;
    } else {
        l->last->next = el;
    }
    l->last = el;
}

void List_insert (List *l, void *data, int no)
{
    if (!l) return;

    ListElem *el = List_newelem (data, no);
    if (l->first == NULL) {
        l->last = el;
    }  else {
        el->next = l->first;
    }
    l->first = el;
}

void List_take (List *l, void **data, int *seqno)
{
    if (!l || !l->first) return;

    ListElem *el = l->first;
    if (l->first == l->last) {
        l->last = NULL;
    }
    l->first = el->next;

    *data = el->data;
    if (seqno) *seqno = el->seqno;

    free (el);
}
