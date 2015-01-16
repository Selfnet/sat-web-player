/* logging functions expect a (local) variable named 'instance' of type
 * 'PP_Instance'
 */
#ifndef __BOILER_H
#define __BOILER_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_var.h"

PP_Module module = {0};
PPB_GetInterface get_browser_interface = NULL;
const PPB_Console *G_PPB_CONSOLE = NULL;
const PPB_Var *G_PPB_VAR = NULL;

#define fetch_interface(name_in_caps) G_PPB_##name_in_caps = get_browser_interface (PPB_##name_in_caps##_INTERFACE);

#define make_var(s) G_PPB_VAR->VarFromUtf8(s, strlen (s))
#define make_str(s, l) G_PPB_VAR->VarToUtf8(s, l)

void __logf (PP_Instance inst, PP_LogLevel lvl, const char *fmt, ...)
{
    char msg[256] = {0};
    va_list ap;
    va_start (ap, fmt);
    vsnprintf (msg, sizeof (msg), fmt, ap);
    va_end (ap);

    G_PPB_CONSOLE->Log (inst, lvl, make_var (msg));
}

#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...)  __logf (instance, PP_LOGLEVEL_LOG, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define log_err(M, ...) __logf(instance, PP_LOGLEVEL_LOG, "[ERROR] (%s:%s:%d) " M "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define log_warn(M, ...) __logf(instance, PP_LOGLEVEL_LOG, "[WARN] (%s:%s:%d) " M "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define log_info(M, ...) __logf(instance, PP_LOGLEVEL_LOG, "[INFO] (%s:%s:%d) " M "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) { log_err(M, ##__VA_ARGS__); goto error; }

#define sentinel(M, ...) { log_err (M, ##__VA_ARGS__); goto error; }

#define check_mem(A) check((A), "Out of memory.")

#define check_debug(A, M, ...) if(!(A)) { debug(M, ##__VA_ARGS__); goto error; }

void
DidDestroy (PP_Instance inst) {}

void
DidChangeView (PP_Instance inst, PP_Resource res) {}

void
DidChangeFocus (PP_Instance inst, PP_Bool got) {}

PP_Bool
HandleDocumentLoad (PP_Instance inst, PP_Resource url_loader)
{
    return PP_FALSE;
}

void
PPP_ShutdownModule (void) {}
#endif
