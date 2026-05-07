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
#  if __has_builtin(__builtin_unreachable)
#    define PDCRT_INALCANZABLE() __builtin_unreachable()
#  else
#    define PDCRT_INALCANZABLE() abort()
#  endif
#else
#  error builtins needed
#endif

PDCRT_INLINE uintptr_t pdcrt_obtener_stack_pointer(void)
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

#define PDCRT_BUG(cond, msg) do { if(cond) { \
    fprintf(stderr, "%s:%d: BUG: %s\n", __FILE__, __LINE__, msg); \
    abort(); \
} } while(0)

#define PDCRT_ASSERT(cond) do { if(!(cond)) { \
    fprintf(stderr, "%s:%d: BUG: %s\n", __FILE__, __LINE__, #cond); \
    abort(); \
} } while(0)

#define PDCRT_BUG_SI_FUERA_DE_MASCARA(mask, val, msg) \
    PDCRT_BUG(((~(mask)) & (val)) != 0, msg)

// La cantidad de bits en un entero.
#define PDCRT_ENTERO_BITS (sizeof(pdcrt_entero) * 8)

#endif //PDCRT_BASE_H
