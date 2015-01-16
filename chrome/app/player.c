#include "boiler.h"

typedef struct {
    PP_Instance instance;
} UDPArgs;

const PPB_NetAddress *G_PPB_NETADDRESS = NULL;
const PPB_UDPSocket  *G_PPB_UDPSOCKET  = NULL;

void *
UDPThread (void *args)
{
    PP_Instance instance = ((UDPArgs *) args)->instance;
    PP_Resource socket = G_PPB_UDPSOCKET->Create (instance);
    check (socket != 0, "Failed to create UDP socket.");

    struct PP_NetAddress_IPv4 addrv4 = {
        .port = 1234,
        .addr = {0, 0, 0, 0}
    };
    PP_Resource naddr =
            G_PPB_NETADDRESS->CreateFromIPv4Address (instance,
                                                     &addrv4);
    check (naddr != 0,
            "Failed to create address from v4 struct.");

error:
    return NULL;
}

PP_Bool
DidCreate (PP_Instance inst, uint32_t argc, const char** argn, const char** argv)
{
    debug ("Started module.");

    return PP_TRUE;
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
    return NULL;
}

int32_t
PPP_InitializeModule (PP_Module mod, PPB_GetInterface gbi)
{
    module = mod;
    get_browser_interface = gbi;

    fetch_interface (VAR);
    fetch_interface (CONSOLE);
    fetch_interface (UDPSOCKET);
    fetch_interface (NETADDRESS);

    return PP_OK;
}
