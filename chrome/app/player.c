#include "boiler.h"
#include "ppapi/c/ppp_messaging.h"
#include "list.h"
//#include "ffmpeg.h"

List *packets = NULL;

PP_Bool
DidCreate (PP_Instance instance, uint32_t argc, const char** argn, const char** argv)
{
    debug ("Started new instance.");

    packets = List_new ();

    return PP_TRUE;
}

void
HandleMessage (PP_Instance instance, struct PP_Var msg)
{
    void *pmsg = malloc (sizeof (struct PP_Var));
    if (pmsg) List_append (packets, pmsg);
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
    module = mod;
    get_browser_interface = gbi;

    fetch_interface (VAR);
    fetch_interface (CONSOLE);

    return PP_OK;
}
