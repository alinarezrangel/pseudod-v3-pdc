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
    pdcrt_obj obj = pdcrt_objeto_caja(pdcrt_crear_caja(ctx, m));
    pdcrt_empujar(ctx, obj);
}

void pdcrt_empujar_tabla_vacia(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad)
{
    pdcrt_extender_pila(ctx, *m, 1);
    pdcrt_obj obj = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, m, capacidad));
    pdcrt_empujar(ctx, obj);
}

pdcrt_obj pdcrt_crear_espacio_de_nombres_cons(pdcrt_ctx *ctx, pdcrt_marco **m)
{
    return pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, m, 32));
}

pdcrt_obj pdcrt_obtener_objeto_runtime(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    (void) ctx;
    (void) m;
    return pdcrt_objeto_runtime();
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
    pdcrt_obj c = pdcrt_objeto_caja(pdcrt_crear_caja(ctx, m));
    pdcrt_fijar_caja(ctx, c, pdcrt_cima(ctx));
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
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar_arreglo_vacio(ctx, &m, num_args_variadic);
        for(int i = 0; i < num_args_variadic; i++)
        {
            // -1 por el arreglo que acabamos de empujar
            pdcrt_obj val = ctx->pila[ctx->tam_pila - m->args - 1 + prefijo + i];
            pdcrt_empujar(ctx, val);
            pdcrt_arreglo_empujar_al_final(ctx, m, -2);
        }
        pdcrt_obj var = pdcrt_sacar(ctx);
        m->registros[p->params[p->base.idc_variadic].reg] = var;
    }

    ctx->tam_pila -= m->args; // saca todos los argumentos
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
    return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, NULL, 0, &pdcrt_importar_k4);
}

static pdcrt_k pdcrt_importar_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_importar_k4);
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

pdcrt_k pdcrt_extraerv(pdcrt_ctx *ctx,
                       pdcrt_marco *m,
                       pdcrt_obj mod,
                       const char *nombre,
                       size_t tam_nombre,
                       pdcrt_kf kf)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    pdcrt_extender_pila(ctx, m, 4);
    pdcrt_empujar(ctx, mod);
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
    pdcrt_insertar(ctx, -3);
    // [val, mod, valop]
    bool ok;
    pdcrt_kf *kf = pdcrt_obtener_valop(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "error interno: valop esperado");
    (void) pdcrt_sacar(ctx);
    (void) pdcrt_sacar(ctx);
    // [val]
    return (*kf)(ctx, m);
}

static pdcrt_k pdcrt_agregar_nombre_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_agregar_nombre(pdcrt_ctx *ctx,
                             pdcrt_marco *m,
                             pdcrt_obj mod,
                             pdcrt_obj valor,
                             const char *nombre,
                             size_t tam_nombre,
                             bool autoejec,
                             pdcrt_kf kf)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    pdcrt_extender_pila(ctx, m, 7);

    pdcrt_empujar(ctx, mod);
    pdcrt_empujar(ctx, valor);
    // [mod, valor]
    pdcrt_empujar(ctx, mod);
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
    (void) pdcrt_sacar(ctx);
    // []
    return (*kf)(ctx, m);
}

static pdcrt_k pdcrt_exportar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_exportar(pdcrt_ctx *ctx,
                       pdcrt_marco *m,
                       pdcrt_obj mod,
                       const char *modulo,
                       size_t tam_modulo)
{
    // Esta función asume que al llamarla, no existe ninguna referencia al
    // marco `m` excepto por el parámetro `m`.

    pdcrt_extender_pila(ctx, m, 4);
    pdcrt_empujar(ctx, mod);
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

pdcrt_obj pdcrt_obtener_clase_objeto(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    (void) m;
    return ctx->clase_objeto;
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
