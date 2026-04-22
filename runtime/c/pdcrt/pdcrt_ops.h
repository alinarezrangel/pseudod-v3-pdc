//
// Created by alinarezrangel on 15/3/26.
//

#ifndef PDCRT_OPS_H
#define PDCRT_OPS_H

#define PDCRT_INTERNO
#include "pdcrt.h"


void pdcrt_extender_pila(pdcrt_ctx *ctx, size_t num_elem);

typedef ssize_t pdcrt_stp;

pdcrt_texto* pdcrt_crear_texto(pdcrt_ctx *ctx, pdcrt_gc_raices *m, const char *str, size_t len);

pdcrt_entero pdcrt_obtener_entero_obj(pdcrt_ctx *ctx, pdcrt_obj o, bool *ok);
pdcrt_float pdcrt_obtener_float_obj(pdcrt_ctx *ctx, pdcrt_obj o, bool *ok);
size_t pdcrt_obtener_tam_texto_obj(pdcrt_ctx *ctx, pdcrt_obj o, bool *ok);

void pdcrt_eliminar_elemento(pdcrt_ctx *ctx, pdcrt_stp pos);
void pdcrt_eliminar_elementos(pdcrt_ctx *ctx, pdcrt_stp inic, size_t cnt);

void pdcrt_duplicar(pdcrt_ctx *ctx, pdcrt_stp i);
void pdcrt_extraer(pdcrt_ctx *ctx, pdcrt_stp i);
void pdcrt_insertar(pdcrt_ctx *ctx, pdcrt_stp pos);

// TODO: ¿Especializar? mover_a_cima(1), mover_a_cima(2), etc
void pdcrt_mover_a_cima(pdcrt_ctx *ctx, pdcrt_stp pos, size_t num_els);

void pdcrt_empujar_interceptar(pdcrt_ctx *ctx, pdcrt_obj o);
void pdcrt_fijar_pila_interceptar(pdcrt_ctx *ctx, size_t i, pdcrt_obj v);

#define pdcrt_empujar_ll(ctx, v) (ctx)->pila[(ctx)->tam_pila++] = (v)
#define pdcrt_fijar_pila_ll(ctx, i, v) (ctx)->pila[(i)] = (v)

#ifdef PDCRT_EMP_INTR
#define pdcrt_empujar(ctx, v) pdcrt_empujar_interceptar(ctx, v)
#define pdcrt_fijar_pila(ctx, i, v) pdcrt_fijar_pila_interceptar(ctx, i, v)
#else
#define pdcrt_empujar(ctx, v) pdcrt_empujar_ll(ctx, v)
#define pdcrt_fijar_pila(ctx, i, v) pdcrt_fijar_pila_ll(ctx, i, v)
#endif
#define pdcrt_sacar(ctx) (ctx)->pila[--(ctx)->tam_pila]
#define pdcrt_cima(ctx) (ctx)->pila[(ctx)->tam_pila - 1]
#define pdcrt_cima_en(ctx, n) (ctx)->pila[(ctx)->tam_pila - (1 + (n))]

#define pdcrt_obtener_local(ctx, m, idx) ((m)->registros[(idx)])
#define pdcrt_fijar_local(ctx, m, idx, v)                               \
    do                                                                  \
    {                                                                   \
        pdcrt_obj local = (v);                                          \
        pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(local);            \
        if(vh)                                                          \
            pdcrt_barrera_de_escritura_cabecera((ctx), PDCRT_CABECERA_GC(m), vh); \
        (m)->registros[(idx)] = local;                                  \
    }                                                                   \
    while(0)

pdcrt_obj pdcrt_caja_vacia(pdcrt_ctx *ctx, pdcrt_gc_raices *m);

#define pdcrt_fijar_caja(ctx, o, v)                     \
    do                                                  \
    {                                                   \
        pdcrt_obj valor = (v);                          \
        pdcrt_barrera_de_escritura((ctx), (o), valor);  \
        (o).caja->valor = valor;                        \
    }                                                   \
    while(0)
#define pdcrt_obtener_caja(ctx, o) (o).caja->valor

typedef struct pdcrt_params_base
{
    size_t num_params;
    bool tiene_variadic : 1;
    size_t idc_variadic : sizeof(size_t) - 1;
} pdcrt_params_base;

typedef struct pdcrt_param_data
{
    size_t reg;
} pdcrt_param_data;

typedef struct pdcrt_params_data
{
    pdcrt_params_base base;
    pdcrt_param_data params[];
} pdcrt_params_data;

void pdcrt_params(pdcrt_ctx *ctx,
                  pdcrt_marco *m,
                  pdcrt_params_data *restrict p,
                  PDCRT_F_IMM);

size_t pdcrt_expandir_varargs(pdcrt_ctx *ctx, const int* proto, size_t nproto);

pdcrt_tk pdcrt_llamar0(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj);
pdcrt_tk pdcrt_llamar1(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1);
pdcrt_tk pdcrt_llamar2(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2);
pdcrt_tk pdcrt_llamar3(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3);
pdcrt_tk pdcrt_llamar4(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3, __m128i a4);
pdcrt_tk pdcrt_llamar5(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5);
pdcrt_tk pdcrt_llamar6(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5, __m128i a6);
pdcrt_tk pdcrt_llamarn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       int nargs,
                       __m128i obj, __m128i msj);
pdcrt_tk pdcrt_llamarnr(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                        int nargs,
                        __m128i obj, __m128i msj,
                        __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5, __m128i a6);

pdcrt_tk pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets);
pdcrt_tk pdcrt_devolver1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res);

PDCRT_INLINE pdcrt_tk pdcrt_saltar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf, __m128i res)
{
    return (*kf)(ctx, m, res);
}

pdcrt_tk pdcrt_importar(pdcrt_ctx *ctx,
                        pdcrt_marco *m,
                        const char *nombre,
                        size_t tam_nombre,
                        pdcrt_kf kf);
pdcrt_tk pdcrt_extraerv(pdcrt_ctx *ctx,
                        pdcrt_marco *m,
                        pdcrt_obj mod,
                        const char *nombre,
                        size_t tam_nombre,
                        pdcrt_kf kf);
pdcrt_tk pdcrt_agregar_nombre(pdcrt_ctx *ctx,
                              pdcrt_marco *m,
                              pdcrt_obj mod,
                              pdcrt_obj valor,
                              const char *nombre,
                              size_t tam_nombre,
                              bool autoejec,
                              pdcrt_kf kf);
pdcrt_tk pdcrt_exportar(pdcrt_ctx *ctx,
                        pdcrt_marco *m,
                        pdcrt_obj mod,
                        const char *modulo,
                        size_t tam_modulo);

typedef struct pdcrt_captura
{
    size_t registro;
} pdcrt_captura;

pdcrt_obj pdcrt_crear_closure_obj(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_f f, size_t num_caps);
pdcrt_obj pdcrt_crear_closure_obj_0(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_f f);
pdcrt_obj pdcrt_crear_closure_obj_1(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_f f, __m128i c1);

pdcrt_obj pdcrt_mk_closure(pdcrt_ctx *ctx,
                           pdcrt_marco *m,
                           pdcrt_gc_raices *rc,
                           pdcrt_f f,
                           const pdcrt_captura *caps,
                           size_t ncaps);

void pdcrt_assert(pdcrt_ctx *ctx, pdcrt_obj v);


pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, pdcrt_gc_raices *m, size_t registros, int args, pdcrt_k k, pdcrt_closure *capturas);
void pdcrt_inicializar_marco(pdcrt_ctx *ctx,
                             pdcrt_marco *m,
                             size_t sz,
                             size_t registros,
                             int args,
                             pdcrt_k k,
                             pdcrt_closure *capturas);
pdcrt_arreglo* pdcrt_crear_arreglo_vacio(pdcrt_ctx *ctx, pdcrt_gc_raices *m, size_t capacidad);
pdcrt_closure* pdcrt_crear_closure(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_f f, size_t capturas);
pdcrt_caja* pdcrt_crear_caja(pdcrt_ctx *ctx, pdcrt_gc_raices *m);
pdcrt_tabla* pdcrt_crear_tabla(pdcrt_ctx *ctx, pdcrt_gc_raices *m, size_t capacidad);
pdcrt_valop* pdcrt_crear_valop(pdcrt_ctx *ctx, pdcrt_gc_raices *m, size_t num_bytes);
pdcrt_corrutina* pdcrt_crear_corrutina_obj(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_obj f);
pdcrt_instancia* pdcrt_crear_instancia_obj(pdcrt_ctx *ctx, pdcrt_gc_raices *m,
                                           pdcrt_obj metodos, pdcrt_obj metodo_no_encontrado, size_t num_atrs);

PDCRT_INLINE pdcrt_obj pdcrt_crear_espacio_de_nombres_cons(pdcrt_ctx *ctx, pdcrt_gc_raices *m)
{
    return pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, m, 32));
}

#endif //PDCRT_OPS_H