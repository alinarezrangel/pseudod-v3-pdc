//
// Created by alinarezrangel on 29/10/25.
//

#ifndef PDCRT_BASE_H
#define PDCRT_BASE_H

#include <string.h>
#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include "pdcrt-plataforma.h"


#ifdef __has_attribute
#  if __has_attribute(always_inline)
#    define PDCRT_INLINE __attribute__((always_inline)) inline
#  else
#    define PDCRT_INLINE inline
#  endif
#  if __has_attribute(noinline)
#    define PDCRT_NOINLINE __attribute__((noinline))
#  else
#    define PDCRT_NOINLINE
#  endif
#else
#  define PDCRT_INLINE inline
#  define PDCRT_NOINLINE
#endif

struct pdcrt_ctx;
typedef struct pdcrt_ctx pdcrt_ctx;

typedef struct pdcrt_aloj_exts
{
    uint32_t version;
    uint32_t tam_bytes;
    char resto[];
} pdcrt_aloj_exts;


typedef struct pdcrt_aloj
{
    void *(*alojar)(void *yo, size_t bytes);
    void *(*realojar)(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo);
    void (*desalojar)(void *yo, void *ptr, size_t tam_actual);
    pdcrt_aloj_exts *(*obtener_extensiones)(void *yo);
} pdcrt_aloj;

void *pdcrt_alojar(pdcrt_aloj *aloj, size_t bytes);
void *pdcrt_realojar(pdcrt_aloj *aloj, void *ptr, size_t tam_actual, size_t tam_nuevo);
void pdcrt_desalojar(pdcrt_aloj *aloj, void *ptr, size_t tam_actual);

pdcrt_aloj* pdcrt_alojador_malloc(void);

pdcrt_aloj* pdcrt_alojador_con_estadisticas(pdcrt_aloj* base);
size_t pdcrt_alojador_con_estadisticas_obtener_usado(pdcrt_aloj* yo);
void pdcrt_desalojar_alojador_con_estadisticas(pdcrt_aloj* yo);


#if defined(__has_builtin)
#  if __has_builtin(__builtin_popcountll)
#    define PDCRT_POPCOUNTLL(x) __builtin_popcountll(x)
#  else
#    error popcount needed
#  endif
#  if __has_builtin(__builtin_clzll)
#    define PDCRT_CLZLL(x) __builtin_clzll(x)
#  else
#    error clz needed
#  endif
#else
#  error builtins needed
#endif

inline uintptr_t pdcrt_obtener_stack_pointer(void)
{
    // TODO: Portar a distíntas arquitecturas
    uintptr_t sp;
    asm inline("mov %%rsp, %0\n"
        : "=r" (sp));
    return sp;
}

inline size_t pdcrt_redondear_a_p2(size_t n)
{
    // Devuelve un entero N tal que N = 1 << x
    // y N es el menor entero tal que N >= n.
    if(n <= 1)
    {
        return 1;
    }
    else
    {
        return 1 << ((sizeof(n) * 8) - PDCRT_CLZLL(n - 1));
    }
}


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


typedef enum pdcrt_subsistema
{
    PDCRT_SUBSISTEMA_GC,
} pdcrt_subsistema;

#if !PDCRT_LOG_COMPILADO
// Si no hay ningún logger activo, la función no hace nada. En ese caso,
// siempre la inlineamos para que el compilador elimine las
// llamadas. Efectivamente, esto "borra" la función del programa.
PDCRT_INLINE
PDCRT_PRINTF_FORMAT(3, 4)
void pdcrt_log(pdcrt_ctx *ctx, pdcrt_subsistema sis, const char *fmt, ...)
{
    (void) ctx;
    (void) sis;
    (void) fmt;
}
#else
PDCRT_PRINTF_FORMAT(3, 4)
void pdcrt_log(pdcrt_ctx *ctx, pdcrt_subsistema sis, const char *fmt, ...);
#endif


#define PDCRT_BUG(cond, msg) do { if(cond) { \
    fprintf(stderr, "%s:%d: BUG: %s\n", __FILE__, __LINE__, msg); \
    abort(); \
} } while(0)

#define PDCRT_BUG_SI_FUERA_DE_MASCARA(mask, val, msg) \
    PDCRT_BUG(((~(mask)) & (val)) != 0, msg)

#endif //PDCRT_BASE_H
