#ifndef PDCRT_H
#define PDCRT_H

#include <setjmp.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


typedef int64_t pdcrt_entero;

#define PDCRT_ENTERO_MAX INT64_MAX
#define PDCRT_ENTERO_MIN INT64_MIN
#define PDCRT_ENTERO_WIDTH INT64_WIDTH
#define PDCRT_ENTERO_C(n) INT64_C(n)

#define PDCRT_ENTERO_PRId PRId64
#define PDCRT_ENTERO_PRIi PRIi64
#define PDCRT_ENTERO_PRIu PRIu64
#define PDCRT_ENTERO_PRIo PRIo64
#define PDCRT_ENTERO_PRIx PRIx64
#define PDCRT_ENTERO_PRIX PRIX64

#define PDCRT_ENTERO_SCNd SCNd64
#define PDCRT_ENTERO_SCNi SCNi64
#define PDCRT_ENTERO_SCNu SCNu64
#define PDCRT_ENTERO_SCNo SCNo64
#define PDCRT_ENTERO_SCNx SCNx64

typedef double pdcrt_float;

#define PDCRT_FLOAT_C(n) n##L

#define PDCRT_FLOAT_DECIMAL_DIG DBL_DECIMAL_DIG
#define PDCRT_FLOAT_MIN DBL_MIN
#define PDCRT_FLOAT_MAX DBL_MAX
#define PDCRT_FLOAT_EPSILON DBL_EPSILON
#define PDCRT_FLOAT_DIG DBL_DIG
#define PDCRT_FLOAT_MANT_DIG DBL_MANT_DIG
#define PDCRT_FLOAT_MIN_EXP DBL_MIN_EXP
#define PDCRT_FLOAT_MIN_10_EXP DBL_MIN_10_EXP
#define PDCRT_FLOAT_MAX_EXP DBL_MAX_EXP
#define PDCRT_FLOAT_MAX_10_EXP DBL_MAX_10_EXP
#define PDCRT_FLOAT_HAS_SUBNORM DBL_HAS_SUBNORM

#define PDCRT_FLOAT_PRIf "f"
#define PDCRT_FLOAT_PRIF "F"
#define PDCRT_FLOAT_PRIg "g"
#define PDCRT_FLOAT_PRIG "G"
#define PDCRT_FLOAT_PRIa "a"
#define PDCRT_FLOAT_PRIA "A"

#define PDCRT_FLOAT_SCNf "f"

#define PDCRT_FLOAT_FLOOR floor
#define PDCRT_FLOAT_CEIL ceil
#define PDCRT_FLOAT_FREXP frexp


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
    pdcrt_marco *siguiente, *anterior;
    int args;
    pdcrt_k k;
    size_t num_locales;
    size_t num_capturas;
    pdcrt_obj locales_y_capturas[];
};

pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, size_t locales, size_t capturas, int args, pdcrt_k k);

#define PDCRT_TABLA_TEXTOS(X)                                \
    X(operador_mas, "operador_+")                            \
        X(operador_menos, "operador_-")                      \
        X(operador_por, "operador_*")                        \
        X(operador_entre, "operador_/")                      \
        X(operador_igual, "operador_=")                      \
        X(operador_distinto, "operador_no=")                 \
        X(sumar, "sumar")                                    \
        X(restar, "restar")                                  \
        X(multiplicar, "multiplicar")                        \
        X(dividir, "dividir")                                \
        X(igual, "igualA")                                   \
        X(distinto, "distíntoDe")                            \
    X(como_texto, "comoTexto")                               \
    X(concatenar, "concatenar")                              \
    X(operador_menor_que, "operador_<")                      \
    X(menor_que, "menorQue")                                 \
    X(operador_mayor_que, "operador_>")                      \
    X(mayor_que, "mayorQue")                                 \
    X(operador_menor_o_igual_a, "operador_=<")               \
    X(menor_o_igual_a, "menorOIgualA")                       \
        X(operador_mayor_o_igual_a, "operador_>=")           \
        X(mayor_o_igual_a, "mayorOIgualA")                   \
        X(verdadero, "VERDADERO")                            \
        X(falso, "FALSO")

typedef struct pdcrt_textos
{
#define PDCRT_X(nombre, _texto) pdcrt_texto *nombre;
    PDCRT_TABLA_TEXTOS(PDCRT_X)
#undef PDCRT_X
} pdcrt_textos;

struct pdcrt_ctx
{
    bool recolector_de_basura_activo;

    pdcrt_obj* pila;
    size_t tam_pila;
    size_t cap_pila;

    pdcrt_gc gc;
    pdcrt_k continuacion_actual;

    pdcrt_marco *primer_marco_activo;
    pdcrt_marco *ultimo_marco_activo;

    unsigned int cnt;

    pdcrt_texto **textos;
    size_t tam_textos;
    size_t cap_textos;
    pdcrt_textos textos_globales;

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

void pdcrt_extender_pila(pdcrt_ctx *ctx, size_t num_elem);

void pdcrt_empujar_entero(pdcrt_ctx *ctx, pdcrt_entero i);
void pdcrt_empujar_booleano(pdcrt_ctx *ctx, bool v);
void pdcrt_empujar_float(pdcrt_ctx *ctx, pdcrt_float f);
void pdcrt_empujar_espacio_de_nombres(pdcrt_ctx *ctx);
void pdcrt_empujar_texto(pdcrt_ctx *ctx, const char *str, size_t len);
void pdcrt_empujar_nulo(pdcrt_ctx *ctx);

void pdcrt_negar(pdcrt_ctx *ctx);

void pdcrt_eliminar_elemento(pdcrt_ctx *ctx, ssize_t pos);
void pdcrt_eliminar_elementos(pdcrt_ctx *ctx, ssize_t inic, size_t cnt);

void pdcrt_ejecutar(pdcrt_ctx *ctx, int args, pdcrt_f f);
bool pdcrt_ejecutar_protegido(pdcrt_ctx *ctx, int args, pdcrt_f f);

#ifdef PDCRT_INTERNO

#define pdcrt_empujar(ctx, v) (ctx)->pila[(ctx)->tam_pila++] = (v)
#define pdcrt_sacar(ctx) (ctx)->pila[--(ctx)->tam_pila]
#define pdcrt_cima(ctx) (ctx)->pila[(ctx)->tam_pila - 1]

#define pdcrt_obtener_local(ctx, m, idx) (m)->locales_y_capturas[(idx)]
#define pdcrt_obtener_captura(ctx, m, idx) (m)->locales_y_capturas[(m)->num_locales + (idx)]
#define pdcrt_fijar_local(ctx, m, idx, v) (m)->locales_y_capturas[(idx)] = (v)
#define pdcrt_fijar_captura(ctx, m, idx, v) (m)->locales_y_capturas[(m)->num_locales + (idx)] = (v)

bool pdcrt_saltar_condicional(pdcrt_ctx *ctx);
pdcrt_k pdcrt_saltar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf);
pdcrt_k pdcrt_enviar_mensaje(pdcrt_ctx *ctx, pdcrt_marco *m,
                             const char* msj, size_t tam_msj,
                             const int* proto, size_t nproto,
                             pdcrt_kf kf);
pdcrt_k pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf);
void pdcrt_prnl(pdcrt_ctx *ctx);
pdcrt_k pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets);
void pdcrt_agregar_nombre(pdcrt_ctx *ctx, const char *nombre, size_t tam_nombre, bool autoejec);
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
