//
// Created by alinarezrangel on 15/3/26.
//

#define PDCRT_INTERNO
#include "pdcrt.h"
#include "pdcrt_base.h"

#include <stddef.h>

void pdcrt_gc_eliminar_de_grupo(pdcrt_gc_grupo *grupo, pdcrt_cabecera_gc *h)
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

void pdcrt_gc_agregar_a_grupo(pdcrt_gc_grupo *grupo, pdcrt_cabecera_gc *h)
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

extern void pdcrt_barrera_de_escritura_cabecera(pdcrt_ctx *ctx, pdcrt_cabecera_gc *ch, pdcrt_cabecera_gc *vh);
extern void pdcrt_barrera_de_escritura(pdcrt_ctx *ctx, pdcrt_obj contenedor, pdcrt_obj valor);

pdcrt_recoleccion pdcrt_gc_recoleccion_por_pila(pdcrt_ctx *ctx)
{
    (void) ctx;
    return (pdcrt_recoleccion) {
        .tipo = PDCRT_RECOLECCION_SIN_PILA,
    };
}

pdcrt_recoleccion pdcrt_gc_recoleccion_por_memoria(pdcrt_ctx *ctx, size_t memoria_requerida)
{
    (void) ctx;
    return (pdcrt_recoleccion) {
        .tipo = PDCRT_RECOLECCION_SIN_MEMORIA,
        .sin_memoria = {
            .memoria_requerida = memoria_requerida,
        },
    };
}

extern bool pdcrt_gc_recoleccion_debe_mover_pila(pdcrt_recoleccion r);
extern bool pdcrt_gc_recoleccion_es_mayor(pdcrt_recoleccion r);
extern bool pdcrt_gc_recoleccion_es_menor(pdcrt_recoleccion r);

void pdcrt_intenta_invocar_al_recolector(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_recoleccion params)
{
    if(ctx->recolector_de_basura_activo)
    {
        size_t usado = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
        if(usado >= ctx->gc.tam_heap)
        {
            pdcrt_recolectar_basura_simple(ctx, m, params);
            size_t nuevo_usado = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
            if(nuevo_usado >= ((ctx->gc.tam_heap / 2) * 1))
            {
                ctx->gc.tam_heap = nuevo_usado + 20 * 1024 * 1024; // +20MiB
                ctx->gc.num_recolecciones = 0;
            }
            else if(nuevo_usado <= (ctx->gc.tam_heap / 2))
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

void pdcrt_activar_recolector_de_basura(pdcrt_ctx *ctx)
{
    ctx->recolector_de_basura_activo = true;
}

void pdcrt_desactivar_recolector_de_basura(pdcrt_ctx *ctx)
{
    ctx->recolector_de_basura_activo = false;
}

void pdcrt_inicializar_obj(pdcrt_ctx *ctx,
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

void *pdcrt_alojar_obj(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_tipo_obj_gc tipo, size_t sz)
{
    pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_memoria(ctx, sz);
    pdcrt_intenta_invocar_al_recolector(ctx, m, params);
    pdcrt_cabecera_gc *h = pdcrt_alojar_ctx(ctx, sz);
    if(!h)
        pdcrt_error(ctx, u8"No se pudo alojar más memoria");
    pdcrt_inicializar_obj(ctx, h, tipo, sz);
    PDCRT_PROBE0(gc_nuevo_objeto_heap);
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
    nuevo->gc.tipo = h->tipo;
    nuevo->gc.num_bytes = h->num_bytes;

    pdcrt_objeto_generico_gc *actual = (pdcrt_objeto_generico_gc *) h;
    size_t tam_cuerpo = h->num_bytes - offsetof(pdcrt_objeto_generico_gc, bytes);
    memcpy(&nuevo->bytes, &actual->bytes, tam_cuerpo);

    h->tipo = PDCRT_TGC_REUBICADO;
    pdcrt_reubicado *reub = (pdcrt_reubicado *) h;
    reub->nueva_direccion = PDCRT_CABECERA_GC(nuevo);
    PDCRT_PROBE0(gc_reubicar);
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
            for(size_t i = 0; i < m->num_registros; i++)
                f_obj(ctx, &m->registros[i], params);
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
            for(size_t i = 0; i < pdcrt_tabla_num_buckets_hasheables(tbl->mascara); i++)
            {
                pdcrt_bucket *b = &tbl->buckets[i];
                if(!b->activo)
                    continue;
                f_obj(ctx, &b->llave, params);
                f_obj(ctx, &b->valor, params);
            }
            for(size_t i = 0; i < tbl->num_colisiones; i++)
            {
                pdcrt_bucket *col = &tbl->colisiones[i];
                if(!col->activo)
                    continue;
                f_obj(ctx, &col->llave, params);
                f_obj(ctx, &col->valor, params);
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

static void pdcrt_gc_marcar_y_mover_todo(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_recoleccion params)
{
    for(size_t i = 0; i < ctx->tam_pila; i++)
        pdcrt_gc_marcar_raiz_obj(ctx, &ctx->pila[i], params);

    // TODO: Encojer la pila si tiene mucha capacidad sin usar

    while(m)
    {
        for(size_t i = 0; i < m->num_raices; i++)
        {
            pdcrt_obj *o = &m->raices_locales[i];
            if(!o->recv && !o->gc)
                continue;

            if(o->recv)
                pdcrt_gc_marcar_raiz_obj(ctx, &m->raices_locales[i], params);
            else
                pdcrt_gc_marcar_raiz(ctx, &o->gc, params);
        }
        m = m->superior;
    }

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
    pdcrt_gc_marcar_raiz_obj(ctx, &ctx->clase_tabla, params);

    if(ctx->continuacion_actual.k.marco)
    {
        pdcrt_gc_marcar_raiz(ctx, PDCRT_CABECERA_GC_PTR(&ctx->continuacion_actual.k.marco), params);
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

size_t pdcrt_gc_liberar_objeto(pdcrt_ctx *ctx, pdcrt_cabecera_gc *h)
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
        liberado += pdcrt_tabla_desalojar(ctx, tbl);
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

void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx,
                                    pdcrt_gc_raices *m,
                                    pdcrt_recoleccion params)
{
    struct timespec inicio, marcar, recolectar, total;
    pdcrt_timediff dif_marcar, dif_recolectar, dif_total;
    size_t mem_usada_al_inicio = 0, mem_usada_al_final = 0;
    char buffer[PDCRT_FORMATEAR_BYTES_TAM_BUFFER];

    if(PDCRT_DTRACE_COMPILADO)
    {
        mem_usada_al_inicio = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
        int tipo_recoleccion = params.tipo == PDCRT_RECOLECCION_SIN_MEMORIA ? 0 : 1;
        PDCRT_PROBE2(gc_entry, mem_usada_al_inicio, tipo_recoleccion);
    }

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

    PDCRT_PROBE0(gc_marcar_entry);

    pdcrt_gc_marcar_y_mover_todo(ctx, m, params);
    while(ctx->gc.gris.primero)
    {
        pdcrt_gc_marcar_y_mover_todos_los_grises(ctx, params);
    }

    PDCRT_PROBE0(gc_marcar_exit);

    if(ctx->log.gc && PDCRT_LOG_COMPILADO && ctx->capacidades.time)
    {
        pdcrt_time(&marcar);
        pdcrt_diferencia(&inicio, &marcar, &dif_marcar);
        pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "marcar: %ld.%03ld (%03ld %03ld)\n", dif_marcar.dif_s, dif_marcar.dif_ms, dif_marcar.dif_us, dif_marcar.dif_ns);
    }

    PDCRT_PROBE0(gc_recolectar_entry);

    pdcrt_gc_recolectar(ctx, params);

    PDCRT_PROBE0(gc_recolectar_exit);

    if(ctx->log.gc && PDCRT_LOG_COMPILADO && ctx->capacidades.time)
    {
        pdcrt_time(&recolectar);
        pdcrt_diferencia(&inicio, &recolectar, &dif_recolectar);
        pdcrt_log(ctx, PDCRT_SUBSISTEMA_GC, "recolectar: %ld.%03ld (%03ld %03ld)\n", dif_recolectar.dif_s, dif_recolectar.dif_ms, dif_recolectar.dif_us, dif_recolectar.dif_ns);
    }

    PDCRT_PROBE0(gc_reset_entry);

    pdcrt_gc_mover_negros_a_blancos(ctx, params);

    PDCRT_PROBE0(gc_reset_exit);

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

    if(PDCRT_DTRACE_COMPILADO)
    {
        mem_usada_al_final = pdcrt_alojador_con_estadisticas_obtener_usado(ctx->alojador);
        PDCRT_PROBE1(gc_exit, mem_usada_al_final);
    }
}

void pdcrt_recolectar_basura_por_pila(pdcrt_ctx *ctx, pdcrt_gc_raices *m)
{
    pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_pila(ctx);
    pdcrt_recolectar_basura_simple(ctx, m, params);
}
