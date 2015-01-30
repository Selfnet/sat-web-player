#include "boiler.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"

#include <pthread.h>
#include <string.h>

#include "list.h"
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>

#define min(x,y) (x) < (y) ? (x) : (y)

const PPB_VarArrayBuffer *G_PPB_VAR_ARRAY_BUFFER = NULL;
const PPB_VarArray *G_PPB_VAR_ARRAY = NULL;

typedef struct {
    struct PP_Var var;
    void * buf;
    uint32_t len;
    uint32_t idx;
} Packet;
typedef struct {
    PP_Instance instance;
    List *packets;
} RefillData;

List *packets = NULL;

int
refill (void *data, uint8_t *buf, int buf_size)
{
    List *packets        = ((RefillData *) data)->packets;
    PP_Instance instance = ((RefillData *) data)->instance;
    debug ("Refill AVIOContext.");

    int ret = pthread_mutex_lock (packets->lock);
    check (ret == 0, "Failed to lock packet list lock while trying to refill AVIOContext.");
    check (packets->first != NULL,);

    Packet *pkt = NULL;
    int left, len;
    while (buf_size > 0) {
        if (pkt == NULL) List_take (packets, (void *) pkt);
        check (pkt != NULL, "Packet list is emtpy, cannot refill AVIOContext.")

        left = pkt->len - pkt->idx;
        len = min (left, buf_size);
        buf_size -= len;
        pkt->idx += len;

        memcpy (buf, (uint8_t *) pkt->buf, len);

        if (pkt->idx == pkt->len) {
            free (pkt);
            pkt = NULL;
        }
    }
    if (pkt != NULL) List_insert (packets, (void *) pkt);

    pthread_mutex_unlock (packets->lock);
    return 0;

error:
    pthread_mutex_unlock (packets->lock);
    return 1;
}

PP_Bool
DidCreate (PP_Instance instance, uint32_t argc, const char** argn, const char** argv)
{
    packets = List_new ();
    RefillData *rfdata = calloc (1, sizeof (RefillData));
    check (rfdata != NULL, "Failed to allocate RefillData.");
    rfdata->instance = instance;
    rfdata->packets  = packets;

    size_t avio_ctx_buffer_size = 4096;
    uint8_t *avio_ctx_buffer = NULL;
    AVIOContext *avio_ctx = NULL;
    AVFormatContext *fmt_ctx = NULL;

    avio_ctx_buffer = av_malloc (avio_ctx_buffer_size);
    check (avio_ctx_buffer != NULL, "Failed to allocate buffer for AVIOContext.");

    avio_ctx = avio_alloc_context (avio_ctx_buffer, avio_ctx_buffer_size, 1, rfdata,
                                   refill, NULL, NULL);
    check (avio_ctx != NULL, "Failed to allocate AVIOContext.");

    debug ("Started new instance: %i.", instance);
    return PP_TRUE;

error:
    return PP_FALSE;
}

void
HandleMessage (PP_Instance instance, struct PP_Var msg_array)
{
    Packet *pkt = NULL;
    struct PP_Var var;
    if (msg_array.type != PP_VARTYPE_ARRAY) {
        log_info ("JS client sent something that is not an array, ignoring.");
        log_var  (msg_array);
        return;
    }

    uint32_t len = G_PPB_VAR_ARRAY->GetLength (msg_array);

    pthread_mutex_lock (packets->lock);
    for (uint32_t i = 0; i < len; i++) {
        var = G_PPB_VAR_ARRAY->Get (msg_array, i);

        if (var.type != PP_VARTYPE_ARRAY_BUFFER) {
            log_info ("JS client put something not an array buffer in the array, ignoring.");
            continue;
        }

        pkt = calloc (1, sizeof (Packet));
        check (pkt != NULL, "Failed to allocate a Packet struct.");

        pkt->var = var;
        pkt->buf = G_PPB_VAR_ARRAY_BUFFER->Map (pkt->var);
        // for now we just ignore the RTP header
        pkt->idx = 8;
        List_append (packets, pkt);
    }

error:
    pthread_mutex_unlock (packets->lock);
    return;
}

const void *
PPP_GetInterface (const char *name)
{
    if (strcmp (name, PPP_INSTANCE_INTERFACE) == 0) {
        static PPP_Instance ii = {
            &DidCreate, &DidDestroy, &DidChangeView, &DidChangeFocus, &HandleDocumentLoad
        };
        return &ii;
    } else
    if (strcmp (name, PPP_MESSAGING_INTERFACE) == 0) {
        static PPP_Messaging mi = {
            &HandleMessage
        };
        return &mi;
    } else
    return NULL;
}

int32_t
PPP_InitializeModule (PP_Module mod, PPB_GetInterface gbi)
{
    get_browser_interface = gbi;

    fetch_interface (VAR);
    fetch_interface (CONSOLE);
    fetch_interface (VAR_ARRAY);
    fetch_interface (VAR_ARRAY_BUFFER);

    return PP_OK;
}
