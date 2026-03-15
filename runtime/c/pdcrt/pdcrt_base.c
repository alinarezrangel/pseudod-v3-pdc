//
// Created by alinarezrangel on 29/10/25.
//

#include "pdcrt_base.h"

uintptr_t pdcrt_obtener_stack_pointer(void);
size_t pdcrt_redondear_a_p2(size_t n);

#if !PDCRT_LOG_COMPILADO
PDCRT_PRINTF_FORMAT(3, 4)
void pdcrt_log(pdcrt_ctx *ctx, pdcrt_subsistema sis, const char *fmt, ...);
#else
PDCRT_PRINTF_FORMAT(3, 4)
void pdcrt_log(pdcrt_ctx *ctx, pdcrt_subsistema sis, const char *fmt, ...)
{
    switch(sis)
    {
    case PDCRT_SUBSISTEMA_GC:
        if(!ctx->log.gc)
            return;
        fprintf(stderr, "; gc -- ");
        break;
    default:
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    va_end(ap);
}
#endif
