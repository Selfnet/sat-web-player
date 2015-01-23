#include "list.h"
#include "dbg.h"

/* Create a new list.
 */
List *List_new ()
{
    List *l = calloc (1, sizeof (List));
    l->lock = calloc (1, sizeof (pthread_mutex_t));
    int ret = pthread_mutex_init (l->lock, NULL);
    check (l != NULL && l->lock != NULL && ret == 0, "Failed to init list.");

    l->len = 0;
    return l;

error:
    return NULL;
}

/* Create a new list element with the given pointer.
 */
ListElem *List_newelem (void * data)
{
    ListElem *el = calloc (1, sizeof (ListElem));
    if (el) el->data = data;

    return el;
}

/* Destroy the whole list, if free_d in not NULL it is called on each data
 * pointer in the elements
 */
void List_free (List *l, List_free_data free_d)
{
    if (!l) return;
    int ret = pthread_mutex_trylock (l->lock);

    check (ret == 0 || ret == EBUSY, "Failed to aquire lock for the list.");

    ListElem *el, *o = NULL;
    if (free_d) {
        for (el = l->first; el != l->last; el = el->next) {
            free_d (el->data);
            free (o);
            o = el;
        }
    } else {
        for (el = l->first; el != l->last; el = el->next) {
            free (o);
            o = el;
        }
    }
    if (free_d && l->last) free_d (l->last->data);
    free (l->last);

    if (ret != EBUSY) pthread_mutex_unlock (l->lock);
    pthread_mutex_destroy (l->lock);
    free (l->lock);
    free (l);

error:
    return;
}

/* Add a new element at the end of the list with the provided data
 * pointer
 */
void List_append (List *l, void *data)
{
    if (!l) return;
    int ret = pthread_mutex_trylock (l->lock);

    check (ret == 0 || ret == EBUSY, "Failed to aquire lock for the list.");

    ListElem *el = List_newelem (data);

    if (l->first == NULL) {
        l->first = el;
    } else {
        l->last->next = el;
    }
    l->last = el;
    l->len++;

    if (ret != EBUSY) pthread_mutex_unlock (l->lock);

error:
    return;
}

/* Add a new element at the beginning of the list with the provided data
 * pointer
 */
void List_insert (List *l, void *data)
{
    if (!l) return;
    int ret = pthread_mutex_trylock (l->lock);

    check (ret == 0 || ret == EBUSY, "Failed to aquire lock for the list.");

    ListElem *el = List_newelem (data);
    if (l->first == NULL) {
        l->last = el;
    }  else {
        el->next = l->first;
    }
    l->first = el;
    l->len++;

    if (ret != EBUSY) pthread_mutex_unlock (l->lock);

error:
    return;
}

/* Remove the first element from the list and write its data to the provided
 * pointer
 */
void List_take (List *l, void **data)
{
    if (!l || !l->first) return;
    int ret = pthread_mutex_trylock (l->lock);

    check (ret == 0 || ret == EBUSY, "Failed to aquire lock for the list.");

    ListElem *el = l->first;
    if (l->first == l->last) {
        l->last = NULL;
    }
    l->first = el->next;

    *data = el->data;
    l->len--;

    free (el);
    if (ret != EBUSY) pthread_mutex_unlock (l->lock);

    return;

error:
    *data = NULL;
}
