#ifndef PDCRT_H
#define PDCRT_H

#include <setjmp.h>
#include <assert.h>
#include <string.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


typedef int64_t pdcrt_entero;
typedef double pdcrt_float;


struct pdcrt_ctx;
typedef struct pdcrt_ctx pdcrt_ctx;


struct pdcrt_marco;
typedef struct pdcrt_marco pdcrt_marco;

struct pdcrt_k;
typedef struct pdcrt_k pdcrt_k;

typedef struct pdcrt_k (*pdcrt_f)(pdcrt_ctx *, int args, struct pdcrt_k);
typedef struct pdcrt_k (*pdcrt_kf)(pdcrt_ctx *, pdcrt_marco *);

struct pdcrt_k
{
    pdcrt_kf kf;
    pdcrt_marco *marco;
};


typedef enum pdcrt_tipo_obj_gc
{
    PDCRT_TGC_MARCO,
    PDCRT_TGC_TEXTO,
} pdcrt_tipo_obj_gc;

typedef struct pdcrt_cabecera_gc
{
    struct pdcrt_cabecera_gc *siguiente, *anterior;
    int generacion;
    pdcrt_tipo_obj_gc tipo;
} pdcrt_cabecera_gc;

typedef struct pdcrt_gc
{
    pdcrt_cabecera_gc *primero, *ultimo;
    int generacion;
} pdcrt_gc;


typedef struct pdcrt_texto
{
    pdcrt_cabecera_gc gc;
    size_t longitud;
    char contenido[];
} pdcrt_texto;

typedef struct pdcrt_obj
{
    pdcrt_f recv;
    union
    {
        pdcrt_entero ival;
        pdcrt_float fval;
        bool bval;
        pdcrt_marco *marco;
        pdcrt_texto *texto;
    };
} pdcrt_obj;

typedef enum pdcrt_tipo
{
    PDCRT_TOBJ_ENTERO,
    PDCRT_TOBJ_FLOAT,
    PDCRT_TOBJ_BOOLEANO,
    PDCRT_TOBJ_MARCO,
    PDCRT_TOBJ_TEXTO,
    PDCRT_TOBJ_NULO,
} pdcrt_tipo;


struct pdcrt_marco
{
    pdcrt_cabecera_gc gc;
    bool activo;
    int args;
    pdcrt_k k;
    size_t num_locales;
    size_t num_capturas;
    pdcrt_obj locales_y_capturas[];
};

pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, size_t locales, size_t capturas, int args, pdcrt_k k);


struct pdcrt_ctx
{
    pdcrt_obj* pila;
    size_t tam_pila;
    size_t cap_pila;

    pdcrt_gc gc;
    pdcrt_k continuacion_actual;

    unsigned int cnt;

    pdcrt_texto **textos;
    size_t tam_textos;
    size_t cap_textos;

    uintptr_t inicio_del_stack;
    size_t tam_stack;

    const char* mensaje_de_error;
    jmp_buf manejador_de_errores;
    bool hay_un_manejador_de_errores;
    jmp_buf salir_del_trampolin;
    bool hay_una_salida_del_trampolin;
};

pdcrt_ctx *pdcrt_crear_contexto(void);
void pdcrt_cerrar_contexto(pdcrt_ctx *ctx);

void pdcrt_empujar_entero(pdcrt_ctx *ctx, pdcrt_entero i);
void pdcrt_empujar_float(pdcrt_ctx *ctx, pdcrt_float f);
void pdcrt_empujar_espacio_de_nombres(pdcrt_ctx *ctx);
void pdcrt_empujar_texto(pdcrt_ctx *ctx, const char *str, size_t len);
void pdcrt_empujar_nulo(pdcrt_ctx *ctx);

void pdcrt_ejecutar(pdcrt_ctx *ctx, int args, pdcrt_f f);
bool pdcrt_ejecutar_protegido(pdcrt_ctx *ctx, int args, pdcrt_f f);

#ifdef PDCRT_INTERNO

bool pdcrt_saltar_condicional(pdcrt_ctx *ctx);
pdcrt_k pdcrt_saltar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf);
pdcrt_k pdcrt_enviar_mensaje(pdcrt_ctx *ctx, pdcrt_marco *m,
                             const char* msj, size_t tam_msj,
                             const int* proto, size_t nproto,
                             pdcrt_kf kf);
pdcrt_k pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf);
void pdcrt_prnl(pdcrt_ctx *ctx);
pdcrt_k pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets);
pdcrt_k pdcrt_exportar(pdcrt_ctx *ctx, pdcrt_marco *m);

bool pdcrt_stack_lleno(pdcrt_ctx *ctx);

int pdcrt_main(int argc, char **argv, pdcrt_f f);

#define PDCRT_DECLARAR_ENTRYPOINT(mod, fmain)                       \
    pdcrt_k pdc_instalar_##mod(pdcrt_ctx *ctx, int args, pdcrt_k k) \
    {                                                               \
        return fmain(ctx, args, k);                                 \
    }

#define PDCRT_DECLARAR_MAIN(mod)                            \
    int main(int argc, char **argv)                         \
    {                                                       \
        return pdcrt_main(argc, argv, &pdc_instalar_##mod); \
    }

#endif  /* PDCRT_INTERNO */

#endif /* PDCRT_H */
