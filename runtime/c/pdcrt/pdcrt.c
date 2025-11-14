#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <stddef.h>
#include <stdarg.h>

#include "pdcrt-plataforma.h"

#define PDCRT_INTERNO
#include "pdcrt/pdcrt.h"


static uintptr_t pdcrt_obtener_stack_pointer(void)
{
    // TODO: Portar a distíntas arquitecturas
    uintptr_t sp;
    asm inline("mov %%rsp, %0\n"
        : "=r" (sp));
    return sp;
}


typedef enum pdcrt_subsistema
{
    PDCRT_SUBSISTEMA_GC,
} pdcrt_subsistema;

#ifdef __has_attribute
#  if __has_attribute(format)
#    define PDCRT_PRINTF_FORMAT(fmtarg, fromarg) \
    __attribute__((format(printf, fmtarg, fromarg)))
#  else
#    define PDCRT_PRINTF_FORMAT(fmtarg, fromarg)
#  endif
#else
#  define PDCRT_PRINTF_FORMAT(fmtarg, fromarg)
#endif

#if !PDCRT_LOG_COMPILADO
// Si no hay ningún logger activo, la función no hace nada. En ese caso,
// siempre la inlineamos para que el compilador elimine las
// llamadas. Efectivamente, esto "borra" la función del programa.
PDCRT_INLINE
#endif
static
PDCRT_PRINTF_FORMAT(3, 4)
void pdcrt_log(pdcrt_ctx *ctx, pdcrt_subsistema sis, const char *fmt, ...)
{
#if PDCRT_LOG_COMPILADO
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
#else
    (void) ctx;
    (void) sis;
    (void) fmt;
#endif
}

static int pdcrt_time(struct timespec *out)
{
    struct timespec tmp;
#ifdef PDCRT_PLATAFORMA_CLOCK_GETTIME_MONOTONIC
    if(clock_gettime(CLOCK_MONOTONIC, out ? out : &tmp) == 0)
        return 1;
#elif PDCRT_PLATAFORMA_TIMESPEC_GET_MONOTONIC
    if(timespec_get(out ? out : &tmp, TIME_MONOTONIC) != 0)
        return 1;
#endif
    return 0;
}

typedef struct pdcrt_timediff
{
    long dif_s;
    long dif_ms;
    long dif_us;
    long dif_ns;
} pdcrt_timediff;

#define PDCRT_ABS(n) ((n) < 0 ? -(n) : (n))

static void pdcrt_diferencia(struct timespec *primero, struct timespec *segundo, pdcrt_timediff *res)
{
    long dif_ns = segundo->tv_nsec - primero->tv_nsec;
    long dif_us = (dif_ns / 1000L) % 1000;
    long dif_ms = (dif_ns / 1000000L) % 1000;
    res->dif_s = segundo->tv_sec - primero->tv_sec;
    res->dif_ms = PDCRT_ABS(dif_ms);
    res->dif_us = PDCRT_ABS(dif_us);
    res->dif_ns = PDCRT_ABS(dif_ns % 1000);
}

#define PDCRT_FORMATEAR_BYTES_TAM_BUFFER 80LU

static void pdcrt_formatear_bytes(char *buffer, size_t bytes)
{
    memset(buffer, 0, PDCRT_FORMATEAR_BYTES_TAM_BUFFER);
    size_t frac = 0;

    int res = 0;
    if(bytes < 1024)
    {
        res = snprintf(buffer, PDCRT_FORMATEAR_BYTES_TAM_BUFFER, "%zub", bytes);
    }
    else if(bytes < 1024LU * 1024LU)
    {
        frac = bytes % 1024LU;
        res = snprintf(buffer, PDCRT_FORMATEAR_BYTES_TAM_BUFFER, "%zu.%03zuKib", bytes / 1024LU, frac);
    }
    else if(bytes < 1024LU * 1024LU * 1024LU)
    {
        frac = (bytes / 1024LU) % 1024LU;
        res = snprintf(buffer, PDCRT_FORMATEAR_BYTES_TAM_BUFFER, "%zu.%03zuMib", bytes / (1024LU * 1024LU), frac);
    }
    else
    {
        frac = (bytes / (1024LU * 1024LU)) % 1024LU;
        res = snprintf(buffer, PDCRT_FORMATEAR_BYTES_TAM_BUFFER, "%zu.%03zuGib", bytes / (1024LU * 1024LU * 1024LU), frac);
    }

    if(res >= (int) PDCRT_FORMATEAR_BYTES_TAM_BUFFER)
    {
        buffer[PDCRT_FORMATEAR_BYTES_TAM_BUFFER - 1] = 0;
    }
    else if(res < 0)
    {
        strncpy(buffer, "no se pudo formatear la cantidad", PDCRT_FORMATEAR_BYTES_TAM_BUFFER);
    }
}


static _Noreturn void pdcrt_error(pdcrt_ctx *ctx, const char* msj)
{
    ctx->mensaje_de_error = msj;
    if(!ctx->hay_un_manejador_de_errores)
    {
        fprintf(stderr, "FATAL: %s\n", msj);
        abort();
    }
    else
    {
        pdcrt_recolectar_basura_por_pila(ctx, NULL);
        longjmp(ctx->manejador_de_errores, 1);
    }
}

static _Noreturn void pdcrt_enomem(pdcrt_ctx *ctx)
{
    pdcrt_error(ctx, "Sin memoria");
}

static pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o);

static void pdcrt_debe_tener_tipo(pdcrt_ctx *ctx, pdcrt_obj obj, pdcrt_tipo t)
{
    if(pdcrt_tipo_de_obj(obj) != t)
    {
        pdcrt_error(ctx, "Valor de tipo inesperado");
    }
}

PDCRT_INLINE static pdcrt_k pdcrt_continuar(pdcrt_ctx *ctx, pdcrt_k k)
{
    return k.kf(ctx, k.marco);
}


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

static void *pdcrt_alojar_malloc(void *yo, size_t bytes)
{
    (void) yo;
    return malloc(bytes);
}

static void *pdcrt_realojar_malloc(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    (void) yo;
    (void) tam_actual;
    return realloc(ptr, tam_nuevo);
}

static void pdcrt_desalojar_malloc(void *yo, void *ptr, size_t tam_actual)
{
    (void) yo;
    (void) tam_actual;
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

typedef struct pdcrt_aloj_basico_pagina
{
    char *pagina;
    uint64_t *ocupado;
} pdcrt_aloj_basico_pagina;

typedef struct pdcrt_aloj_basico
{
    pdcrt_aloj aloj;
    pdcrt_aloj* base;
    pdcrt_aloj_basico_cfg_v1 cfg;

    size_t num_paginas;
    pdcrt_aloj_basico_pagina *paginas;
    size_t num_ocupado;
} pdcrt_aloj_basico;


static void *pdcrt_alojar_basico(void *yo, size_t bytes);
static void *pdcrt_realojar_basico(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo);
static void pdcrt_desalojar_basico(void *yo, void *ptr, size_t tam_actual);

static void pdcrt_aloj_basico_inicializar_pagina(pdcrt_aloj_basico *basico, size_t pagina);

pdcrt_aloj* pdcrt_alojador_basico(pdcrt_aloj* base, pdcrt_aloj_basico_cfg_v1* cfg, size_t tam_cfg)
{
    pdcrt_aloj_basico *basico = pdcrt_alojar(base, sizeof(pdcrt_aloj_basico));
    if(!basico)
        return NULL;

    basico->aloj.alojar = &pdcrt_alojar_basico;
    basico->aloj.realojar = &pdcrt_realojar_basico;
    basico->aloj.desalojar = &pdcrt_desalojar_basico;
    basico->aloj.obtener_extensiones = NULL;

    assert(cfg->tam_pagina % 64 == 0);
    assert(cfg->num_inicial_de_paginas > 0);

    basico->base = base;
    assert(tam_cfg == sizeof(pdcrt_aloj_basico_cfg_v1));
    basico->cfg = *cfg;

    basico->num_paginas = cfg->num_inicial_de_paginas;
    basico->num_ocupado = basico->cfg.tam_pagina / 64;
    basico->paginas = pdcrt_alojar(base, basico->num_paginas * sizeof(pdcrt_aloj_basico_pagina));
    if(!basico->paginas)
    {
        pdcrt_desalojar(base, basico, sizeof(pdcrt_aloj_basico));
        return NULL;
    }
    for(size_t i = 0; i < basico->num_paginas; i++)
    {
        pdcrt_aloj_basico_inicializar_pagina(basico, i);
    }
    return (pdcrt_aloj *) basico;
}

void pdcrt_desalojar_alojador_basico(pdcrt_aloj* basico)
{
    pdcrt_aloj_basico *yo = (pdcrt_aloj_basico *) basico;
    for(size_t i = 0; i < yo->num_paginas; i++)
    {
        pdcrt_aloj_basico_pagina *p = &yo->paginas[i];
        if(p->ocupado)
            pdcrt_desalojar(yo->base, p->ocupado, sizeof(uint64_t) * yo->num_ocupado);
        if(p->pagina)
            pdcrt_desalojar(yo->base, p->pagina, yo->cfg.tam_pagina);
    }
    pdcrt_desalojar(yo->base, yo, sizeof(pdcrt_aloj_basico));
}

static void pdcrt_aloj_basico_inicializar_pagina(pdcrt_aloj_basico *basico, size_t pagina)
{
    pdcrt_aloj_basico_pagina *p = &basico->paginas[pagina];
    p->pagina = NULL,
    p->ocupado = NULL;
}

static bool pdcrt_aloj_basico_esta_ocupado(pdcrt_aloj_basico *basico, pdcrt_aloj_basico_pagina *pagina, size_t byte)
{
    assert(byte <= basico->cfg.tam_pagina);
    return (pagina->ocupado[byte >> 6] & (UINT64_C(1) << (uint64_t) (byte & 63))) != 0;
}

static size_t pdcrt_aloj_basico_siguiente_alineacion(size_t cur)
{
    if((cur & 7) != 0)
        return (cur | 7) + 1;
    else
        return cur;
}

static pdcrt_aloj_basico_pagina * pdcrt_aloj_basico_obtener_pagina(pdcrt_aloj_basico *basico, size_t pagina)
{
    if(basico->paginas[pagina].pagina == NULL)
    {
        char *pag = pdcrt_alojar(basico->base, basico->cfg.tam_pagina);
        if(!pag)
            return NULL;
        memset(pag, 0, basico->cfg.tam_pagina); // TODO: HARDENED
        basico->paginas[pagina].pagina = pag;
    }
    if(basico->paginas[pagina].ocupado == NULL)
    {
        uint64_t *ocupado = pdcrt_alojar(basico->base, sizeof(uint64_t) * basico->num_ocupado);
        if(!ocupado)
            return NULL;
        memset(ocupado, 0, sizeof(uint64_t) * basico->num_ocupado);
        basico->paginas[pagina].ocupado = ocupado;
    }
    return &basico->paginas[pagina];
}

static bool pdcrt_aloj_basico_buscar_libre(pdcrt_aloj_basico *basico,
                                           size_t bytes,
                                           size_t *pagina,
                                           size_t *offset)
{
    assert(bytes <= basico->cfg.tam_pagina);

    size_t tam_pagina = basico->cfg.tam_pagina;
    for(size_t i = 0; i < basico->num_paginas; i++)
    {
        pdcrt_aloj_basico_pagina *p = pdcrt_aloj_basico_obtener_pagina(basico, i);
        if(!p)
        {
            return false;
        }

        size_t off = 0, byte = 0;
        for(byte = 0; byte < bytes && off + bytes <= tam_pagina;)
        {
            size_t j = bytes - (byte + 1);
            if(pdcrt_aloj_basico_esta_ocupado(basico, p, off + j))
            {
                byte = 0;
                off = pdcrt_aloj_basico_siguiente_alineacion(off + j + 1);
            }
            else
            {
                byte += 1;
            }
        }

        if(off + bytes > tam_pagina || byte != bytes)
        {
            // No se encontró un espacio lo suficientemente grande en esta página
            continue;
        }
        else
        {
            *offset = off;
            *pagina = i;
            return true;
        }
    }
    return false;
}

static bool pdcrt_aloj_basico_crear_pagina(pdcrt_aloj_basico *basico)
{
    pdcrt_aloj_basico_pagina *nuevas_paginas = pdcrt_realojar(basico->base,
                                                              basico->paginas,
                                                              basico->num_paginas * sizeof(pdcrt_aloj_basico_pagina),
                                                              (basico->num_paginas * 2) * sizeof(pdcrt_aloj_basico_pagina));
    if(!nuevas_paginas)
        return false;
    basico->paginas = nuevas_paginas;
    for(size_t i = basico->num_paginas; i < basico->num_paginas * 2; i++)
    {
        pdcrt_aloj_basico_inicializar_pagina(basico, i);
    }
    basico->num_paginas *= 2;
    return true;
}

typedef bool (*pdcrt_aloj_basico_iter_bit)(uint64_t *ocupado, uint8_t bit, bool val, void *data);
typedef bool (*pdcrt_aloj_basico_iter_palabra)(uint64_t *ocupado, void *data);

static PDCRT_INLINE void pdcrt_aloj_basico_para_cada_bit(pdcrt_aloj_basico *basico,
                                                         pdcrt_aloj_basico_pagina *p,
                                                         size_t offset,
                                                         size_t bytes,
                                                         pdcrt_aloj_basico_iter_bit iter_bit,
                                                         pdcrt_aloj_basico_iter_palabra iter_palabra,
                                                         void *data)
{
    assert(offset + bytes <= basico->cfg.tam_pagina);
    if(bytes == 0)
        return;
    size_t palabra_inic = offset >> 6;
    uint64_t *ocupado = &p->ocupado[palabra_inic];
    uint8_t bit_inic = offset & 63;
    for(uint8_t bit = bit_inic; bit < 64 && bytes > 0; bit++)
    {
        bool val = (*ocupado & (UINT64_C(1) << (uint64_t) bit)) != 0;
        if(!(*iter_bit)(ocupado, bit, val, data))
            return;
        assert(bytes > 0);
        bytes -= 1;
    }
    if(bytes == 0)
        return;
    ocupado += 1;
    size_t palabras_restantes = bytes >> 6;
    uint8_t bits_restantes = bytes & 63;
    for(size_t palabra = 0; palabra < palabras_restantes && bytes > 0; palabra++)
    {
        if(iter_palabra)
        {
            if(!(*iter_palabra)(ocupado, data))
                return;
        }
        else
        {
            for(uint8_t bit = 0; bit < 64; bit++)
            {
                bool val = (*ocupado & (UINT64_C(1) << (uint64_t) bit)) != 0;
                if(!(*iter_bit)(ocupado, bit, val, data))
                    return;
            }
        }
        assert(bytes >= 64);
        bytes -= 64;
        ocupado += 1;
    }
    if(bytes == 0)
        return;
    for(uint8_t bit = 0; bit < bits_restantes && bytes > 0; bit++)
    {
        bool val = (*ocupado & (UINT64_C(1) << (uint64_t) bit)) != 0;
        if(!(*iter_bit)(ocupado, bit, val, data))
            return;
        assert(bytes > 0);
        bytes -= 1;
    }
    assert(bytes == 0);
}

static bool pdcrt_aloj_basico_marcar_como_ocupado_iter_bit(uint64_t *ocupado, uint8_t bit, bool val, void *data)
{
    (void) val;
    (void) data;
    assert((*ocupado & (UINT64_C(1) << (uint64_t) bit)) == 0);
    *ocupado = *ocupado | (UINT64_C(1) << (uint64_t) bit);
    return true;
}

static bool pdcrt_aloj_basico_marcar_como_ocupado_iter_palabra(uint64_t *ocupado, void *data)
{
    (void) data;
    assert(*ocupado == 0);
    *ocupado = UINT64_MAX;
    return true;
}

static void pdcrt_aloj_basico_marcar_como_ocupado(pdcrt_aloj_basico *basico, pdcrt_aloj_basico_pagina *p, size_t offset, size_t bytes)
{
    pdcrt_aloj_basico_para_cada_bit(basico, p,
                                    offset, bytes,
                                    &pdcrt_aloj_basico_marcar_como_ocupado_iter_bit,
                                    &pdcrt_aloj_basico_marcar_como_ocupado_iter_palabra,
                                    NULL);
}

typedef struct pdcrt_aloj_basico_extender_env
{
    bool ok;
    size_t tam_actual;
    size_t tam_nuevo;
} pdcrt_aloj_basico_extender_env;

#define PDCRT_DEC_CAP(v, n) v = ((v) == 0) ? 0 : ((v) - (n))

static bool pdcrt_aloj_basico_puede_extender_iter_bit(uint64_t *ocupado, uint8_t bit, bool val, void *data)
{
    (void) ocupado;
    (void) bit;
    pdcrt_aloj_basico_extender_env *env = data;
    assert(env->tam_actual > 0 || env->tam_nuevo > 0);
    if(env->tam_actual > 0)
        env->ok = env->ok && val;
    else
        env->ok = env->ok && !val;
    PDCRT_DEC_CAP(env->tam_actual, 1);
    PDCRT_DEC_CAP(env->tam_nuevo, 1);
    return true;
}

static bool pdcrt_aloj_basico_puede_extender(pdcrt_aloj_basico *basico, pdcrt_aloj_basico_pagina *p, size_t offset, size_t tam_actual, size_t tam_nuevo)
{
    assert(p->pagina);
    assert(offset + tam_actual <= basico->cfg.tam_pagina);
    assert(offset + tam_nuevo <= basico->cfg.tam_pagina);
    if(tam_nuevo <= tam_actual)
        return true;
    pdcrt_aloj_basico_extender_env env = {
        .ok = true,
        .tam_actual = tam_actual,
        .tam_nuevo = tam_nuevo,
    };
    pdcrt_aloj_basico_para_cada_bit(basico, p,
                                    offset, tam_actual < tam_nuevo ? tam_nuevo : tam_actual,
                                    &pdcrt_aloj_basico_puede_extender_iter_bit,
                                    NULL,
                                    &env);
    return env.ok;
}

static bool pdcrt_aloj_basico_extender_iter_bit(uint64_t *ocupado, uint8_t bit, bool val, void *data)
{
    (void) val;
    (void) data;
    *ocupado = *ocupado | (UINT64_C(1) << (uint64_t) bit);
    return true;
}

static bool pdcrt_aloj_basico_extender_iter_palabra(uint64_t *ocupado, void *data)
{
    (void) data;
    *ocupado = UINT64_MAX;
    return true;
}

static void pdcrt_aloj_basico_extender(pdcrt_aloj_basico *basico, pdcrt_aloj_basico_pagina *p, size_t offset, size_t bytes)
{
    assert(offset + bytes <= basico->cfg.tam_pagina);
    assert(p->pagina);
    pdcrt_aloj_basico_para_cada_bit(basico, p,
                                    offset, bytes,
                                    &pdcrt_aloj_basico_extender_iter_bit,
                                    &pdcrt_aloj_basico_extender_iter_palabra,
                                    NULL);
}

static bool pdcrt_aloj_basico_marcar_como_desocupado_iter_bit(uint64_t *ocupado, uint8_t bit, bool val, void *data)
{
    (void) val;
    (void) data;
    assert((*ocupado & (UINT64_C(1) << (uint64_t) bit)) != 0);
    *ocupado = *ocupado & ~(UINT64_C(1) << (uint64_t) bit);
    return true;
}

static bool pdcrt_aloj_basico_marcar_como_desocupado_iter_palabra(uint64_t *ocupado, void *data)
{
    (void) data;
    assert(*ocupado == UINT64_MAX);
    *ocupado = 0;
    return true;
}

static void pdcrt_aloj_basico_marcar_como_desocupado(pdcrt_aloj_basico *basico, pdcrt_aloj_basico_pagina *p, size_t offset, size_t bytes)
{
    assert(offset + bytes <= basico->cfg.tam_pagina);
    pdcrt_aloj_basico_para_cada_bit(basico, p,
                                    offset, bytes,
                                    &pdcrt_aloj_basico_marcar_como_desocupado_iter_bit,
                                    &pdcrt_aloj_basico_marcar_como_desocupado_iter_palabra,
                                    NULL);
}

static pdcrt_aloj_basico_pagina *pdcrt_aloj_basico_buscar_pagina(pdcrt_aloj_basico *basico, void *ptr, size_t *pagina, size_t *offset)
{
    char *cptr = ptr;
    *pagina = *offset = 0;
    for(size_t i = 0; i < basico->num_paginas; i++)
    {
        pdcrt_aloj_basico_pagina* p = pdcrt_aloj_basico_obtener_pagina(basico, i);
        if(!p)
            return NULL;
        // Técnicamente, lo siguiente tiene UB: no es legal comparar un puntero de un objeto con un puntero de otro
        // objeto (en este caso, estamos comparando ptr con todas las páginas, incluso a las que no pertenece).
        // Dudo mucho que los compiladores sean tan, pero tan malvados como para optimizar mal este caso, dado
        // que sé que hay mucho software en el mundo que depende de este comportamiento. Sin embargo, sería
        // ideal corregirlo en un futuro.
        if(p->pagina && cptr >= p->pagina && cptr <= p->pagina + basico->cfg.tam_pagina)
        {
            *pagina = i;
            *offset = cptr - p->pagina;
            return p;
        }
    }
    return NULL;
}

static void *pdcrt_alojar_basico(void *yo, size_t bytes)
{
    pdcrt_aloj_basico *basico = yo;

    if(bytes > basico->cfg.tam_pagina)
    {
        return pdcrt_alojar(basico->base, bytes);
    }

    size_t pagina, offset;
    pdcrt_aloj_basico_pagina *p;
    if(!pdcrt_aloj_basico_buscar_libre(basico, bytes, &pagina, &offset))
    {
        if(!pdcrt_aloj_basico_crear_pagina(basico))
            return NULL;
    }
    p = pdcrt_aloj_basico_obtener_pagina(basico, pagina);
    if(!p)
        return NULL;
    void *res = &p->pagina[offset];
    pdcrt_aloj_basico_marcar_como_ocupado(basico, p, offset, bytes);
    return res;
}

static void *pdcrt_realojar_basico(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    pdcrt_aloj_basico *basico = yo;

    if(!ptr)
    {
        assert(tam_actual == 0);
        return pdcrt_alojar_basico(yo, tam_nuevo);
    }

    bool act_ov = tam_actual > basico->cfg.tam_pagina;
    bool nuevo_ov = tam_nuevo > basico->cfg.tam_pagina;
    if(act_ov && nuevo_ov)
    {
        return pdcrt_realojar(basico->base, ptr, tam_actual, tam_nuevo);
    }
    else if((act_ov && !nuevo_ov) || (!act_ov && nuevo_ov))
    {
        void *nuevo = pdcrt_alojar_basico(yo, tam_nuevo);
        if(nuevo)
        {
            memcpy(nuevo, ptr, tam_actual < tam_nuevo ? tam_actual : tam_nuevo);
            pdcrt_desalojar_basico(yo, ptr, tam_actual);
        }
        return nuevo;
    }
    else
    {
        size_t pagina, offset;
        pdcrt_aloj_basico_pagina* p = pdcrt_aloj_basico_buscar_pagina(basico, ptr, &pagina, &offset);
        if(!p)
        {
            assert(0 && u8"estado del alojador de páginas inválido: alojación de tamaño ideal mas no se encontró entre las páginas");
            return NULL;
        }

        if(offset + tam_nuevo < basico->cfg.tam_pagina && pdcrt_aloj_basico_puede_extender(basico, p, offset, tam_actual, tam_nuevo))
        {
            pdcrt_aloj_basico_extender(basico, p, offset, tam_nuevo);
            return ptr;
        }
        else
        {
            void *nuevo = pdcrt_alojar_basico(yo, tam_nuevo);
            if(nuevo)
            {
                memcpy(nuevo, ptr, tam_actual < tam_nuevo ? tam_actual : tam_nuevo);
                pdcrt_desalojar_basico(yo, ptr, tam_actual);
            }
            return nuevo;
        }
    }
}

static void pdcrt_desalojar_basico(void *yo, void *ptr, size_t tam_actual)
{
    if(!ptr)
        return;

    pdcrt_aloj_basico *basico = yo;
    if(tam_actual > basico->cfg.tam_pagina)
    {
        pdcrt_desalojar(basico->base, ptr, tam_actual);
    }
    else
    {
        size_t pagina, offset;
        pdcrt_aloj_basico_pagina* p = pdcrt_aloj_basico_buscar_pagina(basico, ptr, &pagina, &offset);
        if(!p)
        {
            assert(0 && u8"estado del alojador de páginas inválido: alojación de tamaño ideal mas no se encontró entre las páginas");
            return;
        }

        pdcrt_aloj_basico_marcar_como_desocupado(basico, p, offset, tam_actual);
    }
}

typedef struct pdcrt_aloj_con_estadisticas
{
    pdcrt_aloj aloj;
    pdcrt_aloj *base;
    size_t alojado;
} pdcrt_aloj_con_estadisticas;

static void *pdcrt_alojar_con_estadisticas(void *yo, size_t bytes);
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

static void *pdcrt_alojar_con_estadisticas(void *yo, size_t bytes)
{
    pdcrt_aloj_con_estadisticas *est = yo;
    void *res = pdcrt_alojar(est->base, bytes);
    if(res)
        est->alojado += bytes;
    return res;
}

static void *pdcrt_realojar_con_estadisticas(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    pdcrt_aloj_con_estadisticas *est = yo;
    void *res = pdcrt_realojar(est->base, ptr, tam_actual, tam_nuevo);
    if(res)
    {
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
        est->alojado -= tam_actual;
    }
    pdcrt_desalojar(est->base, ptr, tam_actual);
}



#define pdcrt_objeto_entero(i) ((pdcrt_obj) { .recv = &pdcrt_recv_entero, .ival = (i) })
#define pdcrt_objeto_float(f) ((pdcrt_obj) { .recv = &pdcrt_recv_float, .fval = (f) })
#define pdcrt_objeto_booleano(b) ((pdcrt_obj) { .recv = &pdcrt_recv_booleano, .bval = (b) })
#define pdcrt_objeto_marco(m) ((pdcrt_obj) { .recv = &pdcrt_recv_marco, .marco = (m) })
#define pdcrt_objeto_texto(txt) ((pdcrt_obj) { .recv = &pdcrt_recv_texto, .texto = (txt) })
#define pdcrt_objeto_nulo() ((pdcrt_obj) { .recv = &pdcrt_recv_nulo })
#define pdcrt_objeto_arreglo(arr) ((pdcrt_obj) { .recv = &pdcrt_recv_arreglo, .arreglo = (arr) })
#define pdcrt_objeto_closure(cls) ((pdcrt_obj) { .recv = &pdcrt_recv_closure, .closure = (cls) })
#define pdcrt_objeto_caja(cj) ((pdcrt_obj) { .recv = &pdcrt_recv_caja, .caja = (cj) })
#define pdcrt_objeto_tabla(tbl) ((pdcrt_obj) { .recv = &pdcrt_recv_tabla, .tabla = (tbl) })
#define pdcrt_objeto_runtime() ((pdcrt_obj) { .recv = &pdcrt_recv_runtime })
#define pdcrt_objeto_voidptr(ptr) ((pdcrt_obj) { .recv = &pdcrt_recv_voidptr, .pval = (ptr) })
#define pdcrt_objeto_valop(vo) ((pdcrt_obj) { .recv = &pdcrt_recv_valop, .valop = (vo) })
#define pdcrt_objeto_espacio_de_nombres(tbl) ((pdcrt_obj) { .recv = &pdcrt_recv_espacio_de_nombres, .tabla = (tbl) })
#define pdcrt_objeto_corrutina(corrutina) ((pdcrt_obj) { .recv = &pdcrt_recv_corrutina, .coro = (corrutina) })
#define pdcrt_objeto_instancia(instancia) ((pdcrt_obj) { .recv = &pdcrt_recv_instancia, .inst = (instancia) })
#define pdcrt_objeto_reubicado(reub) ((pdcrt_obj) { .recv = &pdcrt_recv_reubicado, .reubicado = (reub) })


static size_t pdcrt_liberar_tabla(pdcrt_ctx *ctx, pdcrt_bucket *lista, size_t tam_lista);


static void pdcrt_gc_eliminar_de_grupo(pdcrt_gc_grupo *grupo, pdcrt_cabecera_gc *h)
{
    assert(h->grupo == grupo->grupo);
    assert(grupo->grupo != PDCRT_TGRP_NINGUNO);
    if(h->anterior)
        h->anterior->siguiente = h->siguiente;
    if(h->siguiente)
        h->siguiente->anterior = h->anterior;
    if(h == grupo->primero)
        grupo->primero = h->siguiente;
    if(h == grupo->ultimo)
        grupo->ultimo = h->anterior;
    h->anterior = h->siguiente = NULL;
    h->grupo = PDCRT_TGRP_NINGUNO;
}

static void pdcrt_gc_agregar_a_grupo(pdcrt_gc_grupo *grupo, pdcrt_cabecera_gc *h)
{
    assert(!h->anterior);
    assert(!h->siguiente);
    assert(h->grupo == PDCRT_TGRP_NINGUNO);
    assert(grupo->grupo != PDCRT_TGRP_NINGUNO);
    if(grupo->ultimo)
    {
        grupo->ultimo->siguiente = h;
        h->anterior = grupo->ultimo;
        grupo->ultimo = h;
    }
    else
    {
        assert(!grupo->primero);
        grupo->primero = grupo->ultimo = h;
    }
    h->grupo = grupo->grupo;
}

void pdcrt_gc_mover_a_grupo(pdcrt_gc_grupo *desde, pdcrt_gc_grupo *hacia, pdcrt_cabecera_gc *h)
{
    pdcrt_gc_eliminar_de_grupo(desde, h);
    pdcrt_gc_agregar_a_grupo(hacia, h);
}

extern inline void pdcrt_barrera_de_escritura_cabecera(pdcrt_ctx *ctx, pdcrt_cabecera_gc *ch, pdcrt_cabecera_gc *vh);
extern inline void pdcrt_barrera_de_escritura(pdcrt_ctx *ctx, pdcrt_obj contenedor, pdcrt_obj valor);

typedef enum pdcrt_tipo_recoleccion
{
    PDCRT_RECOLECCION_SIN_PILA,
    PDCRT_RECOLECCION_SIN_MEMORIA,
} pdcrt_tipo_recoleccion;

typedef struct pdcrt_recoleccion
{
    pdcrt_tipo_recoleccion tipo;
    union
    {
        struct
        {
            size_t memoria_requerida;
        } sin_memoria;
    };
} pdcrt_recoleccion;

static pdcrt_recoleccion pdcrt_gc_recoleccion_por_pila(pdcrt_ctx *ctx)
{
    (void) ctx;
    return (pdcrt_recoleccion) {
        .tipo = PDCRT_RECOLECCION_SIN_PILA,
    };
}

static pdcrt_recoleccion pdcrt_gc_recoleccion_por_memoria(pdcrt_ctx *ctx, size_t memoria_requerida)
{
    (void) ctx;
    return (pdcrt_recoleccion) {
        .tipo = PDCRT_RECOLECCION_SIN_MEMORIA,
        .sin_memoria = {
            .memoria_requerida = memoria_requerida,
        },
    };
}

PDCRT_INLINE static bool pdcrt_gc_recoleccion_debe_mover_pila(pdcrt_recoleccion r)
{
    return r.tipo == PDCRT_RECOLECCION_SIN_PILA;
}

PDCRT_INLINE static bool pdcrt_gc_recoleccion_es_mayor(pdcrt_recoleccion r)
{
    return r.tipo == PDCRT_RECOLECCION_SIN_MEMORIA;
}

PDCRT_INLINE static bool pdcrt_gc_recoleccion_es_menor(pdcrt_recoleccion r)
{
    return r.tipo == PDCRT_RECOLECCION_SIN_PILA;
}

static void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_recoleccion params);

static void pdcrt_intenta_invocar_al_recolector(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_recoleccion params)
{
    if(ctx->recolector_de_basura_activo)
    {
        size_t usado = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
        if(usado >= ctx->gc.tam_heap)
        {
            pdcrt_recolectar_basura_simple(ctx, m, params);
            size_t usado = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
            if(usado >= ((ctx->gc.tam_heap / 2) * 1))
            {
                ctx->gc.tam_heap = usado + 20 * 1024 * 1024; // +20MiB
                ctx->gc.num_recolecciones = 0;
            }
            else if(usado <= (ctx->gc.tam_heap / 2))
            {
                if(ctx->gc.num_recolecciones >= 10)
                {
                    ctx->gc.tam_heap = ctx->gc.tam_heap / 2;
                    ctx->gc.num_recolecciones = 0;
                }
                else
                {
                    ctx->gc.num_recolecciones += 1;
                }
            }
            else
            {
                ctx->gc.num_recolecciones = 0;
            }
        }
    }
}

static void pdcrt_activar_recolector_de_basura(pdcrt_ctx *ctx)
{
    ctx->recolector_de_basura_activo = true;
}

static void pdcrt_desactivar_recolector_de_basura(pdcrt_ctx *ctx)
{
    ctx->recolector_de_basura_activo = false;
}

static void pdcrt_inicializar_obj(pdcrt_ctx *ctx,
                                  pdcrt_cabecera_gc *h,
                                  pdcrt_tipo_obj_gc tipo,
                                  size_t sz)
{
    h->tipo = tipo;
    h->grupo = PDCRT_TGRP_NINGUNO;
    h->en_la_pila = false;
    h->anterior = h->siguiente = NULL;
    h->num_bytes = sz;
    pdcrt_gc_agregar_a_grupo(&ctx->gc.blanco_joven, h);
}

static void *pdcrt_alojar_obj(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_tipo_obj_gc tipo, size_t sz)
{
    pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_memoria(ctx, sz);
    pdcrt_intenta_invocar_al_recolector(ctx, m, params);
    pdcrt_cabecera_gc *h = pdcrt_alojar_ctx(ctx, sz);
    if(!h)
        pdcrt_error(ctx, u8"No se pudo alojar más memoria");
    pdcrt_inicializar_obj(ctx, h, tipo, sz);
    return h;
}



pdcrt_cabecera_gc *pdcrt_gc_cabecera_de(pdcrt_obj o)
{
    switch(pdcrt_tipo_de_obj(o))
    {
        case PDCRT_TOBJ_NULO:
        case PDCRT_TOBJ_ENTERO:
        case PDCRT_TOBJ_FLOAT:
        case PDCRT_TOBJ_BOOLEANO:
        case PDCRT_TOBJ_RUNTIME:
        case PDCRT_TOBJ_VOIDPTR:
            return NULL;
        case PDCRT_TOBJ_MARCO:
        case PDCRT_TOBJ_TEXTO:
        case PDCRT_TOBJ_ARREGLO:
        case PDCRT_TOBJ_CLOSURE:
        case PDCRT_TOBJ_CAJA:
        case PDCRT_TOBJ_TABLA:
        case PDCRT_TOBJ_ESPACIO_DE_NOMBRES:
        case PDCRT_TOBJ_VALOP:
        case PDCRT_TOBJ_CORRUTINA:
        case PDCRT_TOBJ_INSTANCIA:
        case PDCRT_TOBJ_REUBICADO:
            return o.gc;
    }
    assert(0 && "inalcanzable");
}

typedef struct pdcrt_objeto_generico_gc
{
    pdcrt_cabecera_gc gc;
    char bytes[];
} pdcrt_objeto_generico_gc;

static pdcrt_cabecera_gc *pdcrt_gc_reubicar(pdcrt_ctx *ctx, pdcrt_cabecera_gc *h)
{
    assert(h->grupo == PDCRT_TGRP_BLANCO_VIEJO
        || h->grupo == PDCRT_TGRP_BLANCO_JOVEN
        || h->grupo == PDCRT_TGRP_BLANCO_EN_LA_PILA);

    bool gc_activo = ctx->recolector_de_basura_activo;
    if(gc_activo)
        pdcrt_desactivar_recolector_de_basura(ctx);
    // El marco pasado a pdcrt_alojar_obj solo es usado para marcar los valores
    // cuando se recolecta la basura, pero si el recolector está desactivado
    // entonces nunca es usado, por lo que es seguro pasar NULL.
    pdcrt_objeto_generico_gc *nuevo = pdcrt_alojar_obj(ctx, NULL, h->tipo, h->num_bytes);
    if(gc_activo)
        pdcrt_activar_recolector_de_basura(ctx);
    if(!nuevo)
        pdcrt_enomem(ctx);
    nuevo->gc.tipo = h->tipo;
    nuevo->gc.num_bytes = h->num_bytes;

    pdcrt_objeto_generico_gc *actual = (pdcrt_objeto_generico_gc *) h;
    size_t tam_cuerpo = h->num_bytes - offsetof(pdcrt_objeto_generico_gc, bytes);
    memcpy(&nuevo->bytes, &actual->bytes, tam_cuerpo);

    h->tipo = PDCRT_TGC_REUBICADO;
    pdcrt_reubicado *reub = (pdcrt_reubicado *) h;
    reub->nueva_direccion = PDCRT_CABECERA_GC(nuevo);
    return reub->nueva_direccion;
}

static void pdcrt_gc_visitar_contenido(
    pdcrt_ctx *ctx,
    pdcrt_cabecera_gc *h,
    pdcrt_recoleccion params,
    void (*f)(pdcrt_ctx *ctx, pdcrt_cabecera_gc **h, pdcrt_recoleccion params),
    void (*f_obj)(pdcrt_ctx *ctx, pdcrt_obj *o, pdcrt_recoleccion params)
)
{
    switch(h->tipo)
    {
        case PDCRT_TGC_TEXTO:
        case PDCRT_TGC_VALOP:
            break;
        case PDCRT_TGC_MARCO:
        {
            pdcrt_marco *m = (pdcrt_marco *) h;
            if(m->k.marco)
                f(ctx, PDCRT_CABECERA_GC_PTR(&m->k.marco), params);
            for(size_t i = 0; i < (m->num_locales + m->num_capturas); i++)
                f_obj(ctx, &m->locales_y_capturas[i], params);
            break;
        }
        case PDCRT_TGC_ARREGLO:
        {
            pdcrt_arreglo *a = (pdcrt_arreglo *) h;
            for(size_t i = 0; i < a->longitud; i++)
                f_obj(ctx, &a->valores[i], params);
            break;
        }
        case PDCRT_TGC_CLOSURE:
        {
            pdcrt_closure *c = (pdcrt_closure *) h;
            for(size_t i = 0; i < c->num_capturas; i++)
                f_obj(ctx, &c->capturas[i], params);
            break;
        }
        case PDCRT_TGC_CAJA:
        {
            pdcrt_caja *c = (pdcrt_caja *) h;
            f_obj(ctx, &c->valor, params);
            break;
        }
        case PDCRT_TGC_TABLA:
        {
            pdcrt_tabla *tbl = (pdcrt_tabla *) h;
            for(size_t i = 0; i < tbl->num_buckets; i++)
            {
                pdcrt_bucket *b = &tbl->buckets[i];
                if(!b->activo)
                    continue;
                while(b)
                {
                    f_obj(ctx, &b->llave, params);
                    f_obj(ctx, &b->valor, params);
                    b = b->siguiente_colision;
                }
            }
            break;
        }
        case PDCRT_TGC_CORO:
        {
            pdcrt_corrutina *coro = (pdcrt_corrutina *) h;
            if(coro->estado == PDCRT_CORO_INICIAL)
            {
                f_obj(ctx, &coro->punto_de_inicio, params);
            }
            else if(coro->estado == PDCRT_CORO_SUSPENDIDA)
            {
                f(ctx, PDCRT_CABECERA_GC_PTR(&coro->punto_de_suspencion.marco), params);
            }
            else if(coro->estado == PDCRT_CORO_EJECUTANDOSE)
            {
                f(ctx, PDCRT_CABECERA_GC_PTR(&coro->punto_de_continuacion.marco), params);
            }
            break;
        }
        case PDCRT_TGC_INSTANCIA:
        {
            pdcrt_instancia *inst = (pdcrt_instancia *) h;
            f_obj(ctx, &inst->metodos, params);
            f_obj(ctx, &inst->metodo_no_encontrado, params);
            for(size_t i = 0; i < inst->num_atributos; i++)
            {
                f_obj(ctx, &inst->atributos[i], params);
            }
            break;
        }
        default:
            assert(0 && "inalcanzable");
    }
}


static void pdcrt_gc_debe_ser_alcanzable_vis(pdcrt_ctx *ctx, pdcrt_cabecera_gc **h, pdcrt_recoleccion params);
static void pdcrt_gc_debe_ser_alcanzable_vis_obj(pdcrt_ctx *ctx, pdcrt_obj *o, pdcrt_recoleccion params);

static void pdcrt_gc_intenta_mover_a_gris(pdcrt_ctx *ctx,
                                          pdcrt_cabecera_gc *h,
                                          pdcrt_recoleccion params)
{
    bool es_mayor = pdcrt_gc_recoleccion_es_mayor(params);

    if(h->grupo == PDCRT_TGRP_BLANCO_VIEJO)
    {
        if(es_mayor)
        {
            pdcrt_gc_mover_a_grupo(&ctx->gc.blanco_viejo, &ctx->gc.gris, h);
        }
        else
        {
            // Si la recolección es menor, no recolectaremos ningún blanco viejo,
            // por eso no los marcamos.
#ifdef PDCRT_DBG_GC
            pdcrt_gc_visitar_contenido(ctx, h, params,
                                       &pdcrt_gc_debe_ser_alcanzable_vis,
                                       &pdcrt_gc_debe_ser_alcanzable_vis_obj);
#endif
        }
    }
    else if(h->grupo == PDCRT_TGRP_BLANCO_JOVEN)
    {
        pdcrt_gc_mover_a_grupo(&ctx->gc.blanco_joven, &ctx->gc.gris, h);
    }
    else if(h->grupo == PDCRT_TGRP_BLANCO_EN_LA_PILA)
    {
        pdcrt_gc_mover_a_grupo(&ctx->gc.blanco_en_la_pila, &ctx->gc.gris, h);
    }
    else if(h->grupo == PDCRT_TGRP_RAICES_VIEJAS)
    {
        pdcrt_gc_mover_a_grupo(&ctx->gc.raices_viejas, &ctx->gc.gris, h);
    }
    else
    {
        assert(h->grupo == PDCRT_TGRP_GRIS || h->grupo == PDCRT_TGRP_NEGRO);
    }
}

static void pdcrt_gc_debe_ser_alcanzable_vis(pdcrt_ctx *ctx, pdcrt_cabecera_gc **h, pdcrt_recoleccion params)
{
    (void) ctx;
    pdcrt_gc_tipo_grupo g = (*h)->grupo;
    bool es_mayor = pdcrt_gc_recoleccion_es_mayor(params);
    if(es_mayor)
    {
        assert(g != PDCRT_TGRP_NEGRO || g != PDCRT_TGRP_GRIS);
    }
    else
    {
        assert(g != PDCRT_TGRP_BLANCO_VIEJO || g != PDCRT_TGRP_NEGRO || g != PDCRT_TGRP_GRIS);
    }
}

static void pdcrt_gc_debe_ser_alcanzable_vis_obj(pdcrt_ctx *ctx, pdcrt_obj *o, pdcrt_recoleccion params)
{
    pdcrt_cabecera_gc *h = pdcrt_gc_cabecera_de(*o);
    if(h)
        pdcrt_gc_debe_ser_alcanzable_vis(ctx, &o->gc, params);
}

static void pdcrt_gc_no_debe_ser_blanco_vis(pdcrt_ctx *ctx, pdcrt_cabecera_gc **h, pdcrt_recoleccion params);
static void pdcrt_gc_no_debe_ser_blanco_vis_obj(pdcrt_ctx *ctx, pdcrt_obj *o, pdcrt_recoleccion params);

PDCRT_NOINLINE static void pdcrt_gc_marcar(pdcrt_ctx *ctx,
                                           pdcrt_cabecera_gc **h_ptr,
                                           pdcrt_recoleccion params)
{
    pdcrt_cabecera_gc *h = *h_ptr;
    assert(h->grupo != PDCRT_TGRP_NINGUNO);

    if(h->tipo == PDCRT_TGC_REUBICADO)
    {
        pdcrt_reubicado *reub = (pdcrt_reubicado *) h;
        *h_ptr = reub->nueva_direccion;
        h = *h_ptr;
    }
    else
    {
        bool mover_pila = pdcrt_gc_recoleccion_debe_mover_pila(params);
        if(mover_pila && h->en_la_pila)
        {
            h = *h_ptr = pdcrt_gc_reubicar(ctx, h);
        }
    }

    if(h->grupo == PDCRT_TGRP_NEGRO)
    {
#ifdef PDCRT_DBG_GC
        pdcrt_gc_visitar_contenido(ctx, h, params,
                                   &pdcrt_gc_no_debe_ser_blanco_vis,
                                   &pdcrt_gc_no_debe_ser_blanco_vis_obj);
#endif
    }
    else
    {
        pdcrt_gc_intenta_mover_a_gris(ctx, h, params);
    }
}

static void pdcrt_gc_marcar_obj(pdcrt_ctx *ctx, pdcrt_obj *campo, pdcrt_recoleccion params)
{
    pdcrt_cabecera_gc *h = pdcrt_gc_cabecera_de(*campo);
    if(h)
        pdcrt_gc_marcar(ctx, &campo->gc, params);
}

static void pdcrt_gc_no_debe_ser_blanco_vis(pdcrt_ctx *ctx, pdcrt_cabecera_gc **h, pdcrt_recoleccion params)
{
    (void) ctx;
    pdcrt_gc_tipo_grupo g = (*h)->grupo;
    bool es_mayor = pdcrt_gc_recoleccion_es_mayor(params);
    if(es_mayor)
    {
        // Los objetos blancos de la segunda generación no son recolectados en colecciones menores, así que está bien
        // que haya referencias a ellos desde los objetos grises
        assert(g != PDCRT_TGRP_BLANCO_VIEJO || g != PDCRT_TGRP_BLANCO_JOVEN || g != PDCRT_TGRP_BLANCO_EN_LA_PILA);
    }
    else
    {
        assert(g != PDCRT_TGRP_BLANCO_JOVEN || g != PDCRT_TGRP_BLANCO_EN_LA_PILA);
    }
}

static void pdcrt_gc_no_debe_ser_blanco_vis_obj(pdcrt_ctx *ctx, pdcrt_obj *o, pdcrt_recoleccion params)
{
    pdcrt_cabecera_gc *h = pdcrt_gc_cabecera_de(*o);
    if(h)
        pdcrt_gc_no_debe_ser_blanco_vis(ctx, &o->gc, params);
}

PDCRT_NOINLINE static void pdcrt_gc_procesar_gris(pdcrt_ctx *ctx,
                                                  pdcrt_cabecera_gc *h,
                                                  pdcrt_recoleccion params)
{
    assert(h->grupo == PDCRT_TGRP_GRIS);

    pdcrt_gc_visitar_contenido(ctx, h, params,
                               &pdcrt_gc_marcar,
                               &pdcrt_gc_marcar_obj);
    pdcrt_gc_mover_a_grupo(&ctx->gc.gris, &ctx->gc.negro, h);
}

static void pdcrt_gc_marcar_raiz(pdcrt_ctx *ctx, pdcrt_cabecera_gc **h, pdcrt_recoleccion params)
{
    if(*h)
        pdcrt_gc_marcar(ctx, h, params);
}

static void pdcrt_gc_marcar_raiz_obj(pdcrt_ctx *ctx, pdcrt_obj *obj, pdcrt_recoleccion params)
{
    pdcrt_gc_marcar_obj(ctx, obj, params);
}

static void pdcrt_gc_marcar_y_mover_todo(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_recoleccion params)
{
    for(size_t i = 0; i < ctx->tam_pila; i++)
        pdcrt_gc_marcar_raiz_obj(ctx, &ctx->pila[i], params);

    if(m && *m)
        pdcrt_gc_marcar_raiz(ctx, PDCRT_CABECERA_GC_PTR(m), params);

    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->funcion_igualdad, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->funcion_hash, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->registro_de_espacios_de_nombres, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->registro_de_modulos, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->espacio_de_nombres_runtime, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->nombre_del_programa, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->argv, params);

    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_objeto, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_numero, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_arreglo, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_boole, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_procedimiento, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_texto, params);
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_tipo_nulo, params);

    if(ctx->continuacion_actual.marco)
    {
        pdcrt_gc_marcar_raiz(ctx, PDCRT_CABECERA_GC_PTR(&ctx->continuacion_actual.marco), params);
    }

#define PDCRT_X(nombre, _texto)                                         \
    if(ctx->textos_globales.nombre)                                     \
        pdcrt_gc_marcar_raiz(ctx, PDCRT_CABECERA_GC_PTR(&ctx->textos_globales.nombre), params);
    PDCRT_TABLA_TEXTOS(PDCRT_X);
#undef PDCRT_X

    for(pdcrt_cabecera_gc *h = ctx->gc.raices_viejas.primero; h != NULL;)
    {
        pdcrt_cabecera_gc *s = h->siguiente;
        pdcrt_cabecera_gc *p = h;
        assert(h->grupo == PDCRT_TGRP_RAICES_VIEJAS);
        pdcrt_gc_marcar_raiz(ctx, &p, params);
        h = s;
    }
}

static size_t
pdcrt_gc_liberar_objeto(pdcrt_ctx *ctx, pdcrt_cabecera_gc *h)
{
    assert(h->grupo == PDCRT_TGRP_NINGUNO);

    size_t liberado = 0;
    if(h->tipo == PDCRT_TGC_TEXTO)
    {
        // Elimínalo de la lista de textos
        for(size_t i = 0; i < ctx->tam_textos; i++)
        {
            if(ctx->textos[i] == (pdcrt_texto *) h)
            {
                for(size_t j = i + 1; j < ctx->tam_textos; j++)
                {
                    ctx->textos[j - 1] = ctx->textos[j];
                }
                ctx->tam_textos -= 1;
                break;
            }
        }
    }
    else if(h->tipo == PDCRT_TGC_ARREGLO)
    {
        pdcrt_arreglo *a = (pdcrt_arreglo *) h;
        size_t tam = sizeof(pdcrt_obj) * a->capacidad;
        pdcrt_desalojar_ctx(ctx, a->valores, tam);
        liberado += tam;
    }
    else if(h->tipo == PDCRT_TGC_TABLA)
    {
        pdcrt_tabla *tbl = (pdcrt_tabla *) h;
        liberado += pdcrt_liberar_tabla(ctx, tbl->buckets, tbl->num_buckets);
    }

    if(!h->en_la_pila)
    {
        liberado += h->num_bytes;
        pdcrt_desalojar_ctx(ctx, h, h->num_bytes);
    }
    return liberado;
}

static size_t pdcrt_gc_recolectar(pdcrt_ctx *ctx, pdcrt_recoleccion params)
{
    bool es_mayor = pdcrt_gc_recoleccion_es_mayor(params);
    size_t liberado = 0;

    for(pdcrt_cabecera_gc *h = ctx->gc.blanco_en_la_pila.primero; h != NULL;)
    {
        pdcrt_cabecera_gc *s = h->siguiente;
        assert(h->grupo == PDCRT_TGRP_BLANCO_EN_LA_PILA);
        pdcrt_gc_eliminar_de_grupo(&ctx->gc.blanco_en_la_pila, h);
        liberado += pdcrt_gc_liberar_objeto(ctx, h);
        h = s;
    }

    for(pdcrt_cabecera_gc *h = ctx->gc.blanco_joven.primero; h != NULL;)
    {
        pdcrt_cabecera_gc *s = h->siguiente;
        assert(h->grupo == PDCRT_TGRP_BLANCO_JOVEN);
        pdcrt_gc_eliminar_de_grupo(&ctx->gc.blanco_joven, h);
        liberado += pdcrt_gc_liberar_objeto(ctx, h);
        h = s;
    }

    if(es_mayor)
    {
        for(pdcrt_cabecera_gc *h = ctx->gc.blanco_viejo.primero; h != NULL;)
        {
            pdcrt_cabecera_gc *s = h->siguiente;
            assert(h->grupo == PDCRT_TGRP_BLANCO_VIEJO);
            pdcrt_gc_eliminar_de_grupo(&ctx->gc.blanco_viejo, h);
            liberado += pdcrt_gc_liberar_objeto(ctx, h);
            h = s;
        }
    }

    return liberado;
}

static void pdcrt_gc_marcar_y_mover_todos_los_grises(pdcrt_ctx *ctx, pdcrt_recoleccion params)
{
    for(pdcrt_cabecera_gc *h = ctx->gc.gris.primero; h; h = h->siguiente)
    {
        assert(h->grupo == PDCRT_TGRP_GRIS);
        pdcrt_gc_procesar_gris(ctx, h, params);
    }
}

static void pdcrt_gc_mover_negros_a_blancos(pdcrt_ctx *ctx, pdcrt_recoleccion params)
{
    (void) params;
    for(pdcrt_cabecera_gc *h = ctx->gc.negro.primero; h;)
    {
        assert(h->grupo == PDCRT_TGRP_NEGRO);
        assert(h->tipo != PDCRT_TGC_REUBICADO);
        pdcrt_cabecera_gc *s = h->siguiente;
        if(h->en_la_pila)
            pdcrt_gc_mover_a_grupo(&ctx->gc.negro, &ctx->gc.blanco_en_la_pila, h);
        else
            pdcrt_gc_mover_a_grupo(&ctx->gc.negro, &ctx->gc.blanco_viejo, h);
        h = s;
    }
}

static void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx,
                                           pdcrt_marco **m,
                                           pdcrt_recoleccion params)
{
    struct timespec inicio, marcar, recolectar, total;
    pdcrt_timediff dif_marcar, dif_recolectar, dif_total;
    size_t mem_usada_al_inicio = 0, mem_usada_al_final = 0;
    char buffer[PDCRT_FORMATEAR_BYTES_TAM_BUFFER];

    if(ctx->log.gc && PDCRT_LOG_COMPILADO)
    {
        mem_usada_al_inicio = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
        pdcrt_formatear_bytes(buffer, mem_usada_al_inicio);
        pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "inicio GC: %s\n", buffer);

        pdcrt_formatear_bytes(buffer, ctx->gc.tam_heap);
        pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "     heap: %s\n", buffer);

        if(ctx->capacidades.time)
            pdcrt_time(&inicio);
    }

    pdcrt_gc_marcar_y_mover_todo(ctx, m, params);
    while(ctx->gc.gris.primero)
    {
        pdcrt_gc_marcar_y_mover_todos_los_grises(ctx, params);
    }

    if(ctx->log.gc && PDCRT_LOG_COMPILADO && ctx->capacidades.time)
    {
        pdcrt_time(&marcar);
        pdcrt_diferencia(&inicio, &marcar, &dif_marcar);
        pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "marcar: %ld.%03ld (%03ld %03ld)\n", dif_marcar.dif_s, dif_marcar.dif_ms, dif_marcar.dif_us, dif_marcar.dif_ns);
    }

    pdcrt_gc_recolectar(ctx, params);

    if(ctx->log.gc && PDCRT_LOG_COMPILADO && ctx->capacidades.time)
    {
        pdcrt_time(&recolectar);
        pdcrt_diferencia(&inicio, &recolectar, &dif_recolectar);
        pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "recolectar: %ld.%03ld (%03ld %03ld)\n", dif_recolectar.dif_s, dif_recolectar.dif_ms, dif_recolectar.dif_us, dif_recolectar.dif_ns);
    }

    pdcrt_gc_mover_negros_a_blancos(ctx, params);

    if(ctx->log.gc && PDCRT_LOG_COMPILADO)
    {
        mem_usada_al_final = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
        pdcrt_formatear_bytes(buffer, mem_usada_al_final);
        if(ctx->capacidades.time)
        {
            pdcrt_time(&total);
            pdcrt_diferencia(&inicio, &total, &dif_total);
            pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "total: %ld.%03ld (%03ld %03ld)\n", dif_total.dif_s, dif_total.dif_ms, dif_total.dif_us, dif_total.dif_ns);
            pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "       mem: %s\n", buffer);
        }
        else
        {
            pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "total: mem: %s\n", buffer);
        }

        if(mem_usada_al_final > mem_usada_al_inicio)
        {
            pdcrt_formatear_bytes(buffer, mem_usada_al_final - mem_usada_al_inicio);
            pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "       delta: +%s\n", buffer);
        }
        else
        {
            pdcrt_formatear_bytes(buffer, mem_usada_al_inicio - mem_usada_al_final);
            pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "       delta: -%s\n", buffer);
        }
    }
}

void pdcrt_recolectar_basura_por_pila(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_pila(ctx);
    pdcrt_recolectar_basura_simple(ctx, m, params);
}


static pdcrt_texto* pdcrt_crear_nuevo_texto(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str, size_t len)
{
    pdcrt_texto *txt = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_TEXTO, sizeof(pdcrt_texto) + len + 1);
    if(!txt)
        pdcrt_enomem(ctx);
    txt->longitud = len;
    if(len > 0)
        memcpy(txt->contenido, str, len);
    txt->contenido[len] = '\0';
    return txt;
}

static int pdcrt_comparar_str(const char *s1, size_t l1, const char *s2, size_t l2)
{
    if(l1 < l2)
        return -1;
    else if(l1 > l2)
        return 1;
    else
        return memcmp(s1, s2, l1);
}

static pdcrt_texto* pdcrt_crear_texto(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str, size_t len)
{
    // Al alojar un texto nuevo (en caso de que no exista) se podría invocar al
    // recolector de basura. En ese caso, la lista de textos sobre la que
    // acabamos de buscar cambiaría, invalidando los índices que tenemos y
    // dañando todo. Para prevenir esto, llamo al recolector ahora mismo y lo
    // desactivo al crear el texto. No debería fallar por memoria ya que de
    // faltar memoria esta llamada debería abrir espacio.
    pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_memoria(ctx, 0);
    pdcrt_intenta_invocar_al_recolector(ctx, m, params);

    size_t lo = 0, hi = ctx->tam_textos;
    size_t p;
#define PDCRT_RECALCULAR_PIVOTE() p = (hi - lo) / 2 + lo
    PDCRT_RECALCULAR_PIVOTE();
    while(lo < hi)
    {
        pdcrt_texto *txt = ctx->textos[p];
        int c = pdcrt_comparar_str(txt->contenido, txt->longitud, str, len);
        if(c < 0)
        {
            hi = p;
            PDCRT_RECALCULAR_PIVOTE();
        }
        else if(c > 0)
        {
            lo = p + 1;
            PDCRT_RECALCULAR_PIVOTE();
        }
        else
        {
            return txt;
        }
    }
#undef PDCRT_RECALCULAR_PIVOTE

    if(ctx->cap_textos <= ctx->tam_textos)
    {
        size_t ncap = ctx->cap_textos * 2;
        if(ncap == 0)
            ncap = 1;
        pdcrt_texto **textos = pdcrt_realojar_ctx(ctx, ctx->textos,
            sizeof(pdcrt_texto *) * ctx->cap_textos,
            sizeof(pdcrt_texto *) * ncap);
        if(!textos)
            pdcrt_enomem(ctx);
        ctx->textos = textos;
        ctx->cap_textos = ncap;
    }

    const size_t exp_ind = lo;
    bool gc_activo = ctx->recolector_de_basura_activo;
    if(gc_activo)
        pdcrt_desactivar_recolector_de_basura(ctx);
    pdcrt_texto *txt = pdcrt_crear_nuevo_texto(ctx, m, str, len);
    if(gc_activo)
        pdcrt_activar_recolector_de_basura(ctx);
    for(ssize_t i = ctx->tam_textos - 1; i >= (ssize_t) exp_ind; i--)
    {
        ctx->textos[i + 1] = ctx->textos[i];
    }
    ctx->textos[exp_ind] = txt; // #1
    ctx->tam_textos += 1;
    return txt;
}

#define pdcrt_crear_texto_desde_cstr(ctx, m, cstr) \
    pdcrt_crear_texto(ctx, m, cstr, sizeof(cstr) - 1)

static bool pdcrt_comparar_textos(pdcrt_texto *a, pdcrt_texto *b)
{
    return pdcrt_comparar_str(a->contenido, a->longitud,
                              b->contenido, b->longitud) == 0;
}

// La mayoría de los tipos son fáciles de comparar (por igualdad). Sin embargo,
// los números nos presentan un problema: no todos los enteros son floats y no
// todos los floats son enteros.
//
// Específicamente, los «floats enteros» (como 3.0 o -7.0) no necesariamente
// pueden convertirse a un pdcrt_entero. Esto es debido a que los floats
// (incluso los «pequeños» de 32 bits) pueden representar enteros con
// magnitudes mucho mayores a PDCRT_ENTERO_MAX o menores a PDCRT_ENTERO_MIN.
//
// Además, como los floats pierden precisión rápidamente a medida que se alejan
// de 0, los enteros grandes tampoco pueden ser representados como floats. Por
// ejemplo, el entero 9223372036854775806L no puede ser representado como un
// float de 32 bits: es redondeado a 9223372036854775808.0f.
//
// No podemos realizar `ent == (pdcrt_entero) flt` porque `(pdcrt_entero) flt`
// podría hacer overflow, así que primero hay que verificar que `flt` se
// encuentre en el rango de los enteros. En teoría es tan sencillo como `flt >=
// (pdcrt_float) PDCRT_ENTERO_MIN && flt <= (pdcrt_float) PDCRT_ENTERO_MAX`,
// pero como ya dije ¡Algunos enteros son irrepresentables! Si PDCRT_ENTERO_MIN
// o PDCRT_ENTERO_MAX son irrepresentables entonces este condicional fallaría
// al aceptar más o menos valores de los esperados.
//
// Una solución sencilla, utilizada por Lua <http://www.lua.org> 5.4
// <http://www.lua.org/versions.html#5.4>, es la siguiente:
//
// 1. Determinamos cual es el rango de enteros que pueden ser representados por
// pdcrt_float sin pérdida de precisión.
//
// 2. Al comparar `ent` y `flt`, primero convertimos `ent` a un pdcrt_float *si
// se encuentra en este rango seguro*. Si este es el caso, comparamos con `flt
// == (pdcrt_float) ent`.
//
// 3. Si este no es el caso, tratamos de convertir `flt` a un entero mediante
// la condición anteriormente descrita (`flt >= (pdcrt_float) PDCRT_ENTERO_MIN
// && flt <= (pdcrt_float) PDCRT_ENTERO_MAX`).
//
// Lua no maneja el caso en el que PDCRT_ENTERO_MAX no pueda ser un float. En
// cambio, ellos documentan que este caso está «indefinído».
//
// Python <https://www.python.org> 3.13.0a1 (commit
// 1c7ed7e9ebc53290c831d7b610219fa737153a1b) implementa la siguiente estrategia
// (función `float_richcompare`, `Objects/floatobject.c` línea 414):
//
// 1. Si tienen signos distintos, no son iguales.
//
// 2. Si el float es NaN o ±inf, no son iguales.
//
// 3. Si el entero tiene menos bits que pdcrt_float tiene de precisión, `flt ==
// (pdcrt_float) ent`. `(pdcrt_float) ent` siempre podrá representar `ent`.
//
// Recuerda que todos los números en coma flotante son de la forma `flt === sig
// * 2**exp`. La función `frexp(3)` te permite obtener `sig` y `exp`. Si
// ninguno de los casos anteriores funciona, Python procede a desestructurar
// `flt` en sus componentes `sig` y `exp` y a compararlos con `ent`.
//
// Este método (el de Python) es el que decidí utilizar.

// Sacado de
// <https://stackoverflow.com/questions/64842669/how-to-test-if-a-target-has-twos-complement-integers-with-the-c-preprocessor>
_Static_assert((-1 & 3) == 3,
               "tu compilador debe implementar los enteros como números en complemento a 2");

#if !__STDC_IEC_559__ || !__STDC_IEC_60559_BFP__
#error tu compilador debe implementar float/double/long double como números IEEE-754 / IEC 60559
#endif

_Static_assert(FLT_RADIX == 2, "float/double/long double deben ser binarios");
_Static_assert(sizeof(float) == 4, "float debe ser de 32 bits");
_Static_assert(sizeof(double) == 8, "double debe ser de 64 bits");

// La cantidad de bits en un entero.
#define PDCRT_ENTERO_BITS (sizeof(pdcrt_entero) * 8)

enum pdcrt_comparacion
{
    //                              ABC
    PDCRT_MENOR_QUE       = 1,  // 0001
    PDCRT_MENOR_O_IGUAL_A = 3,  // 0011
    PDCRT_MAYOR_QUE       = 4,  // 0100
    PDCRT_MAYOR_O_IGUAL_A = 6,  // 0110
    PDCRT_IGUAL_A         = 10  // 1010
    // El bit A es si es "mayor que", el bit B es el bit "igual a", el bit C es
    // el bit "menor que".
};

static inline bool pdcrt_es_menor_que(enum pdcrt_comparacion op)
{
    return op & 1;
}

static inline bool pdcrt_es_igual_a(enum pdcrt_comparacion op)
{
    return op & 2;
}

static inline bool pdcrt_es_mayor_que(enum pdcrt_comparacion op)
{
    return op & 4;
}

static bool pdcrt_comparar_floats(pdcrt_float a, pdcrt_float b, enum pdcrt_comparacion op)
{
    switch(op)
    {
    case PDCRT_MENOR_QUE:
        return a < b;
    case PDCRT_MENOR_O_IGUAL_A:
        return a <= b;
    case PDCRT_MAYOR_QUE:
        return a > b;
    case PDCRT_MAYOR_O_IGUAL_A:
        return a >= b;
    case PDCRT_IGUAL_A:
        return a == b;
    }
    assert(0 && "inalcanzable");
}

static bool pdcrt_comparar_enteros(pdcrt_entero a, pdcrt_entero b, enum pdcrt_comparacion op)
{
    switch(op)
    {
    case PDCRT_MENOR_QUE:
        return a < b;
    case PDCRT_MENOR_O_IGUAL_A:
        return a <= b;
    case PDCRT_MAYOR_QUE:
        return a > b;
    case PDCRT_MAYOR_O_IGUAL_A:
        return a >= b;
    case PDCRT_IGUAL_A:
        return a == b;
    }
    assert(0 && "inalcanzable");
}

static bool pdcrt_comparar_entero_y_float(pdcrt_entero e, pdcrt_float f, enum pdcrt_comparacion op)
{
    if(PDCRT_FLOAT_MANT_DIG >= PDCRT_ENTERO_BITS) // (1)
    {
        return pdcrt_comparar_floats((pdcrt_float) e, f, op);
    }

    if(isnan(f))
    {
        return false; // No son comparables.
    }
    else if(isinf(f))
    {
        return f > 0 ? pdcrt_es_menor_que(op) : pdcrt_es_mayor_que(op);
    }

    // Debido a (1), sabemos que PDCRT_FLOAT_DIG_SIG < PDCRT_ENTERO_BITS
    static const pdcrt_entero max_entero_repr_float = (1ULL << PDCRT_FLOAT_MANT_DIG) - 1U;
    static const pdcrt_entero min_entero_repr_float = -(1ULL << PDCRT_FLOAT_MANT_DIG);

    if((e >= min_entero_repr_float) && (e <= max_entero_repr_float))
    {
        return pdcrt_comparar_floats((pdcrt_float) e, f, op);
    }

    if((e < 0 && f >= 0.0) || (e <= 0 && f > 0.0))
    {
        return pdcrt_es_menor_que(op);
    }

    if((e > 0 && f <= 0.0) || (e >= 0 && f < 0.0))
    {
        return pdcrt_es_mayor_que(op);
    }

    if(e == 0 && f == 0.0)
    {
        return pdcrt_es_igual_a(op);
    }

    // Ahora sabemos que `e` y `f` tienen el mismo signo (ambos positivos o
    // ambos negativos).

    pdcrt_float f_ent, f_floor;
    f_floor = PDCRT_FLOAT_FLOOR(f);
    if(f_floor == f)
    {
        // `f` es un "float entero" (por ejemplo: 3.0)
        f_ent = f;
    }
    else
    {
        // `f` tiene una parte fraccional.
        switch(op)
        {
        case PDCRT_IGUAL_A:
            // Un float "normal" como 2.4 jamás será igual a un entero.
            return false;
        case PDCRT_MAYOR_O_IGUAL_A:
            // `e >= f` => `e >= floor(f)`
        case PDCRT_MAYOR_QUE:
            // `e > f` => `e > floor(f)`
            f_ent = f_floor;
            break;
        case PDCRT_MENOR_O_IGUAL_A:
            // `e <= f` => `e <= ceil(f)`
        case PDCRT_MENOR_QUE:
            // `e < f` => `e < ceil(f)`
            f_ent = PDCRT_FLOAT_CEIL(f);
            break;
        }
    }

    int exp = 0;
    PDCRT_FLOAT_FREXP(f_ent, &exp);
    assert(exp > 0); // `exp > 0` significa que `f_ent` contiene un
                     // entero.

    size_t f_bits = exp;

    // Además, como es positivo, el exponente es la cantidad de bits antes
    // del punto decimal (¿punto decimal?, ¿o punto binario?).
    if(f_bits > PDCRT_ENTERO_BITS)
    {
        // `f` es más grande o más pequeño que cualquier entero.
        return (f_ent > 0) ? op == PDCRT_MENOR_QUE : op == PDCRT_MAYOR_QUE;
    }
    else
    {
        assert(f_bits <= PDCRT_ENTERO_BITS);
        // Ahora sabemos que tienen cantidades comparables de bits, hay que
        // comparar sus magnitudes.

        // Este cast es seguro (no hará overflow), ya que sabemos que f tiene la
        // misma cantidad de bits *en su parte entera*.
        return pdcrt_comparar_enteros(e, (pdcrt_entero) f_ent, op);
    }
}


#define PDCRT_K(func)                                   \
    if(pdcrt_stack_lleno(ctx))                          \
    {                                                   \
        pdcrt_recolectar_basura_por_pila(ctx, &m);      \
        return (pdcrt_k) { .kf = &func, .marco = m };   \
    }

#define PDCRT_CALC_INICIO() (ctx->tam_pila - args) - 2;
// 2 elementos: yo + el mensaje.
#define PDCRT_SACAR_PRELUDIO() pdcrt_eliminar_elementos(ctx, inic, 2 + args);

typedef enum pdcrt_clase
{
    PDCRT_CLASE_NUMERO,
    PDCRT_CLASE_ARREGLO,
    PDCRT_CLASE_BOOLE,
    PDCRT_CLASE_PROCEDIMIENTO,
    PDCRT_CLASE_TIPO_NULO,
    PDCRT_CLASE_TEXTO,
} pdcrt_clase;

static pdcrt_k pdcrt_recv_fallback_a_clase_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_recv_fallback_a_clase(pdcrt_ctx *ctx, int args, pdcrt_k k, pdcrt_clase clase)
{
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];

    pdcrt_marco *m = pdcrt_crear_marco(ctx, 5, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, pdcrt_objeto_entero(clase));
    pdcrt_fijar_local(ctx, m, 1, yo);
    pdcrt_fijar_local(ctx, m, 2, msj);
    pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(inic));
    pdcrt_fijar_local(ctx, m, 4, pdcrt_objeto_entero(argp));

    pdcrt_extender_pila(ctx, m, 2);

    switch(clase)
    {
    case PDCRT_CLASE_NUMERO:
        pdcrt_empujar(ctx, ctx->clase_numero);
        break;
    case PDCRT_CLASE_ARREGLO:
        pdcrt_empujar(ctx, ctx->clase_arreglo);
        break;
    case PDCRT_CLASE_BOOLE:
        pdcrt_empujar(ctx, ctx->clase_boole);
        break;
    case PDCRT_CLASE_PROCEDIMIENTO:
        pdcrt_empujar(ctx, ctx->clase_procedimiento);
        break;
    case PDCRT_CLASE_TIPO_NULO:
        pdcrt_empujar(ctx, ctx->clase_tipo_nulo);
        break;
    case PDCRT_CLASE_TEXTO:
        pdcrt_empujar(ctx, ctx->clase_texto);
        break;
    }
    pdcrt_obj obj_clase = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(obj_clase) == PDCRT_TOBJ_NULO)
    {
        pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
        pdcrt_inspeccionar_texto(msj.texto);
        pdcrt_error(ctx, "Método no encontrado");
    }

    pdcrt_empujar(ctx, msj);

    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, u8"_obtenerMétodoDeInstancia", 26, proto, 1,
        &pdcrt_recv_fallback_a_clase_k1);
}

static pdcrt_k pdcrt_recv_fallback_a_clase_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    pdcrt_entero clase = pdcrt_obtener_local(ctx, m, 0).ival;
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_entero inic = pdcrt_obtener_local(ctx, m, 3).ival;
    pdcrt_entero argp = pdcrt_obtener_local(ctx, m, 4).ival;

    (void) clase;
    (void) yo;
    (void) argp;

    // [yo, msj, args..., metodo o nulo]
    pdcrt_obj metodo = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(metodo) == PDCRT_TOBJ_NULO)
    {
        pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
        pdcrt_inspeccionar_texto(msj.texto);
        pdcrt_error(ctx, "Método no encontrado");
    }
    else
    {
        pdcrt_eliminar_elemento(ctx, inic + 1);
        // [yo, args..., metodo o nulo]
        pdcrt_insertar(ctx, inic);
        // [metodo o nulo, yo, args...]
        return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 1, m->k.kf);
    }
}

pdcrt_k pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): comoTexto no acepta argumentos");
        static_assert(sizeof(pdcrt_entero) <= 64, "pdcrt_entero no debe tener más de 64 bits");
        char texto[21]; // 64 bits => máx. 20 caracteres + '\0'
        int len = snprintf(texto, sizeof(texto), "%" PDCRT_ENTERO_PRId, yo.ival);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &k.marco, texto, len));
        pdcrt_empujar(ctx, txt);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.sumar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_mas))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al sumar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_entero(ctx, k.marco, yo.ival + otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, ((pdcrt_float) yo.ival) + otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden sumar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.restar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_menos))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al restar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_entero(ctx, k.marco, yo.ival - otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, ((pdcrt_float) yo.ival) - otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden restar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_entero(ctx, k.marco, yo.ival * otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, ((pdcrt_float) yo.ival) * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al dividir se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, ((pdcrt_float) yo.ival) / ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, ((pdcrt_float) yo.ival) / otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden dividir dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.ival == arg.ival);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_entero_y_float(yo.ival, arg.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.ival != arg.ival);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, k.marco, !pdcrt_comparar_entero_y_float(yo.ival, arg.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
#define PDCRT_COMPARAR_ENTERO(m, opm, ms, opms, cmp, op)                \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (entero): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_extender_pila(ctx, k.marco, 1);                           \
        pdcrt_obj arg = ctx->pila[argp];                                \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                 \
            pdcrt_empujar_booleano(ctx, k.marco, yo.ival op arg.ival);            \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)             \
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_entero_y_float(yo.ival, arg.fval, cmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (entero): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return pdcrt_continuar(ctx, k);                                 \
    }
    PDCRT_COMPARAR_ENTERO(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MENOR_QUE, <)
    PDCRT_COMPARAR_ENTERO(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MAYOR_QUE, >)
    PDCRT_COMPARAR_ENTERO(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MENOR_O_IGUAL_A, <=)
    PDCRT_COMPARAR_ENTERO(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MAYOR_O_IGUAL_A, >=)
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): negar no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, -yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): piso no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): techo no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): truncar no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.byte_como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): byteComoTexto no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        char c = yo.ival;
        pdcrt_empujar_texto(ctx, &k.marco, &c, 1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.invertir))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): invertir no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, ~yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
#define PDCRT_OPERADOR_BIT(txt, nm, op)                                              \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.txt))              \
    {                                                                                \
        if(args != 1)                                                                \
            pdcrt_error(ctx, "Numero (entero): "nm" acepta solo un argumento");      \
        pdcrt_extender_pila(ctx, k.marco, 1);                                        \
        pdcrt_obj arg = ctx->pila[argp];                                             \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                              \
            pdcrt_empujar_entero(ctx, k.marco, yo.ival op arg.ival);                 \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                          \
            pdcrt_empujar_entero(ctx, k.marco, yo.ival op (pdcrt_entero) arg.fval);  \
        else                                                                         \
            pdcrt_error(ctx, "Argumento de tipo inesperado");                        \
        PDCRT_SACAR_PRELUDIO();                                                      \
        return pdcrt_continuar(ctx, k);                                              \
    }
    PDCRT_OPERADOR_BIT(operador_bitand, "operador_<*>", &)
    PDCRT_OPERADOR_BIT(operador_bitor, "operador_<+>", |)
    PDCRT_OPERADOR_BIT(operador_bitxor, "operador_<^>", ^)
    PDCRT_OPERADOR_BIT(operador_bitlshift, "operador_<<", <<)
    PDCRT_OPERADOR_BIT(operador_bitrshift, "operador_>>", >>)
#undef PDCRT_OPERADOR_BIT

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_NUMERO);
}

pdcrt_k pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): comoTexto no acepta argumentos");
        char texto[30];
        int len = snprintf(texto, sizeof(texto), "%g", (double) yo.fval);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &k.marco, texto, len));
        pdcrt_empujar(ctx, txt);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.sumar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_mas))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al sumar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, yo.fval + ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, yo.fval + otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden sumar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.restar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_menos))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al restar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, yo.fval - ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, yo.fval - otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden restar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, yo.fval * ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, yo.fval * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al dividir se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, yo.fval / ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, yo.fval / otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden dividir dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.fval == arg.fval);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_entero_y_float(arg.ival, yo.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.fval != arg.fval);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, !pdcrt_comparar_entero_y_float(arg.ival, yo.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
#define PDCRT_COMPARAR_FLOAT(m, opm, ms, opms, rcmp, op)                \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (float): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_extender_pila(ctx, k.marco, 1);                           \
        pdcrt_obj arg = ctx->pila[argp];                                \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                  \
            pdcrt_empujar_booleano(ctx, k.marco, yo.fval op arg.fval);           \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)            \
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_entero_y_float(arg.ival, yo.fval, rcmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (float): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return pdcrt_continuar(ctx, k);                                 \
    }
    PDCRT_COMPARAR_FLOAT(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MAYOR_O_IGUAL_A, <)
    PDCRT_COMPARAR_FLOAT(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MENOR_O_IGUAL_A, >)
    PDCRT_COMPARAR_FLOAT(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MAYOR_QUE, <=)
    PDCRT_COMPARAR_FLOAT(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MENOR_QUE, >=)
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): negar no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, -yo.fval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): piso no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, PDCRT_FLOAT_FLOOR(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): techo no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, PDCRT_FLOAT_CEIL(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): truncar no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, PDCRT_FLOAT_TRUNC(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.byte_como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): byteComoTexto no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        char c = (char) yo.fval;
        pdcrt_empujar_texto(ctx, &k.marco, &c, 1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.invertir))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): invertir no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, ~(pdcrt_entero) yo.fval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
#define PDCRT_OPERADOR_BIT(txt, nm, op)                                              \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.txt))              \
    {                                                                                \
        if(args != 1)                                                                \
            pdcrt_error(ctx, "Numero (float): "nm" acepta solo un argumento");       \
        pdcrt_extender_pila(ctx, k.marco, 1);                                        \
        pdcrt_obj arg = ctx->pila[argp];                                             \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                              \
            pdcrt_empujar_entero(ctx, k.marco, ((pdcrt_entero) yo.fval) op arg.ival); \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                          \
            pdcrt_empujar_entero(ctx, k.marco, ((pdcrt_entero) yo.fval) op (pdcrt_entero) arg.fval); \
        else                                                                         \
            pdcrt_error(ctx, "Argumento de tipo inesperado");                        \
        PDCRT_SACAR_PRELUDIO();                                                      \
        return pdcrt_continuar(ctx, k);                                              \
    }
    PDCRT_OPERADOR_BIT(operador_bitand, "operador_<*>", &)
    PDCRT_OPERADOR_BIT(operador_bitor, "operador_<+>", |)
    PDCRT_OPERADOR_BIT(operador_bitxor, "operador_<^>", ^)
    PDCRT_OPERADOR_BIT(operador_bitlshift, "operador_<<", <<)
    PDCRT_OPERADOR_BIT(operador_bitrshift, "operador_>>", >>)
#undef PDCRT_OPERADOR_BIT

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_NUMERO);
}

pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Booleano: comoTexto no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        if(yo.bval)
            pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.verdadero));
        else
            pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.falso));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.bval == arg.bval);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.bval != arg.bval);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.escojer))
    {
        if(args != 2)
            pdcrt_error(ctx, "Booleano: escojer necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj siVerdadero = ctx->pila[argp];
        pdcrt_obj siFalso = ctx->pila[argp + 1];
        pdcrt_empujar(ctx, yo.bval ? siVerdadero : siFalso);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.llamarSegun)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.llamarSegun2))
    {
        if(args != 2)
            pdcrt_error(ctx, u8"Booleano: llamarSegún necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj siVerdadero = ctx->pila[argp];
        pdcrt_obj siFalso = ctx->pila[argp + 1];
        pdcrt_empujar(ctx, yo.bval ? siVerdadero : siFalso);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_enviar_mensaje(ctx, k.marco, "llamar", 6, NULL, 0, k.kf);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.o)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_o))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: \"||\" necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj v = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
        pdcrt_empujar_booleano(ctx, k.marco, yo.bval || v.bval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.y)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_y))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: \"&&\" necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj v = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
        pdcrt_empujar_booleano(ctx, k.marco, yo.bval && v.bval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_BOOLE);
}

pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) yo;
    (void) argp;
    (void) k;
    assert(0 && "sin implementar");
}

static bool pdcrt_es_digito(char c)
{
    return c >= '0' && c <= '9';
}

static bool pdcrt_prefijo_de_texto(pdcrt_texto *txt, size_t pos, const char *prefix)
{
    for(size_t i = pos; i < txt->longitud; i++)
    {
        char c = *prefix++;
        if(!c)
            break;
        else if(c != txt->contenido[i])
            return false;
    }
    return true;
}

static pdcrt_k pdcrt_texto_formatear_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_texto_formatear_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_texto_formatear_k3(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_texto_formatear_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_texto_formatear_k1);

    pdcrt_obj arr = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj oargs_consumidos = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj oarg_ptr = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 4);
#define PDCRT_RECARGAR_LOCALES()                                    \
    do                                                              \
    {                                                               \
        arr = pdcrt_obtener_local(ctx, m, 0);                       \
        oi = pdcrt_obtener_local(ctx, m, 1);                        \
        oargs_consumidos = pdcrt_obtener_local(ctx, m, 2);          \
        oarg_ptr = pdcrt_obtener_local(ctx, m, 3);                  \
        yo = pdcrt_obtener_local(ctx, m, 4);                        \
        i = oi.ival;                                                \
        args_consumidos = oargs_consumidos.ival;                    \
        arg_ptr = oarg_ptr.ival;                                    \
        args = m->args;                                             \
    }                                                               \
    while(0)

    pdcrt_entero i = oi.ival;
    pdcrt_entero args_consumidos = oargs_consumidos.ival;
    pdcrt_entero arg_ptr = oarg_ptr.ival;
    pdcrt_entero args = m->args;
    pdcrt_extender_pila(ctx, m, 2);

    if((size_t) i >= yo.texto->longitud)
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "");
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "unir", 4, proto, 1, &pdcrt_texto_formatear_k3);
    }

    if(pdcrt_prefijo_de_texto(yo.texto, i, "~T"))
    {
        if(args_consumidos >= args)
            pdcrt_error(ctx, u8"Texto#formatear: más formatos que argumentos");
        args_consumidos += 1;
        i += 2;
        pdcrt_empujar(ctx, arr);
        pdcrt_duplicar(ctx, m,arg_ptr++);
        pdcrt_obj arg = pdcrt_cima(ctx);
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
            pdcrt_error(ctx, "");
        pdcrt_arreglo_empujar_al_final(ctx, m, -2);
        (void) pdcrt_sacar(ctx);
        goto final;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~t"))
    {
        if(args_consumidos >= args)
            pdcrt_error(ctx, u8"Texto#formatear: más formatos que argumentos");
        args_consumidos += 1;
        i += 2;
        pdcrt_empujar(ctx, arr);
        pdcrt_duplicar(ctx, m, arg_ptr++);
        // [arr, arg]
        pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_entero(i));
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(args_consumidos));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(arg_ptr));
        return pdcrt_enviar_mensaje(ctx, m, "comoTexto", 9, NULL, 0, &pdcrt_texto_formatear_k2);
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~~"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "~");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~%"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "\n");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~e"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "}");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~E"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "»");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~q"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "\"");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~|%\n"))
    {
        i += 4;
        goto final;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~|%\r\n"))
    {
        i += 5;
        goto final;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~"))
    {
        pdcrt_error(ctx, u8"Formato inválido para Texto#formatear");
    }
    else
    {
        size_t len = 0;
        for(size_t j = i; j < yo.texto->longitud; j++)
        {
            if(yo.texto->contenido[j] == '~')
                break;
            len += 1;
        }

        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto(ctx, &m, yo.texto->contenido + i, len);
        PDCRT_RECARGAR_LOCALES();
        i += len;
    }

    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    (void) pdcrt_sacar(ctx);
    final:
    pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_entero(i));
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(args_consumidos));
    pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(arg_ptr));
    return pdcrt_texto_formatear_k1(ctx, m);

#undef PDCRT_RECARGAR_LOCALES
}

static pdcrt_k pdcrt_texto_formatear_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_texto_formatear_k2);

    pdcrt_obj arg = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        pdcrt_error(ctx, "");
    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    (void) pdcrt_sacar(ctx);
    return pdcrt_texto_formatear_k1(ctx, m);
}

static pdcrt_k pdcrt_texto_formatear_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_texto_formatear_k3);
    pdcrt_obj oarg_inic = pdcrt_obtener_local(ctx, m, 5);
    pdcrt_entero arg_inic = oarg_inic.ival;
    pdcrt_eliminar_elementos(ctx, arg_inic, m->args);
    return pdcrt_devolver(ctx, m, 1);
}

pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.concatenar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: concatenar necesita 1 argumento");
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_TEXTO);
        size_t bufflen = yo.texto->longitud + arg.texto->longitud;
        char *buff = pdcrt_alojar_ctx(ctx, bufflen);
        if(!buff)
            pdcrt_enomem(ctx);
        memcpy(buff, yo.texto->contenido, yo.texto->longitud);
        memcpy(buff + yo.texto->longitud, arg.texto->contenido, arg.texto->longitud);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_texto *res = pdcrt_crear_texto(ctx, &k.marco, buff, bufflen);
        pdcrt_desalojar_ctx(ctx, buff, bufflen);
        pdcrt_empujar(ctx, pdcrt_objeto_texto(res));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_textos(yo.texto, arg.texto));
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, !pdcrt_comparar_textos(yo.texto, arg.texto));
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_numero_entero))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroEntero no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        const char* s = yo.texto->contenido;
        if(*s == '-')
            s += 1;
        for(; *s; s++)
            if(!pdcrt_es_digito(*s))
                goto error_como_entero;

        pdcrt_entero i;
        i = strtoll(yo.texto->contenido, NULL, 10);
        pdcrt_empujar_entero(ctx, k.marco, i);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    error_como_entero:
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_numero_real))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroReal no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        const char* s = yo.texto->contenido;
        if(*s == '-')
            s += 1;
        bool dot = false;
        for(; *s; s++)
            if(*s == '.' && !dot)
                dot = true;
            else if(*s == '.' && dot)
                goto error_como_real;
            else if(!pdcrt_es_digito(*s))
                goto error_como_real;

        pdcrt_float f;
        f = strtold(yo.texto->contenido, NULL);
        pdcrt_empujar_float(ctx, k.marco, f);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    error_como_real:
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, yo);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: longitud no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.texto->longitud);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: en necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: en necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= yo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #en");
        pdcrt_empujar_texto(ctx, &k.marco, yo.texto->contenido + i, 1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.byte_en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: byteEn necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: byteEn necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= yo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #byteEn");
        pdcrt_empujar_entero(ctx, k.marco, yo.texto->contenido[i]);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.subtexto))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);

        bool ok = false;
        pdcrt_entero inicio, longitud;
        inicio = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 enteros como argumentos");
        longitud = pdcrt_obtener_entero(ctx, argp + 1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 enteros como argumentos");

        if(inicio < 0 || (size_t) inicio > yo.texto->longitud)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el primer argumento de #subTexto");
        if(longitud < 0)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el segundo argumento de #subTexto");

        if((size_t) (inicio + longitud) > yo.texto->longitud)
            longitud = yo.texto->longitud - inicio;

        if(longitud == 0)
        {
            pdcrt_empujar_texto(ctx, &k.marco, "", 0);
        }
        else
        {
            char *buffer = pdcrt_alojar_ctx(ctx, longitud);
            assert(buffer);
            memcpy(buffer, yo.texto->contenido + inicio, longitud);
            pdcrt_empujar_texto(ctx, &k.marco, buffer, longitud);
            pdcrt_desalojar_ctx(ctx, buffer, longitud);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.parte_del_texto))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);

        bool ok = false;
        pdcrt_entero inicio, final;
        inicio = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 enteros como argumentos");
        final = pdcrt_obtener_entero(ctx, argp + 1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 enteros como argumentos");

        if(inicio < 0 || (size_t) inicio > yo.texto->longitud)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el primer argumento de #parteDelTexto");
        if(final < 0)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el segundo argumento de #parteDelTexto");
        if((size_t) final > yo.texto->longitud)
            final = yo.texto->longitud;

        if(final <= inicio)
        {
            pdcrt_empujar_texto(ctx, &k.marco, "", 0);
        }
        else
        {
            char *buffer = pdcrt_alojar_ctx(ctx, final - inicio);
            assert(buffer);
            memcpy(buffer, yo.texto->contenido + inicio, final - inicio);
            pdcrt_empujar_texto(ctx, &k.marco, buffer, final - inicio);
            pdcrt_desalojar_ctx(ctx, buffer, final - inicio);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.buscar))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: buscar necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);

        /* Algoritmo Knuth-Morris-Pratt, sacado de wikipedia:
         * <https://en.wikipedia.org/wiki/Knuth%E2%80%93Morris%E2%80%93Pratt_algorithm>
         * el 2024-05-05.
         */

        bool ok = false;
        pdcrt_entero desde = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: buscar necesita un entero como primer argumento");
        if(desde < 0 || (size_t) desde > yo.texto->longitud)
            pdcrt_error(ctx, "Texto: buscar primer argumento fuera de rango");
        size_t buffer_len = pdcrt_obtener_tam_texto(ctx, argp + 1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: buscar necesita un texto como segundo argumento");

        if(buffer_len == 0)
        {
            pdcrt_empujar_entero(ctx, k.marco, desde);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }

        char *buffer = pdcrt_alojar_ctx(ctx, buffer_len + 1);
        assert(buffer);
        ok = pdcrt_obtener_texto(ctx, argp + 1, buffer, buffer_len + 1);
        assert(ok);
        ssize_t *skip_table = pdcrt_alojar_ctx(ctx, buffer_len * sizeof(ssize_t));
        assert(skip_table);

        // Llena la tabla.
        skip_table[0] = -1;
        ssize_t buffer_candidato = 0;
        for(size_t i = 1; i < buffer_len; i++, buffer_candidato++)
        {
            if(buffer[i] == buffer[buffer_candidato])
            {
                skip_table[i] = skip_table[buffer_candidato];
            }
            else
            {
                skip_table[i] = buffer_candidato;
                while(buffer_candidato >= 0 && buffer[buffer_candidato] != buffer[i])
                {
                    buffer_candidato = skip_table[buffer_candidato];
                }
            }
        }

        // Busca el texto.
        size_t yo_pos = desde, buffer_pos = 0;

        while(yo_pos < yo.texto->longitud)
        {
            if(yo.texto->contenido[yo_pos] == buffer[buffer_pos])
            {
                buffer_pos += 1;
                yo_pos += 1;
                if(buffer_pos >= buffer_len)
                {
                    pdcrt_empujar_entero(ctx, k.marco, yo_pos - buffer_pos);
                    goto buscar_encontrado;
                }
            }
            else
            {
                ssize_t el = skip_table[buffer_pos];
                if(el < 0)
                {
                    yo_pos += 1;
                    buffer_pos = 0;
                }
                else
                {
                    buffer_pos = el;
                }
            }
        }

        pdcrt_empujar_nulo(ctx, k.marco);
    buscar_encontrado:

        pdcrt_desalojar_ctx(ctx, buffer, buffer_len + 1);
        pdcrt_desalojar_ctx(ctx, skip_table, buffer_len * sizeof(ssize_t));

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.formatear))
    {
        const char *str = yo.texto->contenido;
        size_t len = yo.texto->longitud;
        pdcrt_extender_pila(ctx, k.marco, 2);

        // Vamos a calcular la cantidad de formatos en el texto
        // Al hacer esto podemos computar la capacidad del arreglo
        // intermediario y así evitar consumir más memoria de la
        // necesaria.
        bool enFormato = false; // Si el carácter anterior fue '~'
        size_t formatos = 0;
        for(size_t i = 0; i < len; i++)
        {
            if(str[i] == '~')
            {
                if(enFormato)
                    formatos += 1;
                enFormato = !enFormato;
            }
            else if(enFormato)
            {
                formatos += 1;
                enFormato = false;
            }
        }

        pdcrt_empujar_arreglo_vacio(ctx, &k.marco, 2 * formatos + 1);

        pdcrt_marco *m = pdcrt_crear_marco(ctx, 6, 0, args, k);

        // Elimina `yo` y el mensaje
        pdcrt_eliminar_elementos(ctx, inic, 2);

        // arr
        pdcrt_fijar_local(ctx, m, 0, pdcrt_sacar(ctx));
        // oi
        pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_entero(0));
        // oargs_consumidos
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0));
        // oarg_ptr
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(argp - 2));
        // yo
        pdcrt_fijar_local(ctx, m, 4, yo);
        // oarg_inic
        pdcrt_fijar_local(ctx, m, 5, pdcrt_objeto_entero(argp - 2));

        return pdcrt_texto_formatear_k1(ctx, m);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TEXTO);
}

pdcrt_k pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    (void) yo;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, k.marco, pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_NULO);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, k.marco, pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_NULO);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Nulo: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, "NULO");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TIPO_NULO);
}

static pdcrt_k pdcrt_arreglo_igual_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_igual_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_arreglo_igual_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_igual_k1);
    // []
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj otro = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    if((size_t) i.ival < yo.arreglo->longitud && (size_t) i.ival < otro.arreglo->longitud)
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.arreglo->valores[i.ival]);
        pdcrt_empujar(ctx, otro.arreglo->valores[i.ival]);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "igualA", 6, proto, 1, &pdcrt_arreglo_igual_k2);
    }
    else if(yo.arreglo->longitud == otro.arreglo->longitud)
    {
        // Fin del arreglo, todos los elementos fueron iguales
        pdcrt_empujar_booleano(ctx, m, true);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        // Fin del arreglo, tenían tamaños distintos
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar_booleano(ctx, m, false);
        return pdcrt_devolver(ctx, m, 1);
    }
}

static pdcrt_k pdcrt_arreglo_igual_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_igual_k2);
    // [eq]
    bool ok = false;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Arreglo: igualA / operador_=: el metodo igualA de un elemento no devolvio un booleano");
    if(!eq)
    {
        (void) pdcrt_sacar(ctx);
        pdcrt_empujar_booleano(ctx, m, false);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        pdcrt_entero i = pdcrt_obtener_local(ctx, m, 2).ival;
        (void) pdcrt_sacar(ctx); // eq
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));
        return pdcrt_arreglo_igual_k1(ctx, m);
    }
}

static pdcrt_k pdcrt_arreglo_distinto_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_distinto_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_arreglo_distinto_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_distinto_k1);
    // []
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj otro = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    if((size_t) i.ival < yo.arreglo->longitud && (size_t) i.ival < otro.arreglo->longitud)
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.arreglo->valores[i.ival]);
        pdcrt_empujar(ctx, otro.arreglo->valores[i.ival]);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, u8"distintoDe", 11, proto, 1, &pdcrt_arreglo_distinto_k2);
    }
    else if(yo.arreglo->longitud == otro.arreglo->longitud)
    {
        // Fin del arreglo, todos los elementos fueron iguales
        pdcrt_eliminar_elementos(ctx, -3, 3);
        pdcrt_empujar_booleano(ctx, m, false);
        return pdcrt_continuar(ctx, m->k);
    }
    else
    {
        // Fin del arreglo, tenían tamaños distintos
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar_booleano(ctx, m, true);
        return pdcrt_continuar(ctx, m->k);
    }
}

static pdcrt_k pdcrt_arreglo_distinto_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_distinto_k2);
    // [eq]
    bool ok = false;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Arreglo: distintoDe / operador_no=: el metodo distintoDe de un elemento no devolvio un booleano");
    if(!eq)
    {
        (void) pdcrt_sacar(ctx);
        pdcrt_empujar_booleano(ctx, m, true);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        pdcrt_entero i = pdcrt_obtener_local(ctx, m, 2).ival;
        (void) pdcrt_sacar(ctx); // eq
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));
        return pdcrt_arreglo_distinto_k1(ctx, m);
    }
}

static pdcrt_k pdcrt_arreglo_como_texto_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k4(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k5(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_arreglo_como_texto_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj buffer = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    if((size_t) i.ival < yo.arreglo->longitud)
    {
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar(ctx, yo.arreglo->valores[i.ival]);
        return pdcrt_enviar_mensaje(ctx, m, u8"comoTexto", 9, NULL, 0, &pdcrt_arreglo_como_texto_k2);
    }
    else
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_obj sep = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, ", ", 2));
        buffer = pdcrt_obtener_local(ctx, m, 1);
        pdcrt_empujar(ctx, buffer);
        pdcrt_empujar(ctx, sep);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, u8"unir", 4, proto, 1, &pdcrt_arreglo_como_texto_k3);
    }
}

static pdcrt_k pdcrt_arreglo_como_texto_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k2);
    // [eltxt]
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj buffer = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    (void) yo;
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, buffer);
    // [eltxt, buffer]
    pdcrt_obj cima = pdcrt_cima(ctx);
    pdcrt_fijar_pila(ctx, ctx->tam_pila - 1, ctx->pila[ctx->tam_pila - 2]);
    pdcrt_fijar_pila(ctx, ctx->tam_pila - 2, cima);
    // [buffer, eltxt]
    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    // [buffer]
    (void) pdcrt_sacar(ctx);
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i.ival + 1));
    return pdcrt_arreglo_como_texto_k1(ctx, m);
}

static pdcrt_k pdcrt_arreglo_como_texto_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k3);
    // [res]
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar_texto_cstr(ctx, &m, "(Arreglo#crearCon: ");
    pdcrt_extraer(ctx, -2);
    // [pref, res]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "concatenar", 10, proto, 1, &pdcrt_arreglo_como_texto_k4);
}

static pdcrt_k pdcrt_arreglo_como_texto_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k4);
    // [res]
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar_texto_cstr(ctx, &m, ")");
    // [res, suf]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "concatenar", 10, proto, 1, &pdcrt_arreglo_como_texto_k5);
}
static pdcrt_k pdcrt_arreglo_como_texto_k5(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k5);
    // [res]
    return pdcrt_devolver(ctx, m, 1);
}

pdcrt_k pdcrt_recv_arreglo(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_ARREGLO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            pdcrt_fijar_local(ctx, m, 1, arg); // otro
            pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0)); // i
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_arreglo_igual_k1(ctx, m);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_ARREGLO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            pdcrt_fijar_local(ctx, m, 1, arg); // otro
            pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0)); // i
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_arreglo_distinto_k1(ctx, m);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Arreglo: comoTexto no necesita argumentos");
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_obj buffer = pdcrt_objeto_arreglo(
            pdcrt_crear_arreglo_vacio(ctx, &m, yo.arreglo->longitud + 2));
        pdcrt_fijar_local(ctx, m, 1, buffer); // buffer
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0)); // i
        PDCRT_SACAR_PRELUDIO();
        // []
        return pdcrt_arreglo_como_texto_k1(ctx, m);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: en necesita un argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Arreglo: en necesita un entero como argumento");
        if(i < 0 || (size_t) i >= yo.arreglo->longitud)
            pdcrt_error(ctx, "Arreglo: en: indice fuera de rango");
        pdcrt_empujar(ctx, yo.arreglo->valores[i]);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijarEn))
    {
        if(args != 2)
            pdcrt_error(ctx, "Arreglo: fijarEn necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -2, &ok);
        if(!ok)
            pdcrt_error(ctx, "Arreglo: fijarEn necesita un entero como argumento");
        if(i < 0 || (size_t) i >= yo.arreglo->longitud)
            pdcrt_error(ctx, "Arreglo: fijarEn: indice fuera de rango");
        pdcrt_obj val = pdcrt_cima(ctx);
        pdcrt_barrera_de_escritura(ctx, yo, val);
        yo.arreglo->valores[i] = val;
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Arreglo: longitud no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.arreglo->longitud);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.unir))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: unir necesita un argumento");
        pdcrt_obj separador = pdcrt_cima(ctx);
        if(pdcrt_tipo_de_obj(separador) != PDCRT_TOBJ_TEXTO)
            pdcrt_error(ctx, "Arreglo: el argumento de unir debe ser un texto");
        size_t tam_final = 0;
        for(size_t i = 0; i < yo.arreglo->longitud; i++)
        {
            pdcrt_obj el = yo.arreglo->valores[i];
            if(pdcrt_tipo_de_obj(el) != PDCRT_TOBJ_TEXTO)
                pdcrt_error(ctx, "Arreglo: los elementos del arreglo deben ser textos");
            tam_final += el.texto->longitud;
            if(i > 0)
                tam_final += separador.texto->longitud;
        }

        char *buffer = pdcrt_alojar_ctx(ctx, tam_final);
        if(!buffer)
            pdcrt_enomem(ctx);
        size_t cur = 0;

        for(size_t i = 0; i < yo.arreglo->longitud; i++)
        {
            if(i > 0)
            {
                if(separador.texto->longitud > 0)
                    memcpy(buffer + cur, separador.texto->contenido,
                           separador.texto->longitud);
                cur += separador.texto->longitud;
            }
            pdcrt_obj el = yo.arreglo->valores[i];
            if(el.texto->longitud > 0)
                memcpy(buffer + cur, el.texto->contenido, el.texto->longitud);
            cur += el.texto->longitud;
        }

        pdcrt_empujar_texto(ctx, &k.marco, buffer, tam_final);
        pdcrt_desalojar_ctx(ctx, buffer, tam_final);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.agregar_al_final))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: agregarAlFinal necesita 1 argumento");
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, arg);
        pdcrt_arreglo_empujar_al_final(ctx, k.marco, -4);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_ARREGLO);
}

pdcrt_k pdcrt_recv_closure(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Procedimiento: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CLOSURE)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.closure == arg.closure);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Procedimiento: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CLOSURE)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.closure != arg.closure);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Procedimiento: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Procedimiento: %p", yo.closure);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.llamar))
    {
        pdcrt_extender_pila(ctx, k.marco, yo.closure->num_capturas);
        for(size_t i = 0; i < yo.closure->num_capturas; i++)
        {
            pdcrt_empujar(ctx, yo.closure->capturas[i]);
        }
        pdcrt_eliminar_elementos(ctx, inic, 2);
        return (*yo.closure->f)(ctx, args, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_PROCEDIMIENTO);
}

pdcrt_k pdcrt_recv_caja(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;
    (void) yo;
    (void) k;

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_tabla_fijar_en_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_tabla_fijar_en_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_tabla_fijar_en_k3(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_tabla_en_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_tabla_en_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_tabla_rehashear_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_tabla_rehashear_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_tabla_eliminar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_tabla_eliminar_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_tabla_para_cada_par_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_tabla_para_cada_par_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static void pdcrt_tabla_inicializar_buckets(pdcrt_ctx *ctx, pdcrt_bucket *buckets, size_t tam);
static void pdcrt_tabla_inicializar_con_capacidad(pdcrt_ctx *ctx, pdcrt_tabla *tbl, size_t capacidad);

pdcrt_k pdcrt_recv_tabla(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijarEn))
    {
        if(args != 2)
            pdcrt_error(ctx, "Tabla: fijarEn necesita 2 argumentos");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_obj valor = ctx->pila[argp + 1];
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 4, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_fijar_local(ctx, m, 1, llave);
        pdcrt_fijar_local(ctx, m, 2, valor);
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_voidptr(NULL));
        PDCRT_SACAR_PRELUDIO();
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.tabla->funcion_hash);
        pdcrt_empujar(ctx, llave);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 1, &pdcrt_tabla_fijar_en_k1);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: en necesita 1 argumento");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 4, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_fijar_local(ctx, m, 1, llave);
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(NULL));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_booleano(false));
        PDCRT_SACAR_PRELUDIO();
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.tabla->funcion_hash);
        pdcrt_empujar(ctx, llave);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 1, &pdcrt_tabla_en_k1);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: longitud no necesita argumentos");
        size_t l = 0;
        for(size_t i = 0; i < yo.tabla->num_buckets; i++)
        {
            pdcrt_bucket *b = &yo.tabla->buckets[i];
            if(b->activo)
            {
                l += 1;
                while(b->siguiente_colision)
                {
                    b = b->siguiente_colision;
                    l += 1;
                }
            }
        }
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, l);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.rehashear))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: rehashear necesita 1 argumento");
        bool ok;
        pdcrt_entero capacidad_adicional = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Tabla#rehashear: necesita un entero como argumento");

        if(yo.tabla->num_buckets == 0)
        {
            pdcrt_tabla_inicializar_con_capacidad(ctx, yo.tabla, 1);
            assert(yo.tabla->num_buckets > 0);
        }

        if(capacidad_adicional < 0 && (size_t) (-capacidad_adicional) >= yo.tabla->num_buckets)
        {
            pdcrt_error(ctx, u8"Tabla#rehashear: Valor inválido para la capacidad adicional");
        }

        size_t cap = yo.tabla->num_buckets + capacidad_adicional;
        pdcrt_bucket *nuevos_buckets = pdcrt_alojar_ctx(ctx, sizeof(pdcrt_bucket) * cap);
        if(!nuevos_buckets)
            pdcrt_enomem(ctx);
        pdcrt_tabla_inicializar_buckets(ctx, nuevos_buckets, cap);

        pdcrt_marco *m = pdcrt_crear_marco(ctx, 6, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        // bucket actual
        pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_voidptr(&yo.tabla->buckets[0]));
        // índice de bucket actual
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0));
        // Lista de buckets en proceso de creación
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_voidptr(nuevos_buckets));
        // Tamaño de la lista
        pdcrt_fijar_local(ctx, m, 4, pdcrt_objeto_entero(cap));
        // Buckets ocupados
        pdcrt_fijar_local(ctx, m, 5, pdcrt_objeto_entero(0));

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_tabla_rehashear_k1(ctx, m);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.capacidad))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: capacidad no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.tabla->num_buckets);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.contiene))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: contiene necesita 1 argumento");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 4, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_fijar_local(ctx, m, 1, llave);
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(NULL));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_booleano(true));
        PDCRT_SACAR_PRELUDIO();
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.tabla->funcion_hash);
        pdcrt_empujar(ctx, llave);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 1, &pdcrt_tabla_en_k1);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.eliminar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: eliminar necesita 1 argumento");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 4, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_fijar_local(ctx, m, 1, llave);
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(NULL));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_voidptr(NULL));
        PDCRT_SACAR_PRELUDIO();
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.tabla->funcion_hash);
        pdcrt_empujar(ctx, llave);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 1, &pdcrt_tabla_eliminar_k1);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.paraCadaPar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: paraCadaPar necesita 1 argumento");
        if(yo.tabla->num_buckets == 0)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_obj iterador = pdcrt_cima(ctx);
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 4, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            pdcrt_fijar_local(ctx, m, 1, iterador);
            pdcrt_fijar_local(ctx, m, 2,
                              pdcrt_objeto_voidptr(&yo.tabla->buckets[0]));
            pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(0));
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_tabla_para_cada_par_k1(ctx, m);
        }
    }

    assert(0 && "sin implementar");
}

// Las tablas se rehashean al llegar a dos tercios de llenas
#define PDCRT_TABLA_LIMITE(buckets) ((buckets) - ((buckets) / 3))

static void pdcrt_tabla_inicializar_buckets(pdcrt_ctx *ctx, pdcrt_bucket *buckets, size_t tam)
{
    (void) ctx;
    for(size_t i = 0; i < tam; i++)
    {
        pdcrt_bucket *b = &buckets[i];
        b->activo = false;
        b->siguiente_colision = NULL;
        b->llave = b->valor = pdcrt_objeto_nulo();
    }
}

static void pdcrt_tabla_inicializar(pdcrt_ctx *ctx, pdcrt_tabla *tbl)
{
    pdcrt_tabla_inicializar_buckets(ctx, tbl->buckets, tbl->num_buckets);
}

static void pdcrt_tabla_inicializar_con_capacidad(pdcrt_ctx *ctx, pdcrt_tabla *tbl, size_t capacidad)
{
    assert(tbl->num_buckets == 0);
    assert(tbl->buckets == NULL);
    tbl->buckets = pdcrt_alojar_ctx(ctx, sizeof(pdcrt_bucket) * capacidad);
    if(!tbl->buckets)
        pdcrt_enomem(ctx);
    tbl->num_buckets = capacidad;
    tbl->limite_de_ocupacion = PDCRT_TABLA_LIMITE(capacidad);
    pdcrt_tabla_inicializar(ctx, tbl);
}

static size_t pdcrt_liberar_tabla(pdcrt_ctx *ctx, pdcrt_bucket *lista, size_t tam_lista)
{
    size_t liberado = 0;
    for(size_t i = 0; i < tam_lista; i++)
    {
        pdcrt_bucket *b = &lista[i];
        if(b->activo)
        {
            b = b->siguiente_colision;
            while(b)
            {
                pdcrt_bucket *nb = b->siguiente_colision;
                pdcrt_desalojar_ctx(ctx, b, sizeof(pdcrt_bucket));
                liberado += sizeof(pdcrt_bucket);
                b = nb;
            }
        }
    }

    size_t tam = sizeof(pdcrt_bucket) * tam_lista;
    pdcrt_desalojar_ctx(ctx, lista, tam);
    liberado += tam;
    return liberado;
}

static pdcrt_k pdcrt_tabla_fijar_en_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_fijar_en_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj llave = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj valor = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj b = pdcrt_obtener_local(ctx, m, 3);
    (void) b;
    // [hash]
    bool ok;
    pdcrt_entero hash = pdcrt_obtener_entero(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Tabla#fijarEn: El hash debe ser un entero");
    (void) pdcrt_sacar(ctx);
    // []
    if(yo.tabla->num_buckets == 0)
    {
        pdcrt_tabla_inicializar_con_capacidad(ctx, yo.tabla, 1);
        assert(yo.tabla->num_buckets > 0);
    }

    pdcrt_bucket *bucket = &yo.tabla->buckets[hash % yo.tabla->num_buckets];
    if(!bucket->activo)
    {
        bucket->activo = true;
        pdcrt_barrera_de_escritura(ctx, yo, llave);
        bucket->llave = llave;
        pdcrt_barrera_de_escritura(ctx, yo, valor);
        bucket->valor = valor;
        bucket->siguiente_colision = NULL;
        yo.tabla->buckets_ocupados += 1;
        if(yo.tabla->buckets_ocupados >= yo.tabla->limite_de_ocupacion)
        {
            pdcrt_extender_pila(ctx, m, 2);
            pdcrt_empujar(ctx, yo);
            pdcrt_empujar_entero(ctx, m, 1);
            static const int proto[] = {0};
            return pdcrt_enviar_mensaje(ctx, m, "rehashear", 9, proto, 1, &pdcrt_tabla_fijar_en_k3);
        }
        else
        {
            pdcrt_extender_pila(ctx, m, 1);
            pdcrt_empujar_nulo(ctx, m);
            // [NULO]
            return pdcrt_devolver(ctx, m, 1);
        }
    }
    else
    {
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_voidptr(bucket));
        pdcrt_extender_pila(ctx, m, 3);
        pdcrt_empujar(ctx, yo.tabla->funcion_igualdad);
        pdcrt_empujar(ctx, llave);
        pdcrt_empujar(ctx, bucket->llave);
        static const int proto[] = {0, 0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_tabla_fijar_en_k2);
    }
}

static pdcrt_k pdcrt_tabla_fijar_en_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_fijar_en_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj llave = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj valor = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj b = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_bucket *bucket = b.pval;
    // [eq]
    bool ok;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Tabla#fijarEn: la función de igualdad debe devolver un booleano");
    (void) pdcrt_sacar(ctx);
    // []
    if(eq)
    {
        pdcrt_barrera_de_escritura(ctx, yo, valor);
        bucket->valor = valor;
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        if(!bucket->siguiente_colision)
        {
            pdcrt_bucket *nb = pdcrt_alojar_ctx(ctx, sizeof(pdcrt_bucket));
            if(!bucket)
                pdcrt_enomem(ctx);
            nb->siguiente_colision = NULL;
            pdcrt_barrera_de_escritura(ctx, yo, llave);
            nb->llave = llave;
            pdcrt_barrera_de_escritura(ctx, yo, valor);
            nb->valor = valor;
            nb->activo = true;
            bucket->siguiente_colision = nb;
            pdcrt_extender_pila(ctx, m, 1);
            pdcrt_empujar_nulo(ctx, m);
            return pdcrt_devolver(ctx, m, 1);
        }
        else
        {
            bucket = bucket->siguiente_colision;
            pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_voidptr(bucket));
            pdcrt_extender_pila(ctx, m, 3);
            pdcrt_empujar(ctx, yo.tabla->funcion_igualdad);
            pdcrt_empujar(ctx, llave);
            pdcrt_empujar(ctx, bucket->llave);
            static const int proto[] = { 0, 0 };
            return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2,
                                        &pdcrt_tabla_fijar_en_k2);
        }
    }
}

static pdcrt_k pdcrt_tabla_fijar_en_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_fijar_en_k3);
    // [_]
    (void) pdcrt_sacar(ctx);
    pdcrt_empujar_nulo(ctx, m);
    return pdcrt_devolver(ctx, m, 1);
}

static pdcrt_k pdcrt_tabla_en_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_en_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj llave = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj b = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj contiene = pdcrt_obtener_local(ctx, m, 3);
    (void) b;
    // [hash]
    bool ok;
    pdcrt_entero hash = pdcrt_obtener_entero(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Tabla#en: El hash debe ser un entero");
    (void) pdcrt_sacar(ctx);
    if(yo.tabla->num_buckets == 0)
        pdcrt_error(ctx, "Tabla#en: llave no encontrada");
    pdcrt_bucket *bucket = &yo.tabla->buckets[hash % yo.tabla->num_buckets];
    if(!bucket->activo)
    {
        if(contiene.bval)
        {
            pdcrt_empujar_booleano(ctx, m, false);
            return pdcrt_devolver(ctx, m, 1);
        }
        else
        {
            pdcrt_error(ctx, "Tabla#en: la llave no existe en la tabla");
        }
    }
    else
    {
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(bucket));
        pdcrt_empujar(ctx, yo.tabla->funcion_igualdad);
        pdcrt_empujar(ctx, llave);
        pdcrt_empujar(ctx, bucket->llave);
        static const int proto[] = {0, 0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_tabla_en_k2);
    }
}

static pdcrt_k pdcrt_tabla_en_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_en_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj llave = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj b = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj contiene = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_bucket *bucket = b.pval;
    // [eq]
    bool ok;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Tabla#en: la función de igualdad debe devolver un booleano");
    (void) pdcrt_sacar(ctx);
    // []
    if(eq)
    {
        if(contiene.bval)
        {
            pdcrt_empujar_booleano(ctx, m, true);
            return pdcrt_devolver(ctx, m, 1);
        }
        else
        {
            pdcrt_empujar(ctx, bucket->valor);
            return pdcrt_devolver(ctx, m, 1);
        }
    }
    else
    {
        bucket = bucket->siguiente_colision;
        if(!bucket)
        {
            if(contiene.bval)
            {
                pdcrt_empujar_booleano(ctx, m, false);
                return pdcrt_devolver(ctx, m, 1);
            }
            else
            {
                pdcrt_error(ctx, "Tabla#en: llave no encontrada");
            }
        }

        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(bucket));
        pdcrt_empujar(ctx, yo.tabla->funcion_igualdad);
        pdcrt_empujar(ctx, llave);
        pdcrt_empujar(ctx, bucket->llave);
        static const int proto[] = {0, 0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_tabla_en_k2);
    }
}

static pdcrt_k pdcrt_tabla_rehashear_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_rehashear_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj obucket = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj olista = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_obj otam_lista = pdcrt_obtener_local(ctx, m, 4);
    pdcrt_obj obuckets_ocupados = pdcrt_obtener_local(ctx, m, 5);
    pdcrt_bucket *bucket = obucket.pval;
    pdcrt_bucket *lista = olista.pval;
    pdcrt_entero i = oi.ival;
    pdcrt_entero tam_lista = otam_lista.ival;
    pdcrt_entero buckets_ocupados = obuckets_ocupados.ival;

loop:

    if(bucket && bucket->activo)
    {
        pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_voidptr(bucket));
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i));
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.tabla->funcion_hash);
        pdcrt_empujar(ctx, bucket->llave);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 1, &pdcrt_tabla_rehashear_k2);
    }
    else if((size_t) i >= yo.tabla->num_buckets)
    {
        // Fin de la iteración
        (void) pdcrt_liberar_tabla(ctx, yo.tabla->buckets, yo.tabla->num_buckets);
        yo.tabla->buckets = lista;
        yo.tabla->num_buckets = tam_lista;
        yo.tabla->limite_de_ocupacion = PDCRT_TABLA_LIMITE(tam_lista);
        yo.tabla->buckets_ocupados = buckets_ocupados;
        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        i = i + 1;
        if((size_t) i >= yo.tabla->num_buckets)
        {
            bucket = NULL;
            goto loop;
        }
        else
        {
            bucket = &yo.tabla->buckets[i];
            goto loop;
        }
    }
}

static pdcrt_k pdcrt_tabla_rehashear_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_rehashear_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj obucket = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj olista = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_obj otam_lista = pdcrt_obtener_local(ctx, m, 4);
    pdcrt_obj obuckets_ocupados = pdcrt_obtener_local(ctx, m, 5);
    pdcrt_bucket *bucket = obucket.pval;
    pdcrt_bucket *lista = olista.pval;
    pdcrt_entero i = oi.ival;
    pdcrt_entero tam_lista = otam_lista.ival;
    pdcrt_entero buckets_ocupados = obuckets_ocupados.ival;
    (void) i;
    (void) yo;

    // [hash]
    bool ok;
    pdcrt_entero hash = pdcrt_obtener_entero(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Tabla#rehashear: El hash tiene que ser un entero");
    (void) pdcrt_sacar(ctx);

    pdcrt_bucket *b = &lista[hash % tam_lista];
    if(b->activo)
    {
        while(b->siguiente_colision)
        {
            b = b->siguiente_colision;
        }
        pdcrt_bucket *nb = pdcrt_alojar_ctx(ctx, sizeof(pdcrt_bucket));
        if(!nb)
            pdcrt_enomem(ctx);
        nb->activo = true;
        nb->llave = bucket->llave;
        nb->valor = bucket->valor;
        nb->siguiente_colision = NULL;
        b->siguiente_colision = nb;
    }
    else
    {
        b->activo = true;
        b->llave = bucket->llave;
        b->valor = bucket->valor;
        b->siguiente_colision = NULL;
        buckets_ocupados += 1;
    }

    bucket = bucket->siguiente_colision;
    pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_voidptr(bucket));
    pdcrt_fijar_local(ctx, m, 5, pdcrt_objeto_entero(buckets_ocupados));
    return pdcrt_tabla_rehashear_k1(ctx, m);
}

static pdcrt_k pdcrt_tabla_eliminar_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_eliminar_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj llave = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj obucket = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj obucket_anterior = pdcrt_obtener_local(ctx, m, 3);
    (void) obucket;
    (void) obucket_anterior;
    // [hash]
    bool ok;
    pdcrt_entero hash = pdcrt_obtener_entero(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Tabla#eliminar: el hash de la llave debe ser un entero");
    if(yo.tabla->num_buckets == 0)
    {
        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_devolver(ctx, m, 1);
    }
    pdcrt_bucket *bucket = &yo.tabla->buckets[hash % yo.tabla->num_buckets];
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(bucket));
    if(!bucket->activo)
    {
        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_devolver(ctx, m, 1);
    }

    pdcrt_extender_pila(ctx, m, 3);
    pdcrt_empujar(ctx, yo.tabla->funcion_igualdad);
    pdcrt_empujar(ctx, llave);
    pdcrt_empujar(ctx, bucket->llave);
    static const int proto[] = {0, 0};
    return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_tabla_eliminar_k2);
}

static pdcrt_k pdcrt_tabla_eliminar_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_eliminar_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj llave = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj obucket = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj obucket_anterior = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_bucket *bucket = obucket.pval;
    pdcrt_bucket *bucket_anterior = obucket_anterior.pval;
    // [eq]
    bool ok;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Tabla#eliminar: la función de igualdad debe devolver un booleano");

    if(eq)
    {
        if(bucket_anterior)
        {
            bucket_anterior->siguiente_colision = bucket->siguiente_colision;
            pdcrt_desalojar_ctx(ctx, bucket, sizeof(bucket));
        }
        else
        {
            bucket->activo = false;
            bucket->llave = bucket->valor = pdcrt_objeto_nulo();
            bucket->siguiente_colision = NULL;
            yo.tabla->buckets_ocupados -= 1;
        }

        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        bucket_anterior = bucket;
        bucket = bucket->siguiente_colision;
        if(!bucket)
        {
            pdcrt_empujar_nulo(ctx, m);
            return pdcrt_devolver(ctx, m, 1);
        }

        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(bucket));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_voidptr(bucket_anterior));

        pdcrt_extender_pila(ctx, m, 3);
        pdcrt_empujar(ctx, yo.tabla->funcion_igualdad);
        pdcrt_empujar(ctx, llave);
        pdcrt_empujar(ctx, bucket->llave);
        static const int proto[] = {0, 0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_tabla_eliminar_k2);

    }
}

static pdcrt_k pdcrt_tabla_para_cada_par_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_para_cada_par_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj iterador = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj obucket = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_bucket *bucket = obucket.pval;
    pdcrt_entero i = oi.ival;

    if(!bucket || !bucket->activo)
    {
        i += 1;
        if((size_t) i >= yo.tabla->num_buckets)
        {
            pdcrt_empujar_nulo(ctx, m);
            return pdcrt_devolver(ctx, m, 1);
        }
        bucket = &yo.tabla->buckets[i];
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(bucket));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(i));
        return pdcrt_tabla_para_cada_par_k1(ctx, m);
    }
    else
    {
        pdcrt_extender_pila(ctx, m, 3);
        pdcrt_empujar(ctx, iterador);
        pdcrt_empujar(ctx, bucket->llave);
        pdcrt_empujar(ctx, bucket->valor);
        static const int proto[] = {0, 0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2,
                                    &pdcrt_tabla_para_cada_par_k2);
    }
}

static pdcrt_k pdcrt_tabla_para_cada_par_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_para_cada_par_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj iterador = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj obucket = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_bucket *bucket = obucket.pval;
    pdcrt_entero i = oi.ival;
    (void) yo;
    (void) iterador;
    (void) i;

    // [res]
    (void) pdcrt_sacar(ctx);
    bucket = bucket->siguiente_colision;
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_voidptr(bucket));
    return pdcrt_tabla_para_cada_par_k1(ctx, m);
}

pdcrt_k pdcrt_recv_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) yo;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, "Runtime");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.crearTabla))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearTabla necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok;
        pdcrt_entero n = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, u8"Runtime: crearTabla: su único argumento debe ser un entero");
        pdcrt_empujar_tabla_vacia(ctx, &k.marco, n);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.crearCorrutina))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearCorrutina necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_corrutina(ctx, &k.marco, -1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.recolectar_basura))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: recolectarBasura no necesita argumentos");
        pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_memoria(ctx, 0);
        pdcrt_recolectar_basura_simple(ctx, &k.marco, params);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.crear_instancia))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: crearInstancia necesita 3 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [num_atrs, metodos, metodo_no_encontrado]
        bool ok = false;
        pdcrt_entero num_atrs = pdcrt_obtener_entero(ctx, -3, &ok);;
        if(!ok)
            pdcrt_error(ctx, "Runtime: crearInstancia: numAtrs debe ser un entero");
        pdcrt_empujar_instancia(ctx, &k.marco, -2, -1, num_atrs);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.atributos_de_instancia))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: atributosDeInstancia necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        pdcrt_empujar_entero(ctx, k.marco, (pdcrt_entero) inst.inst->num_atributos);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_metodos))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: obtenerMétodos necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        pdcrt_empujar(ctx, inst.inst->metodos);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_atributo))
    {
        if(args != 2)
            pdcrt_error(ctx, "Runtime: obtenerAtributo necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst, atr]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        bool ok = false;
        pdcrt_entero atr = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: obtenerAtributo: el atributo debe ser un entero");
        if(atr < 0 || (size_t) atr >= inst.inst->num_atributos)
            pdcrt_error(ctx, u8"Runtime: obtenerAtributo: índice de atributo inválido");
        pdcrt_empujar(ctx, inst.inst->atributos[atr]);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_atributo))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: fijarAtributo necesita 3 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst, atr, valor]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        bool ok = false;
        pdcrt_entero atr = pdcrt_obtener_entero(ctx, -2, &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: fijarAtributo: el atributo debe ser un entero");
        if(atr < 0 || (size_t) atr >= inst.inst->num_atributos)
            pdcrt_error(ctx, u8"Runtime: fijarAtributo: índice de atributo inválido");
        pdcrt_obj valor = ctx->pila[argp + 2];
        pdcrt_barrera_de_escritura(ctx, inst, valor);
        inst.inst->atributos[atr] = valor;
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.es_instancia))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: esInstancia necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, k.marco, pdcrt_tipo_de_obj(inst) == PDCRT_TOBJ_INSTANCIA);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.enviar_mensaje))
    {
        if(args < 2)
            pdcrt_error(ctx, "Runtime: enviarMensaje necesita al menos 2 argumentos");
        pdcrt_eliminar_elementos(ctx, inic, 2); // Saca yo y msj, deja solo los argumentos
        // [obj, msj, ...args]
        return pdcrt_enviar_mensaje_obj(ctx, k.marco, NULL, args - 2, k.kf);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fallar_con_mensaje))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: fallarConMensaje necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [texto]
        pdcrt_obj texto = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, texto, PDCRT_TOBJ_TEXTO);
        pdcrt_error(ctx, texto.texto->contenido);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.leer_caracter))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: leerCarácter no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        int c = getchar();
        if(c == EOF)
            c = -1;
        pdcrt_empujar_entero(ctx, k.marco, c);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_argv))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerArgv no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, ctx->argv);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_programa))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerPrograma no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, ctx->nombre_del_programa);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_clase_objeto))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerClaseObjeto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, ctx->clase_objeto);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_objeto))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseObjeto necesita 1 argumento");
        ctx->clase_objeto = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_arreglo))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseArreglo necesita 1 argumento");
        ctx->clase_arreglo = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_boole))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseBoole necesita 1 argumento");
        ctx->clase_boole = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_numero))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseNumero necesita 1 argumento");
        ctx->clase_numero = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_procedimiento))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseProcedimieto necesita 1 argumento");
        ctx->clase_procedimiento = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_tipo_nulo))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTipoNulo necesita 1 argumento");
        ctx->clase_tipo_nulo = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_texto))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTexto necesita 1 argumento");
        ctx->clase_texto = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    assert(0 && "sin implementar");
}

pdcrt_k pdcrt_recv_voidptr(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Voidptr: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Voidptr: %p", yo.pval);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    assert(0 && "sin implementar");
}

pdcrt_k pdcrt_recv_valop(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Valop: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Valop: %p", yo.valop);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k4(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k5(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_espacio_de_nombres(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, pdcrt_objeto_tabla(yo.tabla)); // yo_tbl
    pdcrt_fijar_local(ctx, m, 1, msj); // msj
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_booleano(false)); // esAutoejecutable

    pdcrt_eliminar_elementos(ctx, inic, 2); // elimina yo y msj
    return pdcrt_recv_espacio_de_nombres_k1(ctx, m);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k1);
    pdcrt_obj yo_tbl = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_empujar(ctx, yo_tbl);
    pdcrt_empujar(ctx, msj);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_recv_espacio_de_nombres_k2);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k2);
    // [tupla(valor,esAuto)]
    pdcrt_duplicar(ctx, m, -1);
    // [tupla, tupla]
    pdcrt_empujar_entero(ctx, m, 1);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_recv_espacio_de_nombres_k3);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k3);
    pdcrt_obj yo_tbl = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    (void) yo_tbl;
    (void) msj;
    // [tupla, esAuto]
    bool ok;
    bool esAutoejecutable = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "se esperaba un booleano como 'esAutoejecutable' del espacio de nombres");
    (void) pdcrt_sacar(ctx);
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_booleano(esAutoejecutable));
    // [tupla]
    pdcrt_empujar_entero(ctx, m, 0);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_recv_espacio_de_nombres_k4);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k4);
    // [args..., valor]
    pdcrt_obj esAutoejecutable = pdcrt_obtener_local(ctx, m, 2);
    if(!esAutoejecutable.bval)
    {
        if(m->args != 0)
            pdcrt_error(ctx, "tratando de llamar a valor exportado no autoejecutable");
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        pdcrt_insertar(ctx, -(1 + (pdcrt_stp) m->args));
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, NULL, m->args,
                                    &pdcrt_recv_espacio_de_nombres_k5);
    }
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k5(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k5);
    return pdcrt_devolver(ctx, m, 1);
}

static pdcrt_k pdcrt_corrutina_generar(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_corrutina_avanzar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_corrutina(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Corrutina: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Corrutina: %p", yo.coro);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Corrutina: igualA / operador_= necesita 1 argumento");

        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CORRUTINA)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.coro == arg.coro);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Corrutina: distintoDe / operador_no= necesita 1 argumento");

        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CORRUTINA)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.coro != arg.coro);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.finalizada))
    {
        if(args != 0)
            pdcrt_error(ctx, "Corrutina: finalizada no necesita argumentos");
        pdcrt_empujar_booleano(ctx, k.marco, yo.coro->estado == PDCRT_CORO_FINALIZADA);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.avanzar))
    {
        if(args != 0 && args != 1)
            pdcrt_error(ctx, "Corrutina: avanzar necesita 0 o 1 argumentos");

        pdcrt_obj kval;
        if(args == 1)
            kval = ctx->pila[argp];
        else
            kval = pdcrt_objeto_nulo();

        if(yo.coro->estado == PDCRT_CORO_INICIAL)
        {
            pdcrt_extender_pila(ctx, k.marco, 4);
            pdcrt_empujar(ctx, yo.coro->punto_de_inicio);
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 1, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            yo.coro->estado = PDCRT_CORO_EJECUTANDOSE;
            yo.coro->punto_de_continuacion = k;
            pdcrt_empujar(ctx, yo);
            pdcrt_empujar_closure(ctx, &m, &pdcrt_corrutina_generar, 1);
            pdcrt_empujar(ctx, kval);
            PDCRT_SACAR_PRELUDIO();
            static const int proto[] = {0, 0};
            return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_recv_corrutina_avanzar_k1);
        }
        else if(yo.coro->estado == PDCRT_CORO_SUSPENDIDA)
        {
            pdcrt_k sus = yo.coro->punto_de_suspencion;
            yo.coro->estado = PDCRT_CORO_EJECUTANDOSE;
            yo.coro->punto_de_continuacion = k;
            pdcrt_extender_pila(ctx, k.marco, 1);
            pdcrt_empujar(ctx, kval);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, sus);
        }
        else
        {
            pdcrt_error(ctx, "No se puede avanzar una corrutina que se esta ejecutando o esta finalizada");
        }
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_corrutina_generar(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    if(args != 1)
        pdcrt_error(ctx, "Corrutina: generador debe llamarse con un argumento");
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 0, 1, args, k);
    pdcrt_obj obj_coro = pdcrt_obtener_captura(ctx, m, 0);
    pdcrt_corrutina *coro = obj_coro.coro;
    // [res]
    if(coro->estado != PDCRT_CORO_EJECUTANDOSE)
    {
        pdcrt_error(ctx, "Corrutina: no se puede generar un valor para una corrutina que no se esta ejecutando");
    }
    pdcrt_k coro_k = coro->punto_de_continuacion;
    coro->estado = PDCRT_CORO_SUSPENDIDA;
    coro->punto_de_suspencion = k;
    return pdcrt_continuar(ctx, coro_k);
}

static pdcrt_k pdcrt_recv_corrutina_avanzar_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_corrutina_avanzar_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    if(yo.coro->estado != PDCRT_CORO_EJECUTANDOSE)
    {
        pdcrt_error(ctx, "No se puede devolver de una corrutina que no se esta ejecutando");
    }
    yo.coro->estado = PDCRT_CORO_FINALIZADA;
    return pdcrt_devolver(ctx, m, 1);
}

static pdcrt_k pdcrt_instancia_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_instancia_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_instancia_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_instancia_k4(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_instancia(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    pdcrt_marco *m = pdcrt_crear_marco(ctx, 2, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, yo);
    pdcrt_fijar_local(ctx, m, 1, msj);
    pdcrt_eliminar_elementos(ctx, (pdcrt_stp) inic, 2);
    // [...#args]

    pdcrt_extender_pila(ctx, m, 2);
    pdcrt_empujar(ctx, yo.inst->metodos);
    pdcrt_empujar(ctx, msj);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "contiene", 8, proto, 1, &pdcrt_instancia_k1);
}

static pdcrt_k pdcrt_instancia_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);

    // [...#args, contiene?]
    bool ok = false;
    bool res = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Se esperaba booleano al llamar a #contiene en los métodos de una instancia");
    (void) pdcrt_sacar(ctx);

    if(res)
    {
        // Obtén y llama al método
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.inst->metodos);
        pdcrt_empujar(ctx, msj);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_instancia_k2);
    }
    else
    {
        // Llama al metodo_no_encontrado

        if(pdcrt_tipo_de_obj(yo.inst->metodo_no_encontrado) == PDCRT_TOBJ_BOOLEANO)
        {
            if(yo.inst->metodo_no_encontrado.bval)
            {
                pdcrt_extender_pila(ctx, m, 2);
                pdcrt_empujar(ctx, yo.inst->metodos);
                pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.mensaje_no_encontrado));
                static const int proto[] = {0};
                return pdcrt_enviar_mensaje(ctx, m, "contiene", 8, proto, 1, &pdcrt_instancia_k3);
            }
            else
            {
                pdcrt_inspeccionar_texto(msj.texto);
                pdcrt_error(ctx, u8"Método no encontrado");
            }
        }
        else
        {
            pdcrt_extender_pila(ctx, m, 2);
            pdcrt_empujar(ctx, yo.inst->metodo_no_encontrado);
            pdcrt_empujar(ctx, yo);
            pdcrt_empujar(ctx, msj);
            pdcrt_mover_a_cima(ctx, m, -(m->args + 3), m->args);
            // [metodo_no_encontrado, yo, msj, ...#args]
            return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 2, m->k.kf);
        }
    }
}

static pdcrt_k pdcrt_instancia_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    (void) msj;
    // [...#args, método]
    pdcrt_empujar(ctx, yo);
    pdcrt_mover_a_cima(ctx, m, -(m->args + 2), m->args);
    // [metodo, yo, ...#args]
    return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 1, m->k.kf);
}

static pdcrt_k pdcrt_instancia_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k3);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    // [...#args, contiene?]
    bool ok = false;
    bool res = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Se esperaba booleano al llamar a #contiene en los métodos de una instancia");
    (void) pdcrt_sacar(ctx);
    if(res)
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.inst->metodos);
        pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.mensaje_no_encontrado));
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_instancia_k4);
    }
    else
    {
        pdcrt_inspeccionar_texto(msj.texto);
        pdcrt_error(ctx, u8"Método no encontrado");
    }
}

static pdcrt_k pdcrt_instancia_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k4);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    // [...#args, método]
    pdcrt_empujar(ctx, yo);
    pdcrt_empujar(ctx, msj);
    pdcrt_mover_a_cima(ctx, m, -(m->args + 3), m->args);
    // [metodo, yo, msj, ...#args]
    return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 2, m->k.kf);
}

pdcrt_k pdcrt_recv_reubicado(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;
    (void) yo;
    (void) k;

    assert(0 && "sin implementar");
}


static pdcrt_entero pdcrt_hash(pdcrt_ctx *ctx, pdcrt_obj o)
{
    switch(pdcrt_tipo_de_obj(o))
    {
        case PDCRT_TOBJ_ENTERO:
            return o.ival;
        case PDCRT_TOBJ_FLOAT:
            return (pdcrt_entero) *(pdcrt_efloat *) &o.fval;
        case PDCRT_TOBJ_TEXTO:
            return (pdcrt_entero) o.texto;
        case PDCRT_TOBJ_BOOLEANO:
            return o.bval ? 0 : 1;
        case PDCRT_TOBJ_NULO:
            return 0;
        default:
            pdcrt_error(ctx, "No se puede hashear objeto de ese tipo");
    }
}


static pdcrt_k pdcrt_funcion_igualdad_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_funcion_igualdad(pdcrt_ctx *ctx, int args, struct pdcrt_k k)
{
    if(args != 2)
        pdcrt_error(ctx, u8"FunciónIgualdad: se esperaban 2 argumentos");
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 2, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, pdcrt_sacar(ctx));
    pdcrt_fijar_local(ctx, m, 1, pdcrt_sacar(ctx));
    pdcrt_empujar(ctx, pdcrt_obtener_local(ctx, m, 0));
    pdcrt_empujar(ctx, pdcrt_obtener_local(ctx, m, 1));
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "igualA", 6, proto, 1, &pdcrt_funcion_igualdad_k1);
}

static pdcrt_k pdcrt_funcion_igualdad_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_funcion_igualdad_k1);
    // [eq]
    pdcrt_extender_pila(ctx, m, 1);
    return pdcrt_devolver(ctx, m, 1);
}

static pdcrt_k pdcrt_funcion_hash(pdcrt_ctx *ctx, int args, struct pdcrt_k k)
{
    if(args != 1)
        pdcrt_error(ctx, u8"FunciónHash: se esperaba 1 argumento");
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 1, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, pdcrt_sacar(ctx));
    pdcrt_empujar_entero(ctx, m, pdcrt_hash(ctx, pdcrt_obtener_local(ctx, m, 0)));
    return pdcrt_devolver(ctx, m, 1);
}



static pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o)
{
    if(o.recv == &pdcrt_recv_entero)
    {
        return PDCRT_TOBJ_ENTERO;
    }
    else if(o.recv == &pdcrt_recv_float)
    {
        return PDCRT_TOBJ_FLOAT;
    }
    else if(o.recv == &pdcrt_recv_booleano)
    {
        return PDCRT_TOBJ_BOOLEANO;
    }
    else if(o.recv == &pdcrt_recv_marco)
    {
        return PDCRT_TOBJ_MARCO;
    }
    else if(o.recv == &pdcrt_recv_texto)
    {
        return PDCRT_TOBJ_TEXTO;
    }
    else if(o.recv == &pdcrt_recv_nulo)
    {
        return PDCRT_TOBJ_NULO;
    }
    else if(o.recv == &pdcrt_recv_arreglo)
    {
        return PDCRT_TOBJ_ARREGLO;
    }
    else if(o.recv == &pdcrt_recv_closure)
    {
        return PDCRT_TOBJ_CLOSURE;
    }
    else if(o.recv == &pdcrt_recv_caja)
    {
        return PDCRT_TOBJ_CAJA;
    }
    else if(o.recv == &pdcrt_recv_tabla)
    {
        return PDCRT_TOBJ_TABLA;
    }
    else if(o.recv == &pdcrt_recv_runtime)
    {
        return PDCRT_TOBJ_RUNTIME;
    }
    else if(o.recv == &pdcrt_recv_voidptr)
    {
        return PDCRT_TOBJ_VOIDPTR;
    }
    else if(o.recv == &pdcrt_recv_valop)
    {
        return PDCRT_TOBJ_VALOP;
    }
    else if(o.recv == &pdcrt_recv_espacio_de_nombres)
    {
        return PDCRT_TOBJ_ESPACIO_DE_NOMBRES;
    }
    else if(o.recv == &pdcrt_recv_corrutina)
    {
        return PDCRT_TOBJ_CORRUTINA;
    }
    else if(o.recv == &pdcrt_recv_instancia)
    {
        return PDCRT_TOBJ_INSTANCIA;
    }
    else if(o.recv == &pdcrt_recv_reubicado)
    {
        return PDCRT_TOBJ_REUBICADO;
    }
    else
    {
        assert(0 && "inalcanzable");
    }
}


void *pdcrt_alojar_ctx(pdcrt_ctx *ctx, size_t bytes)
{
    return pdcrt_alojar(ctx->alojador, bytes);
}

void *pdcrt_realojar_ctx(pdcrt_ctx *ctx, void *ptr, size_t tam_actual, size_t tam_nuevo)
{
    return pdcrt_realojar(ctx->alojador, ptr, tam_actual, tam_nuevo);
}

void pdcrt_desalojar_ctx(pdcrt_ctx *ctx, void *ptr, size_t tam_actual)
{
    pdcrt_desalojar(ctx->alojador, ptr, tam_actual);
}

static inline size_t pdcrt_stp_a_pos(pdcrt_ctx *ctx, pdcrt_stp i)
{
    if(i < 0)
        return ctx->tam_pila + i;
    else
        return i;
}

static void pdcrt_inicializar_marco_impl(pdcrt_ctx *ctx,
                                         pdcrt_marco *m,
                                         size_t locales,
                                         size_t capturas,
                                         int args,
                                         pdcrt_k k)
{
    m->args = args;
    m->k = k;
    m->num_locales = locales;
    m->num_capturas = capturas;
    size_t top = ctx->tam_pila;
    for(size_t i = 0; i < capturas; i++)
    {
        pdcrt_obj val = ctx->pila[(top - capturas) + i];
        pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(val);
        if(vh)
            pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(m), vh);
        m->locales_y_capturas[locales + i] = val;
    }
    for(size_t i = 0; i < locales; i++)
    {
        m->locales_y_capturas[i] = pdcrt_objeto_nulo();
    }
    ctx->tam_pila -= capturas;
}

pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, size_t locales, size_t capturas, int args, pdcrt_k k)
{
    pdcrt_marco *m = pdcrt_alojar_obj(ctx, &k.marco, PDCRT_TGC_MARCO, sizeof(pdcrt_marco) + sizeof(pdcrt_obj) * (locales + capturas));
    if(!m)
        pdcrt_enomem(ctx);
    pdcrt_inicializar_marco_impl(ctx, m, locales, capturas, args, k);
    return m;
}

void pdcrt_inicializar_marco(pdcrt_ctx *ctx,
                             pdcrt_marco *m,
                             size_t sz,
                             size_t locales,
                             size_t capturas,
                             int args,
                             pdcrt_k k)
{
    pdcrt_inicializar_obj(ctx, PDCRT_CABECERA_GC(m), PDCRT_TGC_MARCO, sz);
    pdcrt_inicializar_marco_impl(ctx, m, locales, capturas, args, k);
    m->gc.en_la_pila = true;
    pdcrt_gc_mover_a_grupo(&ctx->gc.blanco_joven, &ctx->gc.blanco_en_la_pila, PDCRT_CABECERA_GC(m));
}

pdcrt_arreglo* pdcrt_crear_arreglo_vacio(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad)
{
    pdcrt_arreglo *a = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_ARREGLO, sizeof(pdcrt_arreglo));
    if(!a)
        pdcrt_enomem(ctx);
    if(capacidad == 0)
    {
        a->valores = NULL;
    }
    else
    {
        a->valores = pdcrt_alojar_ctx(ctx, sizeof(pdcrt_obj) * capacidad);
        if(!a->valores)
            pdcrt_enomem(ctx);
    }
    a->longitud = 0;
    a->capacidad = capacidad;
    return a;
}

pdcrt_closure* pdcrt_crear_closure(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_f f, size_t capturas)
{
    pdcrt_closure *c = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_CLOSURE,
                                        sizeof(pdcrt_closure) + sizeof(pdcrt_obj) * capturas);
    if(!c)
        pdcrt_enomem(ctx);
    c->f = f;
    c->num_capturas = capturas;
    for(size_t i = 0; i < capturas; i++)
    {
        c->capturas[i] = pdcrt_objeto_nulo();
    }
    return c;
}

pdcrt_caja* pdcrt_crear_caja(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_obj valor)
{
    pdcrt_caja *c = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_CAJA, sizeof(pdcrt_caja));
    if(!c)
        pdcrt_enomem(ctx);
    c->valor = valor;
    return c;
}

pdcrt_tabla* pdcrt_crear_tabla(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad)
{
    pdcrt_tabla *tbl = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_TABLA, sizeof(pdcrt_tabla));
    if(!tbl)
        pdcrt_enomem(ctx);
    tbl->num_buckets = capacidad;
    tbl->buckets = NULL;
    tbl->limite_de_ocupacion = PDCRT_TABLA_LIMITE(capacidad);
    tbl->buckets_ocupados = 0;
    tbl->funcion_igualdad = ctx->funcion_igualdad;
    tbl->funcion_hash = ctx->funcion_hash;
    if(capacidad > 0)
    {
        tbl->buckets = pdcrt_alojar_ctx(ctx, sizeof(pdcrt_bucket) * capacidad);
        if(!tbl->buckets)
            pdcrt_enomem(ctx);
        pdcrt_tabla_inicializar(ctx, tbl);
    }
    return tbl;
}

pdcrt_valop* pdcrt_crear_valop(pdcrt_ctx *ctx, pdcrt_marco **m, size_t num_bytes)
{
    pdcrt_valop *valop = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_VALOP, sizeof(pdcrt_valop) + num_bytes);
    if(!valop)
        pdcrt_enomem(ctx);
    memset(valop->datos, 0, num_bytes);
    return valop;
}

pdcrt_corrutina* pdcrt_crear_corrutina(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp f_idx)
{
    pdcrt_corrutina *coro = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_CORO, sizeof(pdcrt_corrutina));
    if(!coro)
        pdcrt_enomem(ctx);
    pdcrt_obj cuerpo = ctx->pila[pdcrt_stp_a_pos(ctx, f_idx)];
    coro->estado = PDCRT_CORO_INICIAL;
    coro->punto_de_inicio = cuerpo;
    return coro;
}

pdcrt_instancia* pdcrt_crear_instancia(pdcrt_ctx *ctx, pdcrt_marco **m,
                                       pdcrt_stp metodos, pdcrt_stp metodo_no_encontrado, size_t num_atrs)
{
    pdcrt_instancia *inst = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_INSTANCIA, sizeof(pdcrt_instancia) + (sizeof(pdcrt_obj) * num_atrs));
    if(!inst)
        pdcrt_enomem(ctx);
    pdcrt_obj ometodos = ctx->pila[pdcrt_stp_a_pos(ctx, metodos)];
    pdcrt_obj ometodo_no_encontrado = ctx->pila[pdcrt_stp_a_pos(ctx, metodo_no_encontrado)];
    inst->metodos = ometodos;
    inst->metodo_no_encontrado = ometodo_no_encontrado;
    inst->num_atributos = num_atrs;
    for(size_t i = 0; i < num_atrs; i++)
    {
        inst->atributos[i] = pdcrt_objeto_nulo();
    }
    return inst;
}

void pdcrt_empujar_interceptar(pdcrt_ctx *ctx, pdcrt_obj o)
{
    pdcrt_empujar_ll(ctx, o);
}

void pdcrt_fijar_pila_interceptar(pdcrt_ctx *ctx, size_t i, pdcrt_obj v)
{
    pdcrt_fijar_pila_ll(ctx, i, v);
}


pdcrt_ctx *pdcrt_crear_contexto(pdcrt_aloj *aloj)
{
    pdcrt_aloj *est = pdcrt_alojador_con_estadisticas(aloj);
    if(!est)
        return NULL;

    pdcrt_ctx *ctx = pdcrt_alojar(est, sizeof(pdcrt_ctx));
    if(!ctx)
        return NULL;

    ctx->recolector_de_basura_activo = true;
    ctx->alojador = est;

    ctx->tam_pila = 0;
    ctx->pila = NULL;
    ctx->cap_pila = 0;

    // TODO: Todas las funciones que alojan objetos en esta función, si se
    //  quedan sin memoria, fallarán con enomem terminando el proceso. La
    //  solución es poder un "handler" de errores al crear el contexto.

    ctx->gc.blanco_viejo.primero = ctx->gc.blanco_viejo.ultimo = NULL;
    ctx->gc.blanco_viejo.grupo = PDCRT_TGRP_BLANCO_VIEJO;
    ctx->gc.blanco_joven.primero = ctx->gc.blanco_joven.ultimo = NULL;
    ctx->gc.blanco_joven.grupo = PDCRT_TGRP_BLANCO_JOVEN;
    ctx->gc.blanco_en_la_pila.primero = ctx->gc.blanco_en_la_pila.ultimo = NULL;
    ctx->gc.blanco_en_la_pila.grupo = PDCRT_TGRP_BLANCO_EN_LA_PILA;
    ctx->gc.gris.primero = ctx->gc.gris.ultimo = NULL;
    ctx->gc.gris.grupo = PDCRT_TGRP_GRIS;
    ctx->gc.negro.primero = ctx->gc.negro.ultimo = NULL;
    ctx->gc.negro.grupo = PDCRT_TGRP_NEGRO;
    ctx->gc.raices_viejas.primero = ctx->gc.raices_viejas.ultimo = NULL;
    ctx->gc.raices_viejas.grupo = PDCRT_TGRP_RAICES_VIEJAS;

    ctx->gc.tam_heap = 10 * 1024 * 1024; // 10MiB

    ctx->gc.num_recolecciones = 0;

    ctx->primer_marco_activo = ctx->ultimo_marco_activo = NULL;

    ctx->cnt = 0;

    ctx->funcion_hash = ctx->funcion_igualdad = pdcrt_objeto_nulo();

    ctx->registro_de_espacios_de_nombres = pdcrt_objeto_nulo();
    ctx->registro_de_modulos = pdcrt_objeto_nulo();
    ctx->espacio_de_nombres_runtime = pdcrt_objeto_nulo();

    ctx->argv = pdcrt_objeto_nulo();
    ctx->nombre_del_programa = pdcrt_objeto_nulo();

    ctx->clase_objeto = pdcrt_objeto_nulo();
    ctx->clase_arreglo = pdcrt_objeto_nulo();
    ctx->clase_numero = pdcrt_objeto_nulo();
    ctx->clase_boole = pdcrt_objeto_nulo();
    ctx->clase_procedimiento = pdcrt_objeto_nulo();
    ctx->clase_tipo_nulo = pdcrt_objeto_nulo();
    ctx->clase_texto = pdcrt_objeto_nulo();

    ctx->tam_textos = ctx->cap_textos = 0;
    ctx->textos = NULL;

    // TODO: Esto debería estár en `pdcrt_ejecutar_opt`.
    ctx->inicio_del_stack = 0;
    ctx->tam_stack = 3 * 1024 * 1024; // 3 MiB

    ctx->hay_un_manejador_de_errores = false;
    ctx->mensaje_de_error = NULL;

    ctx->hay_una_salida_del_trampolin = false;
    ctx->continuacion_actual = (pdcrt_k) { .kf = NULL, .marco = NULL };

    pdcrt_marco *m = NULL;

#define PDCRT_X(nombre, texto) ctx->textos_globales.nombre = NULL;
    PDCRT_TABLA_TEXTOS(PDCRT_X);
#undef PDCRT_X
#define PDCRT_X(nombre, texto) ctx->textos_globales.nombre = pdcrt_crear_texto_desde_cstr(ctx, &m, texto);
    PDCRT_TABLA_TEXTOS(PDCRT_X);
#undef PDCRT_X

    ctx->funcion_igualdad = pdcrt_objeto_closure(
            pdcrt_crear_closure(ctx, &m, &pdcrt_funcion_igualdad, 0));
    ctx->funcion_hash = pdcrt_objeto_closure(
            pdcrt_crear_closure(ctx, &m, &pdcrt_funcion_hash, 0));

    ctx->registro_de_espacios_de_nombres = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, &m, 0));
    ctx->registro_de_modulos = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, &m, 0));

    ctx->capacidades.time = !!pdcrt_time(NULL);

    ctx->log.gc =
#ifdef PDCRT_LOG_GC
        true
#else
        false
#endif
        ;

    return ctx;
}

void pdcrt_fijar_argv(pdcrt_ctx *ctx, int argc, char **argv)
{
    pdcrt_marco *m = NULL;
    pdcrt_extender_pila(ctx, NULL, 1);
    if(argc > 0)
    {
        pdcrt_obj nm = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, argv[0], strlen(argv[0])));
        ctx->nombre_del_programa = nm;
    }
    pdcrt_empujar_arreglo_vacio(ctx, &m, argc - 1);
    pdcrt_obj arr = pdcrt_cima(ctx);
    for(int i = 1; i < argc; i++)
    {
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, argv[i], strlen(argv[i])));
        pdcrt_barrera_de_escritura(ctx, arr, txt);
        arr.arreglo->valores[arr.arreglo->longitud++] = txt;
    }
    ctx->argv = arr;
    (void) pdcrt_sacar(ctx);
}

void pdcrt_convertir_a_espacio_de_nombres(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_obj o = pdcrt_sacar(ctx);
    pdcrt_debe_tener_tipo(ctx, o, PDCRT_TOBJ_TABLA);
    pdcrt_empujar(ctx, pdcrt_objeto_espacio_de_nombres(o.tabla));
}

static void pdcrt_preparar_registros(pdcrt_ctx *ctx, size_t num_mods)
{
    pdcrt_marco *m = NULL;
    ctx->registro_de_espacios_de_nombres = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, &m, num_mods));
    ctx->registro_de_modulos = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, &m, num_mods));
}

static void pdcrt_liberar_grupo(pdcrt_ctx *ctx, pdcrt_gc_grupo *grupo)
{
    for(pdcrt_cabecera_gc *h = grupo->primero; h != NULL;)
    {
        pdcrt_cabecera_gc *s = h->siguiente;
        h->grupo = PDCRT_TGRP_NINGUNO;
        h->siguiente = h->anterior = NULL;
        pdcrt_gc_liberar_objeto(ctx, h);
        h = s;
    }
    grupo->primero = grupo->ultimo = NULL;
}

void pdcrt_cerrar_contexto(pdcrt_ctx *ctx)
{
    pdcrt_liberar_grupo(ctx, &ctx->gc.blanco_joven);
    pdcrt_liberar_grupo(ctx, &ctx->gc.blanco_viejo);
    pdcrt_liberar_grupo(ctx, &ctx->gc.blanco_en_la_pila);
    pdcrt_liberar_grupo(ctx, &ctx->gc.gris);
    pdcrt_liberar_grupo(ctx, &ctx->gc.negro);
    pdcrt_liberar_grupo(ctx, &ctx->gc.raices_viejas);
    pdcrt_desalojar_ctx(ctx, ctx->textos, sizeof(pdcrt_texto *) * ctx->cap_textos);
    pdcrt_desalojar_ctx(ctx, ctx->pila, sizeof(pdcrt_obj) * ctx->cap_pila);
    pdcrt_aloj *aloj = ctx->alojador;
    pdcrt_desalojar(ctx->alojador, ctx, sizeof(pdcrt_ctx));
    pdcrt_desalojar_alojador_con_estadisticas(aloj);
}


void pdcrt_extender_pila(pdcrt_ctx *ctx, pdcrt_marco *m, size_t num_elem)
{
    (void) m;
    if((num_elem + ctx->tam_pila) > ctx->cap_pila)
    {
        size_t nueva_cap = num_elem + ctx->tam_pila;
        void *nueva_pila = pdcrt_realojar_ctx(ctx, ctx->pila,
            sizeof(pdcrt_obj) * ctx->cap_pila,
            sizeof(pdcrt_obj) * nueva_cap);
        if(!nueva_pila)
            pdcrt_enomem(ctx);
        ctx->pila = nueva_pila;
        ctx->cap_pila = nueva_cap;
    }
}

void pdcrt_empujar_entero(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_entero i)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_entero(i));
}

void pdcrt_empujar_booleano(pdcrt_ctx *ctx, pdcrt_marco *m, bool v)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_booleano(v));
}

void pdcrt_empujar_float(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_float f)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_float(f));
}

void pdcrt_empujar_espacio_de_nombres(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_empujar_tabla_vacia(ctx, m, 24);
}

void pdcrt_empujar_texto(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str, size_t len)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_texto *txt = pdcrt_crear_texto(ctx, m, str, len);
    pdcrt_empujar(ctx, pdcrt_objeto_texto(txt));
}

void pdcrt_empujar_texto_cstr(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str)
{
    pdcrt_empujar_texto(ctx, m, str, strlen(str));
}

void pdcrt_empujar_nulo(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_nulo());
}

void pdcrt_empujar_arreglo_vacio(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_arreglo(pdcrt_crear_arreglo_vacio(ctx, m, capacidad));
    pdcrt_empujar(ctx, obj);
}

void pdcrt_empujar_caja_vacia(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_caja(pdcrt_crear_caja(ctx, m, pdcrt_objeto_nulo()));
    pdcrt_empujar(ctx, obj);
}

void pdcrt_empujar_tabla_vacia(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, m, capacidad));
    pdcrt_empujar(ctx, obj);
}

void pdcrt_obtener_objeto_runtime(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_obj obj = pdcrt_objeto_runtime();
    pdcrt_empujar(ctx, obj);
}

void* pdcrt_empujar_valop(pdcrt_ctx *ctx, pdcrt_marco **m, size_t num_bytes)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_valop(pdcrt_crear_valop(ctx, m, num_bytes));
    pdcrt_empujar(ctx, obj);
    return obj.valop->datos;
}

void pdcrt_empujar_voidptr(pdcrt_ctx *ctx, pdcrt_marco *m, void* ptr)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_obj obj = pdcrt_objeto_voidptr(ptr);
    pdcrt_empujar(ctx, obj);
}

void pdcrt_empujar_corrutina(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp f)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_corrutina(pdcrt_crear_corrutina(ctx, m, f));
    pdcrt_empujar(ctx, obj);
}

void pdcrt_empujar_instancia(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp metodos, pdcrt_stp metodo_no_encontrado, size_t num_atrs)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_instancia(
        pdcrt_crear_instancia(ctx, m, metodos, metodo_no_encontrado, num_atrs));
    pdcrt_empujar(ctx, obj);
}

pdcrt_entero pdcrt_obtener_entero(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];

    *ok = false;
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_ENTERO)
    {
        *ok = true;
        return o.ival;
    }
    else if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_FLOAT)
    {
        int exp = 0;
        PDCRT_FLOAT_FREXP(o.fval, &exp);
        if(exp > 0) // El float no tiene parte decimal
        {
            if(PDCRT_FLOAT_MANT_DIG >= PDCRT_ENTERO_BITS)
            {
                // Cualquier entero es representable dentro de un float:
                pdcrt_float max = (pdcrt_float) PDCRT_ENTERO_MAX;
                pdcrt_float min = (pdcrt_float) PDCRT_ENTERO_MIN;
                if(o.fval >= min && o.fval <= max)
                {
                    *ok = true;
                    return (pdcrt_entero) o.fval;
                }
            }
            else if((size_t) exp <= PDCRT_ENTERO_BITS)
            {
                // Recuerda que exp es la cantidad de bits antes del punto
                // decimal, esto significa que si exp <= PDCRT_ENTERO_BITS el
                // float puede ser convertido a un entero.
                *ok = true;
                return (pdcrt_entero) o.fval;
            }
        }
    }

    return 0;
}

pdcrt_float pdcrt_obtener_float(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];

    *ok = false;
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_ENTERO)
    {
        if(PDCRT_FLOAT_MANT_DIG >= PDCRT_ENTERO_BITS)
        {
            *ok = true;
            return (pdcrt_float) o.ival;
        }
        else
        {
            // No todos los enteros son representables como floats
            static const pdcrt_entero max_entero_repr_float = (1ULL << PDCRT_FLOAT_MANT_DIG) - 1U;
            static const pdcrt_entero min_entero_repr_float = -(1ULL << PDCRT_FLOAT_MANT_DIG);
            if(o.ival >= min_entero_repr_float && o.ival <= max_entero_repr_float)
            {
                *ok = true;
                return (pdcrt_float) o.ival;
            }
        }
    }
    else if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_FLOAT)
    {
        *ok = true;
        return o.fval;
    }

    return 0.0;
}

bool pdcrt_obtener_booleano(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_BOOLEANO)
    {
        *ok = true;
        return o.bval;
    }
    else
    {
        *ok = false;
        return false;
    }
}

size_t pdcrt_obtener_tam_texto(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_TEXTO)
    {
        *ok = true;
        return o.texto->longitud;
    }
    else
    {
        *ok = false;
        return 0;
    }
}

bool pdcrt_obtener_texto(pdcrt_ctx *ctx, pdcrt_stp i, char *buffer, size_t tam_buffer)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_TEXTO)
    {
        if(o.texto->longitud > tam_buffer)
            return false;
        if(o.texto->longitud > 0)
            memcpy(buffer, o.texto->contenido, o.texto->longitud);
        if(o.texto->longitud < tam_buffer)
            memset(buffer + o.texto->longitud, '\0', tam_buffer - o.texto->longitud);
        return true;
    }
    else
    {
        return false;
    }
}

void* pdcrt_obtener_valop(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_VALOP)
    {
        *ok = true;
        return o.valop->datos;
    }
    else
    {
        *ok = false;
        return NULL;
    }
}

void pdcrt_caja_fijar(pdcrt_ctx *ctx, pdcrt_stp caja)
{
    size_t pos = pdcrt_stp_a_pos(ctx, caja);
    pdcrt_fijar_caja(ctx, ctx->pila[pos], pdcrt_sacar(ctx));
}

void pdcrt_caja_obtener(pdcrt_ctx *ctx, pdcrt_stp caja)
{
    size_t pos = pdcrt_stp_a_pos(ctx, caja);
    pdcrt_obj o = pdcrt_obtener_caja(ctx, ctx->pila[pos]);
    pdcrt_empujar(ctx, o);
}

void pdcrt_envolver_en_caja(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    pdcrt_obj c = pdcrt_objeto_caja(pdcrt_crear_caja(ctx, m, pdcrt_cima(ctx)));
    pdcrt_fijar_pila(ctx, ctx->tam_pila - 1, c);
}

void pdcrt_arreglo_en(pdcrt_ctx *ctx, pdcrt_stp arr, pdcrt_entero i)
{
    size_t pos = pdcrt_stp_a_pos(ctx, arr);
    pdcrt_obj oarr = ctx->pila[pos];
    pdcrt_debe_tener_tipo(ctx, oarr, PDCRT_TOBJ_ARREGLO);
    if(i < 0 || (size_t) i >= oarr.arreglo->longitud)
        pdcrt_error(ctx, u8"Índice fuera de rango al acceder al arreglo");
    pdcrt_empujar(ctx, oarr.arreglo->valores[i]);
}

void pdcrt_arreglo_empujar_al_final(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_stp arr)
{
    size_t pos = pdcrt_stp_a_pos(ctx, arr);
    pdcrt_obj oarr = ctx->pila[pos];
    pdcrt_debe_tener_tipo(ctx, oarr, PDCRT_TOBJ_ARREGLO);
    pdcrt_arreglo_abrir_espacio(ctx, m, oarr.arreglo, 1);
    pdcrt_obj val = pdcrt_sacar(ctx);
    pdcrt_barrera_de_escritura(ctx, oarr, val);
    oarr.arreglo->valores[oarr.arreglo->longitud] = val;
    oarr.arreglo->longitud += 1;
}

void pdcrt_negar(pdcrt_ctx *ctx)
{
    pdcrt_obj cima = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(cima) != PDCRT_TOBJ_BOOLEANO)
        pdcrt_error(ctx, "Valor no booleano pasado al operador `no`");
    (void) pdcrt_sacar(ctx);
    pdcrt_empujar(ctx, pdcrt_objeto_booleano(!cima.bval));
}

void pdcrt_eliminar_elemento(pdcrt_ctx *ctx, pdcrt_stp pos)
{
    size_t rpos = pdcrt_stp_a_pos(ctx, pos);
    size_t n = (ctx->tam_pila - rpos) - 1;
    if(n > 0)
        memmove(ctx->pila + rpos, ctx->pila + rpos + 1, n * sizeof(pdcrt_obj));
    ctx->tam_pila -= 1;
}

void pdcrt_eliminar_elementos(pdcrt_ctx *ctx, pdcrt_stp inic, size_t cnt)
{
    for(; cnt > 0; cnt--)
    {
        pdcrt_eliminar_elemento(ctx, inic);
    }
}

void pdcrt_duplicar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_stp i)
{
    pdcrt_extender_pila(ctx, m, 1);
    size_t pos = pdcrt_stp_a_pos(ctx, i);
    pdcrt_fijar_pila(ctx, ctx->tam_pila++, ctx->pila[pos]);
}

void pdcrt_extraer(pdcrt_ctx *ctx, pdcrt_stp i)
{
    size_t pos = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[pos];
    pdcrt_eliminar_elemento(ctx, i);
    pdcrt_fijar_pila(ctx, ctx->tam_pila++, o);
}

void pdcrt_insertar(pdcrt_ctx *ctx, pdcrt_stp pos)
{
    size_t rpos = pdcrt_stp_a_pos(ctx, pos);
    pdcrt_obj o = pdcrt_cima(ctx);
    for(size_t i = ctx->tam_pila - 1; i > rpos; i--)
    {
        pdcrt_fijar_pila(ctx, i, ctx->pila[i - 1]);
    }
    pdcrt_fijar_pila(ctx, rpos, o);
}

void pdcrt_mover_a_cima(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_stp pos, size_t num_els)
{
    pdcrt_extender_pila(ctx, m, num_els);
    size_t rpos = pdcrt_stp_a_pos(ctx, pos);
    size_t npost = ctx->tam_pila - (rpos + num_els);
    // [...pre, ...#num_els, ...post]
    memcpy(ctx->pila + ctx->tam_pila, ctx->pila + rpos, num_els * sizeof(pdcrt_obj));
    // [...pre, ...#num_els, ...post, ...#num_els]
    memmove(ctx->pila + rpos, ctx->pila + rpos + num_els, (npost + num_els) * sizeof(pdcrt_obj));
    // [...pre, ...post, ...#num_els, ...basura]
}


pdcrt_k pdcrt_params(pdcrt_ctx *ctx,
                     pdcrt_marco *m,
                     int params,
                     bool variadic,
                     pdcrt_kf kf)
{
    for(size_t i = 0; i < m->num_locales; i++)
    {
        pdcrt_fijar_local(ctx, m, i, pdcrt_objeto_nulo());
    }

    if(m->args < params)
        pdcrt_error(ctx, u8"función llamada de forma inválida");
    if(variadic)
    {
        int varargs = m->args - params;
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar_arreglo_vacio(ctx, &m, varargs);
        for(int i = 0; i < varargs; i++)
        {
            pdcrt_obj val = ctx->pila[(ctx->tam_pila - varargs - 1) + i];
            pdcrt_empujar(ctx, val);
            pdcrt_arreglo_empujar_al_final(ctx, m, -2);
        }
        pdcrt_obj var = pdcrt_sacar(ctx);
        for(int i = 0; i < varargs; i++)
            (void) pdcrt_sacar(ctx);
        pdcrt_empujar(ctx, var);
    }
    return (*kf)(ctx, m);
}

void pdcrt_variadic(pdcrt_ctx *ctx, pdcrt_marco *m, int params)
{
    (void) ctx;
    (void) m;
    (void) params;
    // Nada que hacer por ahora
}

bool pdcrt_saltar_condicional(pdcrt_ctx *ctx)
{
    pdcrt_obj o = pdcrt_sacar(ctx);
    if(pdcrt_tipo_de_obj(o) != PDCRT_TOBJ_BOOLEANO)
    {
        pdcrt_error(ctx, "Se esperaba un objeto de tipo Booleano");
    }
    return o.bval;
}

pdcrt_k pdcrt_saltar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf)
{
    return (*kf)(ctx, m);
}

pdcrt_k pdcrt_enviar_mensaje(pdcrt_ctx *ctx, pdcrt_marco *m,
                             const char* msj, size_t tam_msj,
                             const int* proto, size_t nproto,
                             pdcrt_kf kf)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_obj msj_o = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, msj, tam_msj));
    for(size_t i = 0; i < nproto; i++)
    {
        pdcrt_fijar_pila(ctx, ctx->tam_pila - i, ctx->pila[(ctx->tam_pila - i) - 1]);
    }
    pdcrt_fijar_pila(ctx, (ctx->tam_pila++) - nproto, msj_o);
    return pdcrt_enviar_mensaje_obj(ctx, m, proto, nproto, kf);
}


pdcrt_k pdcrt_enviar_mensaje_obj(pdcrt_ctx *ctx, pdcrt_marco *m,
                                 const int* proto, size_t nproto,
                                 pdcrt_kf kf)
{
    pdcrt_k k = {
        .kf = kf,
        .marco = m,
    };

    // Posición del inicio de los argumentos
    size_t base = ctx->tam_pila - nproto;

    size_t nargs_expandidos = 0, num_args = 0, variadic_args = 0;
    for(size_t i = 0; i < nproto; i++)
    {
        if(proto && proto[i] == 1)
        {
            pdcrt_obj arg = ctx->pila[base + i];
            pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_ARREGLO);
            nargs_expandidos += arg.arreglo->longitud;
            variadic_args += 1;
        }
        else
        {
            num_args += 1;
        }
    }

    pdcrt_extender_pila(ctx, m, nargs_expandidos + variadic_args);

    if(proto)
    {
        size_t cur = 0;
        for(size_t i = 0; i < nproto; i++)
        {
            size_t argpos = base + cur;
            pdcrt_obj arg = ctx->pila[argpos];
            if(proto[i] == 1)
            {
                // Validado arriba:
                // pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_ARREGLO);
                size_t len = arg.arreglo->longitud;
                size_t por_mover = nproto - (i + 1);
                memmove(ctx->pila + argpos + len, ctx->pila + argpos + 1, por_mover * sizeof(pdcrt_obj));
                num_args += len;
                for(size_t j = 0; j < len; j++)
                {
                    pdcrt_fijar_pila(ctx, argpos + j, arg.arreglo->valores[j]);
                }
                cur += len;
            }
            else
            {
                cur += 1;
            }
        }
    }

    ctx->tam_pila -= variadic_args;
    ctx->tam_pila += nargs_expandidos;
    pdcrt_obj t = ctx->pila[(ctx->tam_pila - num_args) - 2];
    return (*t.recv)(ctx, num_args, k);
}

static void pdcrt_prn_helper(pdcrt_ctx *ctx, pdcrt_obj o)
{
    switch(pdcrt_tipo_de_obj(o))
    {
    case PDCRT_TOBJ_NULO:
        printf("NULO");
        break;
    case PDCRT_TOBJ_BOOLEANO:
        printf("%s", o.bval? "VERDADERO" : "FALSO");
        break;
    case PDCRT_TOBJ_ENTERO:
        printf("%" PDCRT_ENTERO_PRId, o.ival);
        break;
    case PDCRT_TOBJ_FLOAT:
        printf("%f", (double) o.fval);
        break;
    case PDCRT_TOBJ_TEXTO:
        for(size_t i = 0; i < o.texto->longitud; i++)
        {
            putchar(o.texto->contenido[i]);
        }
        break;
    case PDCRT_TOBJ_ARREGLO:
        printf("(Arreglo#crearCon: ");
        for(size_t i = 0; i < o.arreglo->longitud; i++)
        {
            if(i > 0)
                printf(", ");
            pdcrt_prn_helper(ctx, o.arreglo->valores[i]);
        }
        printf(")");
        break;
    case PDCRT_TOBJ_MARCO:
        printf("Marco %p", (void *) o.marco);
        break;
    case PDCRT_TOBJ_RUNTIME:
        printf("Runtime");
        break;
    case PDCRT_TOBJ_CLOSURE:
        printf("Procedimiento %p", (void *) o.closure);
        break;
    case PDCRT_TOBJ_CAJA:
        printf("Caja %p", (void *) o.caja);
        break;
    case PDCRT_TOBJ_TABLA:
        printf("Tabla %p", (void *) o.tabla);
        break;
    case PDCRT_TOBJ_VOIDPTR:
        printf("Voidptr %p", (void *) o.pval);
        break;
    case PDCRT_TOBJ_VALOP:
        printf("Valop %p", (void *) o.valop);
        break;
    case PDCRT_TOBJ_ESPACIO_DE_NOMBRES:
        printf("(EspacioDeNombres en %p)", (void *) o.tabla);
        break;
    case PDCRT_TOBJ_CORRUTINA:
        printf("(Corrutina en %p)", (void *) o.coro);
        break;
    case PDCRT_TOBJ_INSTANCIA:
        printf("(Objeto en %p)", (void *) o.inst);
        break;
    case PDCRT_TOBJ_REUBICADO:
        printf("(Objeto reubicado en %p)", (void *) o.reubicado);
        break;
    }
}

pdcrt_k pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf)
{
    pdcrt_obj o = pdcrt_sacar(ctx);
    pdcrt_prn_helper(ctx, o);
    return (*kf)(ctx, m);
}

void pdcrt_prnl(pdcrt_ctx *ctx)
{
    (void) ctx;
    puts("");
}

pdcrt_k pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets)
{
    (void) ctx;
    assert(rets == 1);
    return pdcrt_continuar(ctx, m->k);
}


static pdcrt_k pdcrt_importar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_importar_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_importar_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_importar_k4(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_importar_k5(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_importar(pdcrt_ctx *ctx, pdcrt_marco *m, const char *nombre, size_t tam_nombre, pdcrt_kf kf)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    pdcrt_extender_pila(ctx, m, 4);
    pdcrt_kf *pkf = pdcrt_empujar_valop(ctx, &m, sizeof(kf));
    *pkf = kf;
    // [valop]
    pdcrt_empujar_texto(ctx, &m, nombre, tam_nombre);
    // [valop, nom]
    pdcrt_empujar(ctx, ctx->registro_de_espacios_de_nombres);
    // [valop, nom, reg]
    pdcrt_empujar_texto(ctx, &m, nombre, tam_nombre);
    // [valop, nom, reg, nom]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "contiene", 8, proto, 1, &pdcrt_importar_k1);
}

static pdcrt_k pdcrt_importar_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_importar_k1);
    // [valop, nom, boole]
    bool ok = false, res = false;
    res = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Error interno: Tabla#contiene debe devolver un boole");
    (void) pdcrt_sacar(ctx);
    if(res)
    {
        // [valop, nom]
        pdcrt_empujar(ctx, ctx->registro_de_espacios_de_nombres);
        // [valop, nom, reg]
        pdcrt_duplicar(ctx, m, -2);
        // [valop, nom, reg, nom]
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_importar_k2);
    }
    else
    {
        pdcrt_extender_pila(ctx, m, 4);
        // [valop, nom]
        pdcrt_empujar(ctx, ctx->registro_de_modulos);
        // [valop, nom, mods]
        pdcrt_duplicar(ctx, m, -2);
        // [valop, nom, mods, nom]
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_importar_k3);
    }
}

static pdcrt_k pdcrt_importar_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_importar_k2);
    // [valop, nom, ns]
    pdcrt_obj nom = ctx->pila[pdcrt_stp_a_pos(ctx, -2)];
    pdcrt_insertar(ctx, -3);
    // [ns, valop, nom]
    bool ok;
    pdcrt_kf *pkf = pdcrt_obtener_valop(ctx, -2, &ok);
    if(!ok)
        pdcrt_error(ctx, "Error interno: valop esperado");
    pdcrt_kf kf = *pkf;
    (void) pdcrt_sacar(ctx);
    // [ns, valop]
    (void) pdcrt_sacar(ctx);
    // [ns]
    return (*kf)(ctx, m);
}

static pdcrt_k pdcrt_importar_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_importar_k3);
    // [valop, nom, mfunc]
    pdcrt_obj nom = ctx->pila[pdcrt_stp_a_pos(ctx, -2)];
    ctx->cnt += 1;
    return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, NULL, 0, &pdcrt_importar_k4);
}

static pdcrt_k pdcrt_importar_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_importar_k4);
    ctx->cnt -= 1;
    // [valop, nom, NULO]
    (void) pdcrt_sacar(ctx);
    // [valop, nom]
    pdcrt_extender_pila(ctx, m, 2);
    pdcrt_empujar(ctx, ctx->registro_de_espacios_de_nombres);
    // [valop, nom, reg]
    pdcrt_insertar(ctx, -2);
    // [valop, reg, nom]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_importar_k5);
}

static pdcrt_k pdcrt_importar_k5(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_importar_k5);
    // [valop, res]
    pdcrt_insertar(ctx, -2);
    // [mod, valop]
    bool ok;
    pdcrt_kf *pkf = pdcrt_obtener_valop(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Error interno: valop esperado");
    pdcrt_kf kf = *pkf;
    (void) pdcrt_sacar(ctx);
    // [mod]
    return (*kf)(ctx, m);
}

pdcrt_k pdcrt_extraerv_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
pdcrt_k pdcrt_extraerv_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_extraerv(pdcrt_ctx *ctx, pdcrt_marco *m, const char *nombre, size_t tam_nombre, pdcrt_kf kf)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    // [mod]
    pdcrt_kf *valop = pdcrt_empujar_valop(ctx, &m, sizeof(pdcrt_kf));
    *valop = kf;
    // [mod, valop]
    pdcrt_duplicar(ctx, m, -2);
    // [mod, valop, mod]
    pdcrt_empujar_texto(ctx, &m, nombre, tam_nombre);
    // [mod, valop, mod, nom]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_extraerv_k1);
}

pdcrt_k pdcrt_extraerv_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_extraerv_k1);
    // [mod, valop, arr]
    pdcrt_empujar_entero(ctx, m, 0);
    // [mod, valop, arr, 0]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_extraerv_k2);
}

pdcrt_k pdcrt_extraerv_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_extraerv_k2);
    // [mod, valop, val]
    pdcrt_extraer(ctx, -2);
    // [mod, val, valop]
    bool ok;
    pdcrt_kf *kf = pdcrt_obtener_valop(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "error interno: valop esperado");
    (void) pdcrt_sacar(ctx);
    return (*kf)(ctx, m);
}

static pdcrt_k pdcrt_agregar_nombre_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_agregar_nombre(pdcrt_ctx *ctx, pdcrt_marco *m, const char *nombre, size_t tam_nombre, bool autoejec, pdcrt_kf kf)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    pdcrt_extender_pila(ctx, m, 5);
    // [mod, valor]
    pdcrt_obj val = ctx->pila[ctx->tam_pila - 2];
    pdcrt_empujar(ctx, val);
    // [mod, valor, mod]
    pdcrt_insertar(ctx, -2);
    // [mod, mod, valor]
    pdcrt_empujar_arreglo_vacio(ctx, &m, 2);
    // [mod, mod, valor, arr]
    pdcrt_insertar(ctx, -2);
    // [mod, mod, arr, valor]
    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    // [mod, mod, arr]
    pdcrt_empujar_booleano(ctx, m, autoejec);
    // [mod, mod, arr, auto]
    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    // [mod, mod, arr]
    pdcrt_empujar_texto(ctx, &m, nombre, tam_nombre);
    // [mod, mod, arr, nom]
    pdcrt_insertar(ctx, -2);
    // [mod, mod, nom, arr]
    pdcrt_kf *pkf = pdcrt_empujar_valop(ctx, &m, sizeof(kf));
    // [mod, mod, nom, arr, valop]
    *pkf = kf;
    pdcrt_insertar(ctx, -4);
    // [mod, valop, mod, nom, arr]
    static const int proto[] = {0, 0};
    return pdcrt_enviar_mensaje(ctx, m, "fijarEn", 7, proto, 2, &pdcrt_agregar_nombre_k1);
}

static pdcrt_k pdcrt_agregar_nombre_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_agregar_nombre_k1);
    // [mod, valop, res]
    (void) pdcrt_sacar(ctx);
    // [mod, valop]
    bool ok;
    pdcrt_kf *pkf = pdcrt_obtener_valop(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Error interno: valop esperado");
    pdcrt_kf kf = *pkf;
    (void) pdcrt_sacar(ctx);
    // [mod]
    return (*kf)(ctx, m);
}

static pdcrt_k pdcrt_exportar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_exportar(pdcrt_ctx *ctx, pdcrt_marco *m, const char *modulo, size_t tam_modulo)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    pdcrt_extender_pila(ctx, m, 3);
    // [mod]
    pdcrt_empujar(ctx, ctx->registro_de_espacios_de_nombres);
    // [mod, reg]
    pdcrt_insertar(ctx, -2);
    // [reg, mod]
    pdcrt_empujar_texto(ctx, &m, modulo, tam_modulo);
    // [reg, mod, nom]
    pdcrt_insertar(ctx, -2);
    // [reg, nom, mod]
    static const int proto[] = {0, 0};
    return pdcrt_enviar_mensaje(ctx, m, "fijarEn", 7, proto, 2, &pdcrt_exportar_k1);
}

static pdcrt_k pdcrt_exportar_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_exportar_k1);
    // [NULO]
    return pdcrt_devolver(ctx, m, 1);
}

void pdcrt_obtener_clase_objeto(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, ctx->clase_objeto);
}

void pdcrt_empujar_closure(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_f f, size_t num_caps)
{
    pdcrt_closure *c = pdcrt_crear_closure(ctx, m, f, num_caps);
    for(size_t i = 0; i < num_caps; i++)
    {
        size_t ci = (num_caps - 1) - i;
        pdcrt_obj cap = pdcrt_sacar(ctx);
        pdcrt_barrera_de_escritura(ctx, pdcrt_objeto_closure(c), cap);
        c->capturas[ci] = cap;
    }
    pdcrt_empujar(ctx, pdcrt_objeto_closure(c));
}

void pdcrt_assert(pdcrt_ctx *ctx)
{
    pdcrt_obj b = pdcrt_sacar(ctx);
    pdcrt_debe_tener_tipo(ctx, b, PDCRT_TOBJ_BOOLEANO);
    if(!b.bval)
    {
        pdcrt_error(ctx, "necesitas fallido");
    }
}

void pdcrt_son_identicos(pdcrt_ctx *ctx)
{
    pdcrt_obj a = pdcrt_sacar(ctx);
    pdcrt_obj b = pdcrt_sacar(ctx);
    if(pdcrt_tipo_de_obj(a) != pdcrt_tipo_de_obj(b))
    {
        pdcrt_empujar(ctx, pdcrt_objeto_booleano(false));
    }
    else
    {
        // HACK: Debería haber una forma de comparar los "datos" de dos
        // objetos, pero como no hay comparamos en cambio sus enteros.
        pdcrt_empujar(ctx, pdcrt_objeto_booleano(a.ival == b.ival));
    }
}

void pdcrt_obtener_espacio_de_nombres_del_runtime(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, ctx->espacio_de_nombres_runtime);
}

void pdcrt_arreglo_abrir_espacio(pdcrt_ctx *ctx,
                                 pdcrt_marco *m,
                                 pdcrt_arreglo *arr,
                                 size_t espacio)
{
    // TODO: Haz que use el GC si no hay memoria
    (void) m;

    if((arr->longitud + espacio) <= arr->capacidad)
        return;
    size_t nueva_cap = arr->longitud + espacio;
    if(nueva_cap == 0)
        return;
    pdcrt_obj *nuevos_vals = pdcrt_realojar_ctx(ctx, arr->valores,
                                                sizeof(pdcrt_obj) * arr->capacidad,
                                                sizeof(pdcrt_obj) * nueva_cap);
    if(!nuevos_vals)
        pdcrt_enomem(ctx);
    arr->valores = nuevos_vals;
    arr->capacidad = nueva_cap;
}

bool pdcrt_stack_lleno(pdcrt_ctx *ctx)
{
    uintptr_t xp = pdcrt_obtener_stack_pointer();
    size_t diff;
    if(xp > ctx->inicio_del_stack)
        diff = xp - ctx->inicio_del_stack;
    else
        diff = ctx->inicio_del_stack - xp;
    return diff > ctx->tam_stack;
}

static pdcrt_k pdcrt_continuacion_de_ejecutar(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    if(!ctx->hay_una_salida_del_trampolin)
    {
        // TODO: Documenta este mensaje de error.
        pdcrt_error(ctx, u8"Debe haber una salida del trampolín para terminar la ejecución");
    }
    // Si el marco es reubicado, la nueva dirección quedará en `m`, que será
    // inaccesible después de que `longjmp` devuelva.
    //
    // Esto normalmente sería problematico, pero aquí no importa ya que nada
    // usará el marco después de esta llamada.
    pdcrt_recolectar_basura_por_pila(ctx, &m);
    longjmp(ctx->salir_del_trampolin, 1);
}

static bool pdcrt_ejecutar_opt(pdcrt_ctx *ctxp, int args, pdcrt_f f, bool protegido)
{
    pdcrt_ctx * volatile ctx = ctxp;
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 0, 0, 0, (pdcrt_k){0});
    pdcrt_k k = {
        .kf = &pdcrt_continuacion_de_ejecutar,
        .marco = m,
    };
    volatile size_t tam_pila = ctx->tam_pila;
    volatile jmp_buf viejo_s;
    volatile jmp_buf viejo_e;
    memcpy((jmp_buf *) &viejo_s, &ctx->salir_del_trampolin, sizeof(jmp_buf));
    memcpy((jmp_buf *) &viejo_e, &ctx->manejador_de_errores, sizeof(jmp_buf));
    volatile bool habia_s = ctx->hay_una_salida_del_trampolin;
    volatile bool habia_e = ctx->hay_un_manejador_de_errores;
    volatile uintptr_t viejo_sp = ctx->inicio_del_stack;

    if(ctx->inicio_del_stack == 0)
    {
        ctx->inicio_del_stack = pdcrt_obtener_stack_pointer();
        if(ctx->inicio_del_stack == 0)
            ctx->inicio_del_stack = 1;
    }

#define CLEANUP()                                                       \
    do                                                                  \
    {                                                                   \
        memcpy(&ctx->salir_del_trampolin, (jmp_buf *) &viejo_s, sizeof(jmp_buf)); \
        memcpy(&ctx->manejador_de_errores, (jmp_buf *) &viejo_e, sizeof(jmp_buf)); \
        ctx->hay_una_salida_del_trampolin = habia_s;                    \
        ctx->hay_un_manejador_de_errores = habia_e;                     \
        ctx->inicio_del_stack = viejo_sp;                               \
    }                                                                   \
    while(0)

    if(setjmp(ctx->salir_del_trampolin) > 0)
    {
        CLEANUP();
        return true;
    }
    if(protegido)
    {
        if(setjmp(ctx->manejador_de_errores) > 0)
        {
            CLEANUP();
            ctx->tam_pila = tam_pila; // Saca los valores intermediarios
            return false;
        }

        ctx->hay_un_manejador_de_errores = true;
    }

    ctx->hay_una_salida_del_trampolin = true;
    ctx->continuacion_actual = f(ctx, args, k);
    do
    {
        ctx->continuacion_actual = (*ctx->continuacion_actual.kf)(ctx, ctx->continuacion_actual.marco);
    }
    while(1);
}

void pdcrt_ejecutar(pdcrt_ctx *ctx, int args, pdcrt_f f)
{
    pdcrt_ejecutar_opt(ctx, args, f, false);
}

bool pdcrt_ejecutar_protegido(pdcrt_ctx *ctx, int args, pdcrt_f f)
{
    return pdcrt_ejecutar_opt(ctx, args, f, true);
}

static pdcrt_k pdcrt_preparar_registro_de_modulos_importar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_preparar_registro_de_modulos_importar(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 0, 0, args, k);
    return pdcrt_importar(ctx, m, "pdcrt_N95_runtime", 17, &pdcrt_preparar_registro_de_modulos_importar_k1);
}

static pdcrt_k pdcrt_preparar_registro_de_modulos_importar_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_preparar_registro_de_modulos_importar_k1);
    pdcrt_convertir_a_espacio_de_nombres(ctx, m);
    ctx->espacio_de_nombres_runtime = pdcrt_sacar(ctx);
    pdcrt_empujar_nulo(ctx, m);
    return pdcrt_devolver(ctx, m, 1);
}

void pdcrt_preparar_registro_de_modulos(pdcrt_ctx *ctx, size_t num_mods)
{
    pdcrt_preparar_registros(ctx, num_mods + 1);
    pdcrt_cargar_dependencia(ctx, &pdc_instalar_pdcrt_N95_runtime, "pdcrt_N95_runtime", 17);
    pdcrt_ejecutar(ctx, 0, &pdcrt_preparar_registro_de_modulos_importar);
    (void) pdcrt_sacar(ctx);
}

static pdcrt_k pdcrt_cargar_dependencia_fijarEn_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_cargar_dependencia_fijarEn(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 0, 0, args, k);
    static const int proto[] = {0, 0};
    return pdcrt_enviar_mensaje(ctx, m, "fijarEn", 7, proto, 2, &pdcrt_cargar_dependencia_fijarEn_k1);
}

static pdcrt_k pdcrt_cargar_dependencia_fijarEn_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_cargar_dependencia_fijarEn_k1);
    pdcrt_empujar_nulo(ctx, m);
    return pdcrt_devolver(ctx, m, 1);
}

void pdcrt_cargar_dependencia(pdcrt_ctx *ctx, pdcrt_f fmod, const char *nombre, size_t tam_nombre)
{
    pdcrt_marco *m = NULL;
    pdcrt_extender_pila(ctx, NULL, 3);
    pdcrt_empujar(ctx, ctx->registro_de_modulos);
    pdcrt_empujar_texto(ctx, &m, nombre, tam_nombre);
    pdcrt_empujar_closure(ctx, &m, fmod, 0);
    pdcrt_ejecutar(ctx, 0, pdcrt_cargar_dependencia_fijarEn);
    (void) pdcrt_sacar(ctx);
}

int pdcrt_main(int argc, char **argv, void (*cargar_deps)(pdcrt_ctx *ctx), pdcrt_f f)
{
    pdcrt_aloj *aloj_malloc = pdcrt_alojador_malloc();
    pdcrt_ctx *ctx = pdcrt_crear_contexto(aloj_malloc);
    pdcrt_fijar_argv(ctx, argc, argv);
    cargar_deps(ctx);
    pdcrt_ejecutar(ctx, 0, f);
    pdcrt_cerrar_contexto(ctx);
    return 0;
}

void pdcrt_inspeccionar_pila(pdcrt_ctx *ctx)
{
    printf("PILA %zu %zu\n", ctx->tam_pila, ctx->cap_pila);
    for(size_t i = 0; i < ctx->tam_pila; i++)
    {
        printf("  ");
        pdcrt_obj o = ctx->pila[i];
        switch(pdcrt_tipo_de_obj(o))
        {
        case PDCRT_TOBJ_BOOLEANO:
            printf("booleano %d\n", o.bval);
            break;
        case PDCRT_TOBJ_ENTERO:
            printf("entero %" PDCRT_ENTERO_PRId "\n", o.ival);
            break;
        case PDCRT_TOBJ_FLOAT:
            printf("float %f\n", (double) o.fval);
            break;
        case PDCRT_TOBJ_NULO:
            printf("nulo\n");
            break;
        case PDCRT_TOBJ_TEXTO:
            printf("texto <");
            for(size_t i = 0; i < o.texto->longitud; i++)
            {
                putchar(o.texto->contenido[i]);
            }
            printf(">\n");
            break;
        case PDCRT_TOBJ_ARREGLO:
            printf("arreglo %p\n", o.arreglo);
            break;
        case PDCRT_TOBJ_CLOSURE:
            printf("función %p\n", o.closure);
            break;
        case PDCRT_TOBJ_CAJA:
            printf("caja %p\n", o.caja);
            break;
        case PDCRT_TOBJ_TABLA:
            printf("tabla %p\n", o.tabla);
            break;
        case PDCRT_TOBJ_VOIDPTR:
            printf("void* %p\n", o.pval);
            break;
        case PDCRT_TOBJ_VALOP:
            printf("valop %p\n", o.valop);
            break;
        case PDCRT_TOBJ_CORRUTINA:
            printf("corrutina %p\n", o.coro);
            break;
        case PDCRT_TOBJ_INSTANCIA:
            printf("instancia %p\n", o.inst);
            break;
        case PDCRT_TOBJ_REUBICADO:
            printf("reubicado %p\n", o.reubicado);
            break;
        default:
            printf("unk %d\n", pdcrt_tipo_de_obj(o));
            break;
        }
    }
}

void pdcrt_inspeccionar_texto(pdcrt_texto *txt)
{
    for(size_t i = 0; i < txt->longitud; i++)
    {
        putchar(txt->contenido[i]);
    }
    putchar('\n');
}
