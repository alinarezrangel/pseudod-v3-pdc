//
// Created by alinarezrangel on 15/3/26.
//

#include "pdcrt_ops.h"


static size_t pdcrt_stp_a_pos(pdcrt_ctx *ctx, pdcrt_stp i)
{
    if(i < 0)
        return ctx->tam_pila + i;
    else
        return i;
}

static void pdcrt_inicializar_marco_impl(pdcrt_ctx *ctx,
                                         pdcrt_marco *m,
                                         size_t registros,
                                         size_t capturas,
                                         int args,
                                         pdcrt_k k)
{
    m->args = args;
    m->k = k;
    m->num_registros = registros;
    for(size_t i = 0; i < registros; i++)
    {
        m->registros[i] = pdcrt_objeto_nulo();
    }
    size_t top = ctx->tam_pila;
    for(size_t i = 0; i < capturas; i++)
    {
        pdcrt_obj val = ctx->pila[(top - capturas) + i];
        pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(val);
        if(vh)
            pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(m), vh);
        m->registros[i] = val;
    }
    ctx->tam_pila -= capturas;
}

pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, size_t registros, size_t capturas, int args, pdcrt_k k)
{
    pdcrt_marco *m = pdcrt_alojar_obj(ctx, &k.marco, PDCRT_TGC_MARCO, sizeof(pdcrt_marco) + sizeof(pdcrt_obj) * registros);
    if(!m)
        pdcrt_enomem(ctx);
    pdcrt_inicializar_marco_impl(ctx, m, registros, capturas, args, k);
    return m;
}

void pdcrt_inicializar_marco(pdcrt_ctx *ctx,
                             pdcrt_marco *m,
                             size_t sz,
                             size_t registros,
                             size_t capturas,
                             int args,
                             pdcrt_k k)
{
    pdcrt_inicializar_obj(ctx, PDCRT_CABECERA_GC(m), PDCRT_TGC_MARCO, sz);
    pdcrt_inicializar_marco_impl(ctx, m, registros, capturas, args, k);
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

pdcrt_caja* pdcrt_crear_caja(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    pdcrt_caja *c = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_CAJA, sizeof(pdcrt_caja));
    if(!c)
        pdcrt_enomem(ctx);
    c->valor = pdcrt_objeto_nulo();
    return c;
}

pdcrt_tabla* pdcrt_crear_tabla(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad)
{
    pdcrt_tabla *tbl = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_TABLA, sizeof(pdcrt_tabla));
    if(!tbl)
        pdcrt_enomem(ctx);
    pdcrt_tabla_inicializar(ctx, tbl, capacidad);
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

pdcrt_corrutina* pdcrt_crear_corrutina_obj(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_obj f)
{
    pdcrt_corrutina *coro = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_CORO, sizeof(pdcrt_corrutina));
    if(!coro)
        pdcrt_enomem(ctx);
    coro->estado = PDCRT_CORO_INICIAL;
    coro->punto_de_inicio = f;
    return coro;
}

pdcrt_corrutina* pdcrt_crear_corrutina(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp f_idx)
{
    return pdcrt_crear_corrutina_obj(ctx, m, ctx->pila[pdcrt_stp_a_pos(ctx, f_idx)]);
}

pdcrt_instancia* pdcrt_crear_instancia_obj(pdcrt_ctx *ctx, pdcrt_marco **m,
                                           pdcrt_obj metodos, pdcrt_obj metodo_no_encontrado, size_t num_atrs)
{
    pdcrt_instancia *inst = pdcrt_alojar_obj(ctx, m, PDCRT_TGC_INSTANCIA, sizeof(pdcrt_instancia) + (sizeof(pdcrt_obj) * num_atrs));
    if(!inst)
        pdcrt_enomem(ctx);
    inst->metodos = metodos;
    inst->metodo_no_encontrado = metodo_no_encontrado;
    inst->num_atributos = num_atrs;
    for(size_t i = 0; i < num_atrs; i++)
        inst->atributos[i] = pdcrt_objeto_nulo();
    return inst;
}

pdcrt_instancia* pdcrt_crear_instancia(pdcrt_ctx *ctx, pdcrt_marco **m,
                                       pdcrt_stp metodos, pdcrt_stp metodo_no_encontrado, size_t num_atrs)
{
    return pdcrt_crear_instancia_obj(ctx, m,
                                     ctx->pila[pdcrt_stp_a_pos(ctx, metodos)],
                                     ctx->pila[pdcrt_stp_a_pos(ctx, metodo_no_encontrado)],
                                     num_atrs);
}

void pdcrt_empujar_interceptar(pdcrt_ctx *ctx, pdcrt_obj o)
{
    pdcrt_empujar_ll(ctx, o);
}

void pdcrt_fijar_pila_interceptar(pdcrt_ctx *ctx, size_t i, pdcrt_obj v)
{
    pdcrt_fijar_pila_ll(ctx, i, v);
}


void pdcrt_extender_pila(pdcrt_ctx *ctx, pdcrt_marco *m, size_t num_elem)
{
    (void) m;
    if((num_elem + ctx->tam_pila) > ctx->cap_pila)
    {
        // TODO: Redimensionar por el mínimo necesario es estúpido, mejor
        //  redimensionar por MAX(num_elem + tam_pila, cap_pila * 2)
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

pdcrt_obj pdcrt_crear_espacio_de_nombres_cons(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    return pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, m, 32));
}

pdcrt_entero pdcrt_obtener_entero_obj(pdcrt_ctx *ctx, pdcrt_obj o, bool *ok)
{
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
            // ReSharper disable once CppDFAUnreachableCode
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

pdcrt_float pdcrt_obtener_float_obj(pdcrt_ctx *ctx, pdcrt_obj o, bool *ok)
{
    *ok = false;
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_ENTERO)
    {
        // ReSharper disable once CppDFAUnreachableCode
        if(PDCRT_FLOAT_MANT_DIG >= PDCRT_ENTERO_BITS)
        {
            *ok = true;
            return (pdcrt_float) o.ival;
        }
        else
        {
            // No todos los enteros son representables como floats
            // NOTA: Mantener sincronizado con `pdcrt_comparar_entero_y_float()`
            static const pdcrt_entero max_entero_repr_float = (1ULL << PDCRT_FLOAT_MANT_DIG) - 1U;
            static const pdcrt_entero min_entero_repr_float = -max_entero_repr_float;
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

size_t pdcrt_obtener_tam_texto_obj(pdcrt_ctx *ctx, pdcrt_obj o, bool *ok)
{
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

void pdcrt_eliminar_elementos(pdcrt_ctx *ctx, pdcrt_stp inic, size_t cnt)
{
    size_t rpos = pdcrt_stp_a_pos(ctx, inic);
    size_t n = (ctx->tam_pila - rpos) - cnt;
    if(n > 0)
        memmove(ctx->pila + rpos, ctx->pila + rpos + cnt, n * sizeof(pdcrt_obj));
    ctx->tam_pila -= cnt;
}

void pdcrt_eliminar_elemento(pdcrt_ctx *ctx, pdcrt_stp pos)
{
    pdcrt_eliminar_elementos(ctx, pos, 1);
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


void pdcrt_params(pdcrt_ctx *ctx,
                     pdcrt_marco *m,
                     pdcrt_params_data *restrict p)
{
    if(m->args < p->base.num_params - (p->base.tiene_variadic ? 1 : 0))
        pdcrt_error(ctx, u8"función llamada de forma inválida");

    // Núm. parámetros antes del variadic
    size_t prefijo = p->base.tiene_variadic ? p->base.idc_variadic : p->base.num_params;
    // Núm. parámetros después del variadic
    size_t sufijo = p->base.tiene_variadic ? p->base.num_params - p->base.idc_variadic - 1 : 0;
    // Núm. argumentos variadic
    size_t num_args_variadic = p->base.tiene_variadic ? m->args - prefijo - sufijo : 0;
    // Núm. parámetros variadic
    size_t num_param_variadic = p->base.tiene_variadic ? 1 : 0;

    assert(m->args == (prefijo + num_args_variadic + sufijo));
    assert(p->base.num_params == (prefijo + sufijo + num_param_variadic));

    for(size_t i = 0; i < prefijo; i++)
    {
        m->registros[p->params[i].reg] = ctx->pila[ctx->tam_pila - m->args + i];
    }
    for(size_t i = 0; i < sufijo; i++)
    {
        m->registros[p->params[prefijo + num_param_variadic + i].reg] =
            ctx->pila[ctx->tam_pila - m->args + prefijo + num_args_variadic + i];
    }

    if(p->base.tiene_variadic)
    {
        pdcrt_obj arr = pdcrt_objeto_arreglo(pdcrt_crear_arreglo_vacio(ctx, &m, num_args_variadic));
        for(int i = 0; i < num_args_variadic; i++)
        {
            pdcrt_obj val = ctx->pila[ctx->tam_pila - m->args + prefijo + i];
            if(arr.arreglo->longitud >= arr.arreglo->capacidad)
                pdcrt_error(ctx, u8"arreglo de variadics demasiado pequeño");
            arr.arreglo->valores[arr.arreglo->longitud++] = val;
        }
        m->registros[p->params[p->base.idc_variadic].reg] = arr;
    }

    ctx->tam_pila -= m->args; // saca todos los argumentos
}


size_t pdcrt_expandir_varargs(pdcrt_ctx *ctx, pdcrt_marco *m, const int* proto, size_t nproto)
{
    // TODO implementar
}

pdcrt_tk pdcrt_llamar0(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                      __m128i obj, __m128i msj)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 0, (pdcrt_k) { .kf = kf, .marco = m },
                        obj, msj,
                        PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO());
}

pdcrt_tk pdcrt_llamar1(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1)

{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 1, (pdcrt_k) { .kf = kf, .marco = m },
                        obj, msj,
                        a1, PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO());
}

pdcrt_tk pdcrt_llamar2(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 2, (pdcrt_k) { .kf = kf, .marco = m },
                        obj, msj,
                        a1, a2, PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO());
}

pdcrt_tk pdcrt_llamar3(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 3, (pdcrt_k) { .kf = kf, .marco = m },
                        obj, msj,
                        a1, a2, a3, PDCRT_XMM_NULO(), PDCRT_XMM_NULO(), PDCRT_XMM_NULO());
}

pdcrt_tk pdcrt_llamar4(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3, __m128i a4)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 4, (pdcrt_k) { .kf = kf, .marco = m },
                        obj, msj,
                        a1, a2, a3, a4, PDCRT_XMM_NULO(), PDCRT_XMM_NULO());
}

pdcrt_tk pdcrt_llamar5(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 5, (pdcrt_k) { .kf = kf, .marco = m },
                        obj, msj,
                        a1, a2, a3, a4, a5, PDCRT_XMM_NULO());
}

pdcrt_tk pdcrt_llamar6(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       __m128i obj, __m128i msj,
                       __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5, __m128i a6)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, 6, (pdcrt_k) { .kf = kf, .marco = m }, obj, msj, a1, a2, a3, a4, a5, a6);
}

pdcrt_tk pdcrt_llamarn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                       int nargs,
                       __m128i obj, __m128i msj)
{
    if(nargs == 0)
    {
        return pdcrt_llamar0(ctx, m, kf, obj, msj);
    }
    else if(nargs == 1)
    {
        __m128i a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        return pdcrt_llamar1(ctx, m, kf, obj, msj, a1);
    }
    else if(nargs == 2)
    {
        __m128i a2 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        return pdcrt_llamar2(ctx, m, kf, obj, msj, a1, a2);
    }
    else if(nargs == 3)
    {
        __m128i a3 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a2 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        return pdcrt_llamar3(ctx, m, kf, obj, msj, a1, a2, a3);
    }
    else if(nargs == 4)
    {
        __m128i a4 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a3 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a2 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        return pdcrt_llamar4(ctx, m, kf, obj, msj, a1, a2, a3, a4);
    }
    else if(nargs == 5)
    {
        __m128i a5 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a4 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a3 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a2 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        return pdcrt_llamar5(ctx, m, kf, obj, msj, a1, a2, a3, a4, a5);
    }
    else if(nargs == 6)
    {
        __m128i a6 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a5 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a4 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a3 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a2 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        __m128i a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
        return pdcrt_llamar6(ctx, m, kf, obj, msj, a1, a2, a3, a4, a5, a6);
    }
    else
    {
        __m128i a6 = pdcrt_xmm_desde_obj(ctx->pila[(ctx->tam_pila - nargs) - 0]);
        __m128i a5 = pdcrt_xmm_desde_obj(ctx->pila[(ctx->tam_pila - nargs) - 1]);
        __m128i a4 = pdcrt_xmm_desde_obj(ctx->pila[(ctx->tam_pila - nargs) - 2]);
        __m128i a3 = pdcrt_xmm_desde_obj(ctx->pila[(ctx->tam_pila - nargs) - 3]);
        __m128i a2 = pdcrt_xmm_desde_obj(ctx->pila[(ctx->tam_pila - nargs) - 4]);
        __m128i a1 = pdcrt_xmm_desde_obj(ctx->pila[(ctx->tam_pila - nargs) - 5]);
        pdcrt_eliminar_elementos(ctx, -nargs, 6);
        pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
        nargs -= 6;
        return (*oobj.recv)(ctx, nargs, (pdcrt_k) { .kf = kf, .marco = m },
                            obj, msj, a1, a2, a3, a4, a5, a6);
    }
}

pdcrt_tk pdcrt_llamarnr(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf,
                        int nargs,
                        __m128i obj, __m128i msj,
                        __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5, __m128i a6)
{
    pdcrt_obj oobj = pdcrt_obj_desde_xmm(obj);
    return (*oobj.recv)(ctx, nargs, (pdcrt_k) { .kf = kf, .marco = m }, obj, msj, a1, a2, a3, a4, a5, a6);
}


// pdcrt_k pdcrt_enviar_mensaje(pdcrt_ctx *ctx, pdcrt_marco *m,
//                              const char* msj, size_t tam_msj,
//                              const int* proto, size_t nproto,
//                              pdcrt_kf kf)
// {
//     pdcrt_extender_pila(ctx, m, 1);
//     pdcrt_obj msj_o = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, msj, tam_msj));
//     for(size_t i = 0; i < nproto; i++)
//     {
//         pdcrt_fijar_pila(ctx, ctx->tam_pila - i, ctx->pila[(ctx->tam_pila - i) - 1]);
//     }
//     pdcrt_fijar_pila(ctx, (ctx->tam_pila++) - nproto, msj_o);
//     return pdcrt_enviar_mensaje_obj(ctx, m, proto, nproto, kf);
// }
//
//
// pdcrt_k pdcrt_enviar_mensaje_obj(pdcrt_ctx *ctx, pdcrt_marco *m,
//                                  const int* proto, size_t nproto,
//                                  pdcrt_kf kf)
// {
//     pdcrt_k k = {
//         .kf = kf,
//         .marco = m,
//     };
//
//     // Posición del inicio de los argumentos
//     size_t base = ctx->tam_pila - nproto;
//
//     size_t nargs_expandidos = 0, num_args = 0, variadic_args = 0;
//     for(size_t i = 0; i < nproto; i++)
//     {
//         if(proto && proto[i] == 1)
//         {
//             pdcrt_obj arg = ctx->pila[base + i];
//             pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_ARREGLO);
//             nargs_expandidos += arg.arreglo->longitud;
//             variadic_args += 1;
//         }
//         else
//         {
//             num_args += 1;
//         }
//     }
//
//     pdcrt_extender_pila(ctx, m, nargs_expandidos + variadic_args);
//
//     if(proto)
//     {
//         size_t cur = 0;
//         for(size_t i = 0; i < nproto; i++)
//         {
//             size_t argpos = base + cur;
//             pdcrt_obj arg = ctx->pila[argpos];
//             if(proto[i] == 1)
//             {
//                 // Validado arriba:
//                 // pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_ARREGLO);
//                 size_t len = arg.arreglo->longitud;
//                 size_t por_mover = nproto - (i + 1);
//                 memmove(ctx->pila + argpos + len, ctx->pila + argpos + 1, por_mover * sizeof(pdcrt_obj));
//                 num_args += len;
//                 for(size_t j = 0; j < len; j++)
//                 {
//                     pdcrt_fijar_pila(ctx, argpos + j, arg.arreglo->valores[j]);
//                 }
//                 cur += len;
//             }
//             else
//             {
//                 cur += 1;
//             }
//         }
//     }
//
//     ctx->tam_pila -= variadic_args;
//     ctx->tam_pila += nargs_expandidos;
//     pdcrt_obj t = ctx->pila[(ctx->tam_pila - num_args) - 2];
//     return (*t.recv)(ctx, num_args, k);
// }

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

void pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj v)
{
    (void) m;
    pdcrt_prn_helper(ctx, v);
}

void pdcrt_prnl(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    (void) ctx;
    (void) m;
    puts("");
}

pdcrt_tk pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets)
{
    assert(rets == 1);
    return pdcrt_continuar(ctx, m->k, pdcrt_xmm_desde_obj(pdcrt_sacar(ctx)));
}

pdcrt_tk pdcrt_devolver1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res)
{
    (void) ctx;
    return pdcrt_continuar(ctx, m->k, res);
}

static pdcrt_tk pdcrt_importar_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res);

pdcrt_tk pdcrt_importar(pdcrt_ctx *ctx, pdcrt_marco *m, const char *nombre, size_t tam_nombre, pdcrt_kf kf)
{
    pdcrt_obj onombre = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, nombre, tam_nombre));
    pdcrt_debe_tener_tipo(ctx, ctx->registro_de_espacios_de_nombres, PDCRT_TOBJ_TABLA);
    pdcrt_obj ovalor = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx, m, ctx->registro_de_espacios_de_nombres.tabla, onombre, &ovalor);
    if(contiene)
    {
        return (*kf)(ctx, m, pdcrt_xmm_desde_obj(ovalor));
    }
    else
    {
        pdcrt_debe_tener_tipo(ctx, ctx->registro_de_modulos, PDCRT_TOBJ_TABLA);
        contiene = pdcrt_tabla_en(ctx, m, ctx->registro_de_modulos.tabla, onombre, &ovalor);
        if(!contiene)
        {
            pdcrt_inspeccionar_texto(onombre.texto);
            pdcrt_error(ctx, u8"Módulo no exíste");
        }

        pdcrt_marco *m2 = pdcrt_crear_marco(ctx, 1, 0, 0, (pdcrt_k) { .kf = kf, .marco = m });
        pdcrt_fijar_local(ctx, m2, 0, onombre);
        return pdcrt_llamar0(ctx, m2, &pdcrt_importar_k1,
            pdcrt_xmm_desde_obj(ovalor), pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)));
    }
}

static pdcrt_tk pdcrt_importar_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res)
{
    PDCRT_K(pdcrt_importar_k1);
    pdcrt_obj onombre = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj ovalor = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx, m, ctx->registro_de_espacios_de_nombres.tabla, onombre, &ovalor);
    if(contiene)
    {
        return pdcrt_devolver1(ctx, m, pdcrt_xmm_desde_obj(ovalor));
    }
    else
    {
        pdcrt_error(ctx, u8"Módulo no guardo su espacio de nombres");
    }
}

pdcrt_tk pdcrt_extraerv(pdcrt_ctx *ctx,
                        pdcrt_marco *m,
                        pdcrt_obj mod,
                        const char *nombre,
                        size_t tam_nombre,
                        pdcrt_kf kf)
{
    pdcrt_debe_tener_tipo(ctx, mod, PDCRT_TOBJ_TABLA);
    pdcrt_obj onombre = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, nombre, tam_nombre));
    pdcrt_obj ovalor = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx, m, mod.tabla, onombre, &ovalor);
    if(contiene)
    {
        pdcrt_debe_tener_tipo(ctx, ovalor, PDCRT_TOBJ_ARREGLO);
        if(ovalor.arreglo->longitud != 2)
            pdcrt_error(ctx, u8"Extraerv: el valor de un módulo no es un arreglo de 2 elementos");
        return (*kf)(ctx, m, pdcrt_xmm_desde_obj(ovalor.arreglo->valores[0]));
    }
    else
    {
        pdcrt_inspeccionar_texto(onombre.texto);
        pdcrt_error(ctx, u8"Módulo no contiene el nombre dado");
    }
}

pdcrt_tk pdcrt_agregar_nombre(pdcrt_ctx *ctx,
                              pdcrt_marco *m,
                              pdcrt_obj mod,
                              pdcrt_obj valor,
                              const char *nombre,
                              size_t tam_nombre,
                              bool autoejec,
                              pdcrt_kf kf)
{
    pdcrt_debe_tener_tipo(ctx, mod, PDCRT_TOBJ_TABLA);
    pdcrt_obj onombre = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, nombre, tam_nombre));
    pdcrt_obj arr = pdcrt_objeto_arreglo(pdcrt_crear_arreglo_vacio(ctx, &m, 2));
    if(arr.arreglo->capacidad < 2)
        pdcrt_error(ctx, u8"Arreglo debe tener capacidad mínima de 2");
    arr.arreglo->valores[0] = valor;
    arr.arreglo->valores[1] = pdcrt_objeto_booleano(autoejec);
    arr.arreglo->longitud = 2;
    pdcrt_tabla_fijar(ctx, m, mod.tabla, onombre, arr, true);
    return (*kf)(ctx, m, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
}

pdcrt_tk pdcrt_exportar(pdcrt_ctx *ctx,
                        pdcrt_marco *m,
                        pdcrt_obj mod,
                        const char *modulo,
                        size_t tam_modulo)
{
    pdcrt_debe_tener_tipo(ctx, ctx->registro_de_espacios_de_nombres, PDCRT_TOBJ_TABLA);
    pdcrt_obj omodulo = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, modulo, tam_modulo));
    pdcrt_tabla_fijar(ctx, m, ctx->registro_de_espacios_de_nombres.tabla, omodulo, mod, true);
    return pdcrt_devolver1(ctx, m, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
}

pdcrt_obj pdcrt_obtener_clase_objeto(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    (void) m;
    return ctx->clase_objeto;
}

pdcrt_obj pdcrt_crear_closure_obj(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_f f, size_t num_caps)
{
    pdcrt_closure *c = pdcrt_crear_closure(ctx, m, f, num_caps);
    for(size_t i = 0; i < num_caps; i++)
    {
        size_t ci = (num_caps - 1) - i;
        pdcrt_obj cap = pdcrt_sacar(ctx);
        pdcrt_barrera_de_escritura(ctx, pdcrt_objeto_closure(c), cap);
        c->capturas[ci] = cap;
    }
    return pdcrt_objeto_closure(c);
}

pdcrt_obj pdcrt_crear_closure_obj_1(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_f f,
                                    __m128i c1)
{
    pdcrt_closure *c = pdcrt_crear_closure(ctx, m, f, 1);
    pdcrt_obj cap = pdcrt_obj_desde_xmm(c1);
    pdcrt_barrera_de_escritura(ctx, pdcrt_objeto_closure(c), cap);
    c->capturas[0] = cap;
    return pdcrt_objeto_closure(c);
}

pdcrt_obj pdcrt_mk_closure(pdcrt_ctx *ctx,
                           pdcrt_marco **m,
                           pdcrt_f f,
                           const pdcrt_captura *caps,
                           size_t ncaps)
{
    pdcrt_closure *c = pdcrt_crear_closure(ctx, m, f, ncaps);
    for(size_t i = 0; i < ncaps; i++)
    {
        pdcrt_obj cap = pdcrt_obtener_local(ctx, *m, caps[i].registro);
        pdcrt_barrera_de_escritura(ctx, pdcrt_objeto_closure(c), cap);
        c->capturas[i] = cap;
    }
    return pdcrt_objeto_closure(c);
}

void pdcrt_assert(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj v)
{
    (void) m;
    pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
    if(!v.bval)
    {
        pdcrt_error(ctx, "necesitas fallido");
    }
}
