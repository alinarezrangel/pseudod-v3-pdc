#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <stddef.h>
#include <stdarg.h>

#include "pdcrt-plataforma.h"
#include "pdcrt_hash.h"

#define PDCRT_INTERNO
#include "pdcrt.h"
#include "pdcrt_ops.h"


int pdcrt_time(struct timespec *out)
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

#define PDCRT_ABS(n) ((n) < 0 ? -(n) : (n))

void pdcrt_diferencia(struct timespec *primero, struct timespec *segundo, pdcrt_timediff *res)
{
    long dif_ns = segundo->tv_nsec - primero->tv_nsec;
    long dif_us = (dif_ns / 1000L) % 1000;
    long dif_ms = (dif_ns / 1000000L) % 1000;
    res->dif_s = segundo->tv_sec - primero->tv_sec;
    res->dif_ms = PDCRT_ABS(dif_ms);
    res->dif_us = PDCRT_ABS(dif_us);
    res->dif_ns = PDCRT_ABS(dif_ns % 1000);
}

void pdcrt_formatear_bytes(char *buffer, size_t bytes)
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


_Noreturn void pdcrt_error(pdcrt_ctx *ctx, const char* msj)
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

_Noreturn void pdcrt_enomem(pdcrt_ctx *ctx)
{
    pdcrt_error(ctx, "Sin memoria");
}

void pdcrt_debe_tener_tipo(pdcrt_ctx *ctx, pdcrt_obj obj, pdcrt_tipo t)
{
    if(pdcrt_tipo_de_obj(obj) != t)
    {
        pdcrt_error(ctx, "Valor de tipo inesperado");
    }
}

extern _Noreturn pdcrt_k pdcrt_trampolin(pdcrt_ctx *ctx, pdcrt_k k);
extern pdcrt_k pdcrt_continuar(pdcrt_ctx *ctx, pdcrt_k k);


extern bool pdcrt_es_menor_que(enum pdcrt_comparacion op);
extern bool pdcrt_es_igual_a(enum pdcrt_comparacion op);
extern bool pdcrt_es_mayor_que(enum pdcrt_comparacion op);

bool pdcrt_es_primitivo(pdcrt_ctx *ctx, pdcrt_obj o)
{
    (void) ctx;

    switch(pdcrt_tipo_de_obj(o))
    {
    case PDCRT_TOBJ_ENTERO:
    case PDCRT_TOBJ_FLOAT:
    case PDCRT_TOBJ_TEXTO:
    case PDCRT_TOBJ_BOOLEANO:
    case PDCRT_TOBJ_NULO:
        return true;
    default:
        return false;
    }
}

pdcrt_entero pdcrt_hash(pdcrt_ctx *ctx, pdcrt_obj o)
{
    switch(pdcrt_tipo_de_obj(o))
    {
        case PDCRT_TOBJ_ENTERO:
            return pdcrt_hash_entero(o.ival);
        case PDCRT_TOBJ_FLOAT:
            return pdcrt_hash_float(o.fval);
        case PDCRT_TOBJ_TEXTO:
            return pdcrt_hash_bytes(o.texto->contenido, o.texto->longitud);
        case PDCRT_TOBJ_BOOLEANO:
            return o.bval ? 0 : 1;
        case PDCRT_TOBJ_NULO:
            return 2;
        default:
            pdcrt_error(ctx, "No se puede hashear objeto de ese tipo");
    }
}

bool pdcrt_igualdad(pdcrt_ctx *ctx, pdcrt_obj a, pdcrt_obj b)
{
    (void) ctx;

    pdcrt_tipo ta = pdcrt_tipo_de_obj(a);
    pdcrt_tipo tb = pdcrt_tipo_de_obj(b);

    if(ta == tb)
    {
        switch(ta)
        {
        case PDCRT_TOBJ_ENTERO:
            return a.ival == b.ival;
        case PDCRT_TOBJ_FLOAT:
            return a.fval == b.fval;
        case PDCRT_TOBJ_BOOLEANO:
            return a.bval == b.bval;
        case PDCRT_TOBJ_NULO:
            return true;
        case PDCRT_TOBJ_TEXTO:
            return pdcrt_comparar_textos(a.texto, b.texto);
        default:
            return false;
        }
    }
    else if(ta == PDCRT_TOBJ_ENTERO && tb == PDCRT_TOBJ_FLOAT)
    {
        return pdcrt_comparar_entero_y_float(a.ival, b.fval, PDCRT_IGUAL_A);
    }
    else if(ta == PDCRT_TOBJ_FLOAT && tb == PDCRT_TOBJ_ENTERO)
    {
        return pdcrt_comparar_entero_y_float(b.ival, a.fval, PDCRT_IGUAL_A);
    }
    else
    {
        return false;
    }
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

pdcrt_texto* pdcrt_crear_texto(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str, size_t len)
{
    // Al alojar un texto nuevo (en caso de que no exista) se podría invocar al
    // recolector de basura. En ese caso, la lista de textos sobre la que
    // acabamos de buscar cambiaría, invalidando los índices que tenemos y
    // dañando todo. Para prevenir esto, llamo al recolector ahora mismo y lo
    // desactivo al crear el texto. No debería fallar por memoria, ya que de
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

bool pdcrt_comparar_textos(pdcrt_texto *a, pdcrt_texto *b)
{
    return a == b;
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

bool pdcrt_comparar_floats(pdcrt_float a, pdcrt_float b, enum pdcrt_comparacion op)
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

bool pdcrt_comparar_enteros(pdcrt_entero a, pdcrt_entero b, enum pdcrt_comparacion op)
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

bool pdcrt_comparar_entero_y_float(pdcrt_entero e, pdcrt_float f, enum pdcrt_comparacion op)
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



pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o)
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

    ctx->hay_un_continuar = false;

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

pdcrt_obj pdcrt_convertir_a_espacio_de_nombres(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj mod)
{
    pdcrt_debe_tener_tipo(ctx, mod, PDCRT_TOBJ_TABLA);
    return pdcrt_objeto_espacio_de_nombres(mod.tabla);
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

bool pdcrt_son_identicos(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj x, pdcrt_obj y)
{
    (void) ctx;
    (void) m;
    if(pdcrt_tipo_de_obj(x) != pdcrt_tipo_de_obj(y))
    {
        return false;
    }
    else
    {
        // HACK: Debería haber una forma de comparar los "datos" de dos
        //  objetos, pero como no hay comparamos en cambio sus enteros.
        return x.ival == y.ival;
    }
}

pdcrt_obj pdcrt_obtener_espacio_de_nombres_del_runtime(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    return ctx->espacio_de_nombres_runtime;
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
    volatile jmp_buf viejo_c;
    memcpy((jmp_buf *) &viejo_s, &ctx->salir_del_trampolin, sizeof(jmp_buf));
    memcpy((jmp_buf *) &viejo_e, &ctx->manejador_de_errores, sizeof(jmp_buf));
    memcpy((jmp_buf *) &viejo_c, &ctx->continuar, sizeof(jmp_buf));
    volatile bool habia_s = ctx->hay_una_salida_del_trampolin;
    volatile bool habia_e = ctx->hay_un_manejador_de_errores;
    volatile bool habia_c = ctx->hay_un_continuar;
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
        memcpy(&ctx->continuar, (jmp_buf *) &viejo_c, sizeof(jmp_buf)); \
        ctx->hay_una_salida_del_trampolin = habia_s;                    \
        ctx->hay_un_manejador_de_errores = habia_e;                     \
        ctx->hay_un_continuar = habia_c;                                \
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

    if(setjmp(ctx->continuar) == 0)
    {
        ctx->hay_un_continuar = true;
        ctx->continuacion_actual = f(ctx, args, k);
    }

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
    ctx->espacio_de_nombres_runtime = pdcrt_convertir_a_espacio_de_nombres(ctx, m, pdcrt_sacar(ctx));
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
    // [nulo]
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
