#include "boiler.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "list.h"
#include <libavformat/avformat.h>
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"

const PPB_VarArrayBuffer *G_PPB_VAR_ARRAY_BUFFER = NULL;
const PPB_VarArray *G_PPB_VAR_ARRAY = NULL;

typedef struct {
    struct PP_Var var;
    void * buf;
    uint32_t len;
    uint32_t idx;
} Packet;

List *packets = NULL;

int
refill (void *pkt_list, uint8_t *buf, int buf_size)
{
    return 0;
}

PP_Bool
DidCreate (PP_Instance instance, uint32_t argc, const char** argn, const char** argv)
{
    check (packets == NULL, "Another instance is already running. We don't handle multiple instances yet.");
    debug ("Started new instance.");

    packets = List_new (instance);

    size_t avio_ctx_buffer_size = 4096;
    uint8_t *avio_ctx_buffer = NULL;
    AVIOContext *avio_ctx = NULL;
    AVFormatContext *fmt_ctx = NULL;

    avio_ctx_buffer = av_malloc (avio_ctx_buffer_size);
    check (avio_ctx_buffer != NULL, "Failed to allocate buffer for AVIOContext.");

    avio_ctx = avio_alloc_context (avio_ctx_buffer, avio_ctx_buffer_size, 1, packets,
                                   refill, NULL, NULL);
    check (avio_ctx != NULL, "Failed to allocate AVIOContext.");

    return PP_TRUE;

error:
    return PP_FALSE;
}

void
HandleMessage (PP_Instance instance, struct PP_Var msg_array)
{
    Packet *pkt = NULL;
    struct PP_Var var;
    check_info (msg_array.type == PP_VARTYPE_ARRAY, "JS client sent something that is not an array, ignoring.");

    uint32_t len = G_PPB_VAR_ARRAY->GetLength (msg_array);

    for (uint32_t i = 0; i < len; i++) {
        var = G_PPB_VAR_ARRAY->Get (msg_array, i);

        if (pkt->var.type != PP_VARTYPE_ARRAY_BUFFER) {
            log_info ("JS client put something not an array buffer in the array, ignoring.");
            continue;
        }

        pkt = calloc (1, sizeof (Packet));
        check (pkt != NULL, "Failed to allocate a Packet struct. Guess we ran out of memory, aborting.");

        pkt->var = var;
        pkt->buf = G_PPB_VAR_ARRAY_BUFFER->Map (pkt->var);
        // for now we just ignore the RTP header
        pkt->idx = 8;
        List_append (packets, pkt);
    }

error:
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
