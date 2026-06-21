//
// Created by alinarezrangel on 30/10/25.
//

#include "pdffi.h"

#define PDCRT_INTERNO
#include "pdcrt.h"
#include "pdcrt_ops.h"

struct pdffi_ctx
{
    pdcrt_ctx *ctx;
    pdcrt_marco *m;
    size_t idc_pila;
    pdcrt_entero tam_pila;
};

static void pdffi_verifica_pila_en_la_cima(pdffi_ctx *ctx)
{
    if(ctx->idc_pila + ctx->tam_pila > ctx->ctx->tam_pila)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Marco de pila invalido (tamaño)");
}

static pdcrt_obj pdffi_obtener(pdffi_ctx *ctx, pdffi_reg reg)
{
    pdffi_verifica_pila_en_la_cima(ctx);
    PDCRT_ASSERT(ctx->tam_pila > 0);
    if(reg < 0)
    {
        PDCRT_ASSERT(ctx->tam_pila + reg >= 0);
        size_t idc = ctx->idc_pila + (size_t) (ctx->tam_pila + reg);
        return ctx->ctx->pila[idc];
    }
    else
    {
        PDCRT_ASSERT(reg < ctx->tam_pila);
        return ctx->ctx->pila[ctx->idc_pila + reg];
    }
}

void pdffi_extender_pila(pdffi_ctx *ctx, size_t elementos)
{
    PDCRT_ASSERT(ctx->tam_pila > 0);
    PDCRT_ASSERT(elementos < (size_t) PDCRT_ENTERO_MAX);
    pdffi_verifica_pila_en_la_cima(ctx);
    pdcrt_extender_pila(ctx->ctx, elementos);
    ctx->tam_pila += (pdcrt_entero) elementos;
}

size_t pdffi_tam_pila(pdffi_ctx *ctx)
{
    PDCRT_ASSERT(ctx->tam_pila > 0);
    return ctx->tam_pila;
}

static void pdffi_empujar(pdffi_ctx *ctx, pdcrt_obj obj)
{
    pdffi_verifica_pila_en_la_cima(ctx);
    pdcrt_empujar(ctx->ctx, obj);
    ctx->tam_pila += 1;
}

static pdcrt_obj pdffi_sacar(pdffi_ctx *ctx)
{
    pdffi_verifica_pila_en_la_cima(ctx);
    PDCRT_ASSERT(ctx->tam_pila > 0);
    pdcrt_obj v = pdcrt_sacar(ctx->ctx);
    ctx->tam_pila -= 1;
    return v;
}

pdcrt_entero pdffi_obtener_entero(pdffi_ctx *ctx, pdffi_reg p, bool *ok)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    return pdcrt_obtener_entero_obj(ctx->ctx, v, ok);
}

pdcrt_float pdffi_obtener_float(pdffi_ctx *ctx, pdffi_reg p, bool *ok)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    return pdcrt_obtener_float_obj(ctx->ctx, v, ok);
}

bool pdffi_obtener_boole(pdffi_ctx *ctx, pdffi_reg p, bool *ok)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    if(v.recv == &pdcrt_recv_booleano)
    {
        *ok = true;
        return v.bval;
    }
    else
    {
        *ok = false;
        return false;
    }
}

bool pdffi_es_nulo(pdffi_ctx *ctx, pdffi_reg p)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    return v.recv == &pdcrt_recv_nulo;
}

void *pdffi_obtener_voidptr(pdffi_ctx *ctx, pdffi_reg p, bool *ok)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    if(v.recv == &pdcrt_recv_voidptr)
    {
        *ok = true;
        return v.pval;
    }
    else
    {
        *ok = false;
        return NULL;
    }
}

size_t pdffi_obtener_tam_texto(pdffi_ctx *ctx, pdffi_reg p, bool *ok)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    if(v.recv == &pdcrt_recv_texto)
    {
        *ok = true;
        return v.texto->longitud;
    }
    else
    {
        *ok = false;
        return 0;
    }
}

void pdffi_copiar_texto(pdffi_ctx *ctx, pdffi_reg p, bool *ok,
                        char *txt, size_t tam)
{
    pdcrt_obj v = pdffi_obtener(ctx, p);
    if(v.recv == &pdcrt_recv_texto)
    {
        *ok = true;
        size_t copiar, relleno;
        if(tam > v.texto->longitud)
        {
            copiar = v.texto->longitud;
            relleno = tam - v.texto->longitud;
        }
        else
        {
            copiar = tam;
            relleno = 0;
        }

        if(copiar)
            memcpy(txt, v.texto->contenido, copiar);
        if(relleno)
            memset(txt + copiar, 0, relleno);
    }
    else
    {
        *ok = false;
    }
}

bool pdffi_es_arreglo(pdffi_ctx *ctx, pdffi_reg arr)
{
    pdcrt_obj v = pdffi_obtener(ctx, arr);
    return v.recv == &pdcrt_recv_arreglo;
}

void pdffi_arreglo_en(pdffi_ctx *ctx, pdffi_reg arr, pdffi_reg i)
{
    pdcrt_obj varr = pdffi_obtener(ctx, arr);
    pdcrt_obj vi = pdffi_obtener(ctx, i);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, varr, &pdcrt_recv_arreglo);
    bool ok = false;
    pdcrt_entero ei = pdcrt_obtener_entero_obj(ctx->ctx, vi, &ok);
    if(!ok)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Índice invalido: debe ser un entero");
    if(ei < 0 || ei >= varr.arreglo->longitud)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Índice invalido: fuera de rango");
    pdffi_empujar(ctx, varr.arreglo->valores[ei]);
}

void pdffi_arreglo_en_i(pdffi_ctx *ctx, pdffi_reg arr, pdcrt_entero i)
{
    pdcrt_obj varr = pdffi_obtener(ctx, arr);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, varr, &pdcrt_recv_arreglo);
    if(i < 0 || i >= varr.arreglo->longitud)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Índice invalido: fuera de rango");
    pdffi_empujar(ctx, varr.arreglo->valores[i]);
}

void pdffi_arreglo_fijar_en(pdffi_ctx *ctx, pdffi_reg arr, pdffi_reg i, pdffi_reg v)
{
    pdcrt_obj varr = pdffi_obtener(ctx, arr);
    pdcrt_obj vi = pdffi_obtener(ctx, i);
    pdcrt_obj vv = pdffi_obtener(ctx, v);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, varr, &pdcrt_recv_arreglo);
    bool ok = false;
    pdcrt_entero ei = pdcrt_obtener_entero_obj(ctx->ctx, vi, &ok);
    if(!ok)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Índice invalido: debe ser un entero");
    if(ei < 0 || ei >= varr.arreglo->longitud)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Índice invalido: fuera de rango");
    varr.arreglo->valores[ei] = vv;
}

void pdffi_arreglo_fijar_en_i(pdffi_ctx *ctx, pdffi_reg arr, pdcrt_entero i, pdffi_reg v)
{
    pdcrt_obj varr = pdffi_obtener(ctx, arr);
    pdcrt_obj vv = pdffi_obtener(ctx, v);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, varr, &pdcrt_recv_arreglo);
    if(i < 0 || i >= varr.arreglo->longitud)
        pdcrt_errortb(ctx->ctx, ctx->m, u8"Índice invalido: fuera de rango");
    varr.arreglo->valores[i] = vv;
}

pdcrt_entero pdffi_arreglo_longitud(pdffi_ctx *ctx, pdffi_reg arr)
{
    pdcrt_obj varr = pdffi_obtener(ctx, arr);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, varr, &pdcrt_recv_arreglo);
    return varr.arreglo->longitud;
}

void pdffi_arreglo_agregar_al_final(pdffi_ctx *ctx, pdffi_reg arr, pdffi_reg v)
{
    pdcrt_obj varr = pdffi_obtener(ctx, arr);
    pdcrt_obj vv = pdffi_obtener(ctx, v);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, varr, &pdcrt_recv_arreglo);
    pdcrt_arreglo_abrir_espacio(ctx->ctx,
                                ctx->m,
                                varr.arreglo,
                                1);
    varr.arreglo->valores[varr.arreglo->longitud ++] = vv;
}

bool pdffi_es_diccionario(pdffi_ctx *ctx, pdffi_reg dic)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    return vdic.recv == &pdcrt_recv_tabla;
}

void pdffi_diccionario_en(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_obj vllave = pdffi_obtener(ctx, llave);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    pdcrt_obj vvalor = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx->ctx, vdic.tabla, vllave, &vvalor);
    if(contiene)
    {
        pdffi_empujar(ctx, vvalor);
    }
    else
    {
        pdcrt_errortb(ctx->ctx, ctx->m, "Diccionario (Tabla): Llave no existente");
    }
}

bool pdffi_diccionario_intenta_en(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_obj vllave = pdffi_obtener(ctx, llave);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    pdcrt_obj vvalor = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx->ctx, vdic.tabla, vllave, &vvalor);
    pdffi_empujar(ctx, vvalor);
    return contiene;
}

void pdffi_diccionario_fijar_en(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave, pdffi_reg valor)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_obj vllave = pdffi_obtener(ctx, llave);
    pdcrt_obj vvalor = pdffi_obtener(ctx, valor);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    pdcrt_tabla_fijar(ctx->ctx, vdic.tabla, vllave, vvalor, true);
}

void pdffi_diccionario_eliminar(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_obj vllave = pdffi_obtener(ctx, llave);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    pdcrt_tabla_eliminar(ctx->ctx, vdic.tabla, vllave, true);
}

bool pdffi_diccionario_contiene(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_obj vllave = pdffi_obtener(ctx, llave);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    pdcrt_obj vvalor = pdcrt_objeto_nulo();
    return pdcrt_tabla_en(ctx->ctx, vdic.tabla, vllave, &vvalor);
}

void pdffi_diccionario_vaciar(pdffi_ctx *ctx, pdffi_reg dic)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    pdcrt_tabla_vaciar(ctx->ctx, vdic.tabla, true);
}

pdcrt_entero pdffi_diccionario_longitud(pdffi_ctx *ctx, pdffi_reg dic)
{
    pdcrt_obj vdic = pdffi_obtener(ctx, dic);
    pdcrt_debe_tener_tipo_rapido(ctx->ctx, vdic, &pdcrt_recv_tabla);
    return vdic.tabla->buckets_ocupados;
}

void pdffi_empujar_entero(pdffi_ctx *ctx, pdcrt_entero i)
{
    pdffi_empujar(ctx, pdcrt_objeto_entero(i));
}

void pdffi_empujar_float(pdffi_ctx *ctx, pdcrt_float f)
{
    pdffi_empujar(ctx, pdcrt_objeto_float(f));
}

void pdffi_empujar_booleano(pdffi_ctx *ctx, bool v)
{
    pdffi_empujar(ctx, pdcrt_objeto_booleano(v));
}

void pdffi_empujar_nulo(pdffi_ctx *ctx)
{
    pdffi_empujar(ctx, pdcrt_objeto_nulo());
}

void pdffi_empujar_voidptr(pdffi_ctx *ctx, void *ptr)
{
    pdffi_empujar(ctx, pdcrt_objeto_voidptr(ptr));
}

void pdffi_empujar_texto(pdffi_ctx *ctx, const char *str, size_t tam_str)
{
    PDCRT_DEFINE_RAICES(1);
    PDCRT_GUARDAR_RAIZ_CABECERA(0, ctx->m);
    pdcrt_texto *txt = pdcrt_crear_texto(ctx->ctx, PDCRT_GC(), str, tam_str);
    PDCRT_CARGAR_RAIZ_CABECERA(0, ctx->m);
    pdffi_empujar(ctx, pdcrt_objeto_texto(txt));
}

void pdffi_empujar_texto_c(pdffi_ctx *ctx, const char *str)
{
    PDCRT_DEFINE_RAICES(1);
    PDCRT_GUARDAR_RAIZ_CABECERA(0, ctx->m);
    pdcrt_texto *txt = pdcrt_crear_texto_desde_cstr(ctx->ctx, PDCRT_GC(), str);
    PDCRT_CARGAR_RAIZ_CABECERA(0, ctx->m);
    pdffi_empujar(ctx, pdcrt_objeto_texto(txt));
}

void pdffi_empujar_arreglo_vacio(pdffi_ctx *ctx, size_t cap)
{
    PDCRT_DEFINE_RAICES(1);
    PDCRT_GUARDAR_RAIZ_CABECERA(0, ctx->m);
    pdcrt_arreglo *arr = pdcrt_crear_arreglo_vacio(ctx->ctx, PDCRT_GC(), cap);
    PDCRT_CARGAR_RAIZ_CABECERA(0, ctx->m);
    pdffi_empujar(ctx, pdcrt_objeto_arreglo(arr));
}

void pdffi_empujar_diccionario_vacio(pdffi_ctx *ctx, size_t cap)
{
    PDCRT_DEFINE_RAICES(1);
    PDCRT_GUARDAR_RAIZ_CABECERA(0, ctx->m);
    pdcrt_tabla *tbl = pdcrt_crear_tabla(ctx->ctx, PDCRT_GC(), cap);
    PDCRT_CARGAR_RAIZ_CABECERA(0, ctx->m);
    pdffi_empujar(ctx, pdcrt_objeto_tabla(tbl));
}

pdcrt_obj pdffi_invocar_funcion(pdcrt_ctx *ctx, pdcrt_marco **m, pdffi_f f, int args)
{
    size_t inic = ctx->tam_pila - args;

    pdffi_ctx ffi = {
        .ctx = ctx,
        .idc_pila = inic,
        .tam_pila = args,
        .m = *m,
    };

    int nres = (*f)(&ffi, args);
    *m = ffi.m;
    if(nres != 1)
        pdcrt_errortb(ctx, *m, "Solo se puede devolver un valor");
    pdcrt_obj res = pdffi_sacar(&ffi);
    ctx->tam_pila = ffi.idc_pila;
    return res;
}

static pdcrt_tk pdffi_funcion_de_closure(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    pdcrt_obj cls = pdcrt_obj_desde_xmm(yo);

    PDCRT_DEFINE_RAICES(7);
    PDCRT_GUARDAR_RAIZ_XMM(0, a1);
    PDCRT_GUARDAR_RAIZ_XMM(1, a2);
    PDCRT_GUARDAR_RAIZ_XMM(2, a3);
    PDCRT_GUARDAR_RAIZ_XMM(3, a4);
    PDCRT_GUARDAR_RAIZ_XMM(4, a5);
    PDCRT_GUARDAR_RAIZ_XMM(5, a6);
    PDCRT_GUARDAR_RAIZ_K(6, k);
    pdcrt_marco *m = pdcrt_crear_marco(ctx, PDCRT_GC(), cls.closure->num_capturas, args, k, cls.closure);
    PDCRT_CARGAR_RAIZ_XMM(0, a1);
    PDCRT_CARGAR_RAIZ_XMM(1, a2);
    PDCRT_CARGAR_RAIZ_XMM(2, a3);
    PDCRT_CARGAR_RAIZ_XMM(3, a4);
    PDCRT_CARGAR_RAIZ_XMM(4, a5);
    PDCRT_CARGAR_RAIZ_XMM(5, a6);
    PDCRT_CARGAR_RAIZ_K(6, k);

    // pila: [a7, a8, a9, ...]

    size_t cima = ctx->tam_pila, inic = ctx->tam_pila - (args > PDCRT_NN_IMM ? args - PDCRT_NN_IMM : 0);
    size_t tam = cima - inic, mitad_tam = tam / 2;
    for(size_t i = 0; i < mitad_tam; i ++)
    {
        pdcrt_obj x = ctx->pila[i + inic];
        ctx->pila[i + inic] = ctx->pila[(tam - 1) - i];
        ctx->pila[(tam - 1) - i] = x;
    }

    // pila: [..., a9, a8, a7]

    pdcrt_extender_pila(ctx, 6 + (m->num_registros - 1));
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a6));
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a5));
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a4));
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a3));
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a2));
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a1));

    // pila: [..., a9, a8, a7, a6, a5, a4, ..., a1]

    for(size_t i = 1; i < m->num_registros; i ++)
    {
        pdcrt_empujar(ctx, m->registros[m->num_registros - i]);
    }

    // pila: [..., a9, a8, a7, a6, a5, a4, ..., a1, ..., c3, c2, c1]

    cima = ctx->tam_pila;
    tam = cima - inic;
    mitad_tam = tam / 2;

    for(size_t i = 0; i < mitad_tam; i ++)
    {
        pdcrt_obj x = ctx->pila[i + inic];
        ctx->pila[i + inic] = ctx->pila[(tam - 1) - i];
        ctx->pila[(tam - 1) - i] = x;
    }

    // pila: [c1, c2, c3, ..., a1, a2, a3, ..., a7, a8, a9, ...]

    pdcrt_debe_tener_tipo_rapido(ctx, m->registros[0], &pdcrt_recv_voidptr);
    pdffi_f f = m->registros[0].pval;
    pdcrt_obj res = pdffi_invocar_funcion(ctx, &m, f, args + (m->num_registros - 1));
    return pdcrt_continuar(ctx, m->k, pdcrt_xmm_desde_obj(res));
}

void pdffi_empujar_closure(pdffi_ctx *ctx, pdffi_f f, size_t n_capturas)
{
    PDCRT_DEFINE_RAICES(1);
    PDCRT_GUARDAR_RAIZ_CABECERA(0, ctx->m);
    pdcrt_closure *cls = pdcrt_crear_closure(ctx->ctx, PDCRT_GC(), &pdffi_funcion_de_closure, n_capturas + 1);
    PDCRT_CARGAR_RAIZ_CABECERA(0, ctx->m);
    cls->capturas[0] = pdcrt_objeto_voidptr(f);
    for(size_t i = 0; i < n_capturas; i ++)
    {
        cls->capturas[n_capturas - i] = pdffi_sacar(ctx);
    }
    pdffi_empujar(ctx, pdcrt_objeto_closure(cls));
}

void pdffi_llamar(pdffi_ctx *ctx, pdffi_reg func, int nargs, int nrets)
{
    return pdffi_enviar_mensaje(ctx, func, "llamar", 6, nargs, nrets);
}

void pdffi_llamarv(pdffi_ctx *ctx, pdffi_reg func, int nargs, int *nrets)
{
    return pdffi_enviar_mensajev(ctx, func, "llamar", 6, nargs, nrets);
}

int pdffi_llamark(pdffi_ctx *ctx, pdffi_reg func, int nargs, int nrets, void *k_env, pdffi_k k)
{
    return pdffi_enviar_mensajek(ctx, func, "llamar", 6, nargs, nrets, k_env, k);
}

int pdffi_llamarvk(pdffi_ctx *ctx, pdffi_reg func, int nargs, void *k_env, pdffi_vk k)
{
    return pdffi_enviar_mensajevk(ctx, func, "llamar", 6, nargs, k_env, k);
}

int pdffi_llamart(pdffi_ctx *ctx, pdffi_reg func, int nargs)
{
    return pdffi_enviar_mensajet(ctx, func, "llamar", 6, nargs);
}

_Noreturn int pdffi_error(pdffi_ctx *ctx, const char *msj)
{
    pdcrt_errortb(ctx->ctx, ctx->m, msj);
}

static pdcrt_tk pdffi_dentro_de_ejecutar_k(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res)
{
    PDCRT_K(pdffi_dentro_de_ejecutar_k);
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(res));
    return pdcrt_continuar(ctx, m->k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
}

static pdcrt_tk pdffi_dentro_de_ejecutar(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    pdcrt_obj cls = pdcrt_obj_desde_xmm(yo);
    pdcrt_debe_tener_tipo_rapido(ctx, cls, &pdcrt_recv_closure);
    pdcrt_obj obj = cls.closure->capturas[0];
    pdcrt_obj rmsj = cls.closure->capturas[1];
    pdcrt_obj rargs = cls.closure->capturas[2];
    pdcrt_debe_tener_tipo_rapido(ctx, rmsj, &pdcrt_recv_texto);
    pdcrt_debe_tener_tipo_rapido(ctx, rargs, &pdcrt_recv_entero);
    return pdcrt_llamarn(ctx, k.marco, k.kf, rargs.ival, pdcrt_xmm_desde_obj(obj), pdcrt_xmm_desde_obj(rmsj));
}

void pdffi_enviar_mensaje(pdffi_ctx *ctx, pdffi_reg obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs, int nrets)
{
    if(nrets != 1)
        pdcrt_errortb(ctx->ctx, ctx->m, "Solo se puede devolver 1 valor");

    PDCRT_DEFINE_RAICES(1);
    PDCRT_GUARDAR_RAIZ_CABECERA(0, ctx->m);
    pdcrt_texto *txt = pdcrt_crear_texto(ctx->ctx, PDCRT_GC(), nombre, tam_nombre);
    PDCRT_CARGAR_RAIZ_CABECERA(0, ctx->m);

    pdcrt_obj vobj = pdffi_obtener(ctx, obj);
    pdcrt_extender_pila(ctx->ctx, 3);
    pdcrt_empujar(ctx->ctx, vobj);
    pdcrt_empujar(ctx->ctx, pdcrt_objeto_texto(txt));
    pdcrt_empujar(ctx->ctx, pdcrt_objeto_entero(nargs));
    pdcrt_ejecutar(ctx->ctx, 2, &pdffi_dentro_de_ejecutar);
}

void pdffi_enviar_mensajev(pdffi_ctx *ctx, pdffi_reg obj,
                           const char *nombre, size_t tam_nombre,
                           int nargs, int *nrets)
{
    *nrets = 1;
    pdffi_enviar_mensaje(ctx, obj, nombre, tam_nombre, nargs, 1);
}

int pdffi_enviar_mensajek(pdffi_ctx *ctx, pdffi_reg obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs, int nrets,
                          void *k_env, pdffi_k k)
{
    // TODO: implementar
    PDCRT_BUG(true, "implementar");
}

int pdffi_enviar_mensajevk(pdffi_ctx *ctx, pdffi_reg obj,
                           const char *nombre, size_t tam_nombre,
                           int nargs,
                           void *k_env, pdffi_vk k)
{
    // TODO: implementar
    PDCRT_BUG(true, "implementar");
}

int pdffi_enviar_mensajet(pdffi_ctx *ctx, pdffi_reg obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs)
{
    // TODO: implementar
    PDCRT_BUG(true, "implementar");
}
