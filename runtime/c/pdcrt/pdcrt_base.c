//
// Created by alinarezrangel on 29/10/25.
//

#include "pdcrt_base.h"
#include "pdcrt_dtrace.h"

#include <stdlib.h>
#include <stdarg.h>

extern uintptr_t pdcrt_obtener_stack_pointer(void);
extern size_t pdcrt_redondear_a_p2(size_t n);

void *pdcrt_alojar(pdcrt_aloj *aloj, size_t bytes)
{
    return aloj->alojar(aloj, bytes);
}

void *pdcrt_realojar(pdcrt_aloj *aloj, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    return aloj->realojar(aloj, ptr, tam_actual, tam_nuevo);
}

void pdcrt_desalojar(pdcrt_aloj *aloj, void *ptr, size_t tam_actual)
{
    aloj->desalojar(aloj, ptr, tam_actual);
}

static void *pdcrt_alojar_malloc(void *yo, size_t tam_nuevo)
{
    (void) yo;
    PDCRT_PROBE1(malloc, tam_nuevo);
    return malloc(tam_nuevo);
}

static void *pdcrt_realojar_malloc(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    (void) yo;
    PDCRT_PROBE2(realloc, tam_actual, tam_nuevo);
    return realloc(ptr, tam_nuevo);
}

static void pdcrt_desalojar_malloc(void *yo, void *ptr, size_t tam_actual)
{
    (void) yo;
    (void) tam_actual;
    PDCRT_PROBE1(free, tam_actual);
    free(ptr);
}

pdcrt_aloj* pdcrt_alojador_malloc(void)
{
    static pdcrt_aloj de_malloc = {
        .alojar = &pdcrt_alojar_malloc,
        .realojar = &pdcrt_realojar_malloc,
        .desalojar = &pdcrt_desalojar_malloc,
        .obtener_extensiones = NULL,
    };
    return &de_malloc;
}

typedef struct pdcrt_aloj_con_estadisticas
{
    pdcrt_aloj aloj;
    pdcrt_aloj *base;
    size_t alojado;
} pdcrt_aloj_con_estadisticas;

static void *pdcrt_alojar_con_estadisticas(void *yo, size_t tam_nuevo);
static void *pdcrt_realojar_con_estadisticas(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo);
static void pdcrt_desalojar_con_estadisticas(void *yo, void *ptr, size_t tam_actual);

pdcrt_aloj* pdcrt_alojador_con_estadisticas(pdcrt_aloj* base)
{
    pdcrt_aloj_con_estadisticas* res = pdcrt_alojar(base, sizeof(pdcrt_aloj_con_estadisticas));
    if(!res)
        return NULL;
    res->aloj.alojar = &pdcrt_alojar_con_estadisticas;
    res->aloj.realojar = &pdcrt_realojar_con_estadisticas;
    res->aloj.desalojar = &pdcrt_desalojar_con_estadisticas;
    res->aloj.obtener_extensiones = NULL;
    res->base = base;
    res->alojado = 0;
    return (pdcrt_aloj *) res;
}

size_t pdcrt_alojador_con_estadisticas_obtener_usado(pdcrt_aloj* yo)
{
    pdcrt_aloj_con_estadisticas *est = (pdcrt_aloj_con_estadisticas *) yo;
    return est->alojado;
}

void pdcrt_desalojar_alojador_con_estadisticas(pdcrt_aloj* yo)
{
    pdcrt_aloj_con_estadisticas *est = (pdcrt_aloj_con_estadisticas *) yo;
    pdcrt_desalojar(est->base, est, sizeof(pdcrt_aloj_con_estadisticas));
}

static void *pdcrt_alojar_con_estadisticas(void *yo, size_t tam_nuevo)
{
    pdcrt_aloj_con_estadisticas *est = yo;
    void *res = pdcrt_alojar(est->base, tam_nuevo);
    if(res)
    {
        PDCRT_PROBE1(malloc_con_estadisticas, tam_nuevo);
        est->alojado += tam_nuevo;
    }
    return res;
}

static void *pdcrt_realojar_con_estadisticas(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    pdcrt_aloj_con_estadisticas *est = yo;
    void *res = pdcrt_realojar(est->base, ptr, tam_actual, tam_nuevo);
    if(res)
    {
        PDCRT_PROBE2(realloc_con_estadisticas, tam_actual, tam_nuevo);
        est->alojado -= tam_actual;
        est->alojado += tam_nuevo;
    }
    return res;
}

static void pdcrt_desalojar_con_estadisticas(void *yo, void *ptr, size_t tam_actual)
{
    pdcrt_aloj_con_estadisticas *est = yo;
    if(ptr)
    {
        PDCRT_PROBE1(free_con_estadisticas, tam_actual);
        est->alojado -= tam_actual;
    }
    pdcrt_desalojar(est->base, ptr, tam_actual);
}
