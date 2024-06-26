#include <stdio.h>
#include <limits.h>

#define PDCRT_INTERNO
#include "pdcrt.h"


static void pdcrt_error(pdcrt_ctx *ctx, const char* msj)
{
    ctx->mensaje_de_error = msj;
    if(!ctx->hay_un_manejador_de_errores)
    {
        fprintf(stderr, "FATAL: %s", msj);
        abort();
    }
    else
    {
        longjmp(ctx->manejador_de_errores, 1);
    }
}

static void pdcrt_enomem(pdcrt_ctx *ctx)
{
    pdcrt_error(ctx, "Sin memoria");
}

static pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o);

static void pdcrt_debe_tener_tipo(pdcrt_ctx *ctx, pdcrt_obj obj, pdcrt_tipo t)
{
    if(pdcrt_tipo_de_obj(obj) != t)
    {
        pdcrt_error(ctx, "Valor de tipo inesperado");
    }
}


static void pdcrt_inspeccionar_pila(pdcrt_ctx *ctx);


static pdcrt_k pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k);

#define pdcrt_objeto_entero(i) ((pdcrt_obj) { .recv = &pdcrt_recv_entero, .ival = (i) })
#define pdcrt_objeto_float(f) ((pdcrt_obj) { .recv = &pdcrt_recv_float, .fval = (f) })
#define pdcrt_objeto_booleano(b) ((pdcrt_obj) { .recv = &pdcrt_recv_booleano, .bval = (b) })
#define pdcrt_objeto_marco(m) ((pdcrt_obj) { .recv = &pdcrt_recv_marco, .marco = (m) })
#define pdcrt_objeto_texto(txt) ((pdcrt_obj) { .recv = &pdcrt_recv_texto, .texto = (txt) })
#define pdcrt_objeto_nulo() ((pdcrt_obj) { .recv = &pdcrt_recv_nulo })


static void pdcrt_gc_marcar(pdcrt_ctx *ctx, pdcrt_obj obj);
static void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx);

static void pdcrt_intenta_invocar_al_recolector(pdcrt_ctx *ctx)
{
    if(ctx->recolector_de_basura_activo && (ctx->cnt % 2 == 0))
    {
        pdcrt_recolectar_basura_simple(ctx);
    }
}

static void pdcrt_activar_recolector_de_basura(pdcrt_ctx *ctx)
{
    ctx->recolector_de_basura_activo = true;
}

static void pdcrt_desactivar_recolector_de_basura(pdcrt_ctx *ctx)
{
    ctx->recolector_de_basura_activo = false;
}

static void *pdcrt_alojar_obj(pdcrt_ctx *ctx, pdcrt_tipo_obj_gc tipo, size_t sz)
{
    pdcrt_intenta_invocar_al_recolector(ctx);
    pdcrt_cabecera_gc *h = malloc(sz);
    assert(h);
    h->generacion = ctx->gc.generacion;
    h->tipo = tipo;
    if(!ctx->gc.primero)
        ctx->gc.primero = h;
    h->siguiente = NULL;
    h->anterior = ctx->gc.ultimo;
    if(h->anterior)
        h->anterior->siguiente = h;
    ctx->gc.ultimo = h;
    return h;
}

#define PDCRT_CABECERA_GC(v) ((pdcrt_cabecera_gc *) (v))

static void pdcrt_gc_marcar_cabecera(pdcrt_ctx *ctx, pdcrt_cabecera_gc *h)
{
    switch(h->tipo)
    {
    case PDCRT_TGC_MARCO:
    {
        if(h->generacion == ctx->gc.generacion)
            break;
        h->generacion = ctx->gc.generacion;
        pdcrt_marco *m = (pdcrt_marco *) h;
        if(m->k.marco)
            pdcrt_gc_marcar_cabecera(ctx, PDCRT_CABECERA_GC(m->k.marco));
        for(size_t i = 0; i < (m->num_locales + m->num_capturas); i++)
            pdcrt_gc_marcar(ctx, m->locales_y_capturas[i]);
        break;
    }
    case PDCRT_TGC_TEXTO:
        if(h->generacion == ctx->gc.generacion)
            break;
        h->generacion = ctx->gc.generacion;
        break;
    }
}

static void pdcrt_gc_marcar(pdcrt_ctx *ctx, pdcrt_obj obj)
{
    pdcrt_cabecera_gc *h;
    switch(pdcrt_tipo_de_obj(obj))
    {
    case PDCRT_TOBJ_NULO:
    case PDCRT_TOBJ_ENTERO:
    case PDCRT_TOBJ_FLOAT:
    case PDCRT_TOBJ_BOOLEANO:
        return;
    case PDCRT_TOBJ_MARCO:
        h = PDCRT_CABECERA_GC(obj.marco);
        break;
    case PDCRT_TOBJ_TEXTO:
        h = PDCRT_CABECERA_GC(obj.texto);
        break;
    }
    pdcrt_gc_marcar_cabecera(ctx, h);
}

static void pdcrt_gc_marcar_todo(pdcrt_ctx *ctx)
{
    for(size_t i = 0; i < ctx->tam_pila; i++)
        pdcrt_gc_marcar(ctx, ctx->pila[i]);

    for(pdcrt_marco *m = ctx->primer_marco_activo; m; m = m->siguiente)
        pdcrt_gc_marcar_cabecera(ctx, PDCRT_CABECERA_GC(m));

    if(ctx->continuacion_actual.marco)
    {
        pdcrt_obj obj = pdcrt_objeto_marco(ctx->continuacion_actual.marco);
        pdcrt_gc_marcar(ctx, obj);
    }

#define PDCRT_X(nombre, _texto)                                         \
    if(ctx->textos_globales.nombre)                                     \
        pdcrt_gc_marcar_cabecera(ctx, PDCRT_CABECERA_GC(ctx->textos_globales.nombre));
    PDCRT_TABLA_TEXTOS(PDCRT_X);
#undef PDCRT_X
}

static void pdcrt_desactivar_marco(pdcrt_ctx *ctx, pdcrt_marco *m);

static void pdcrt_gc_recolectar(pdcrt_ctx *ctx)
{
    for(pdcrt_cabecera_gc *h = ctx->gc.primero; h != NULL;)
    {
        pdcrt_cabecera_gc *s = h->siguiente;
        if(h->generacion != ctx->gc.generacion)
        {
            if(h->anterior)
                h->anterior->siguiente = h->siguiente;
            if(h->siguiente)
                h->siguiente->anterior = h->anterior;
            if(h == ctx->gc.primero)
                ctx->gc.primero = h->siguiente;
            if(h == ctx->gc.ultimo)
                ctx->gc.ultimo = h->anterior;

            if(h->tipo == PDCRT_TGC_MARCO)
            {
                pdcrt_marco *m = (pdcrt_marco *) h;
                assert(m->anterior == NULL && m->siguiente == NULL);
            }
            else if(h->tipo == PDCRT_TGC_TEXTO)
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

            free(h);
        }
        h = s;
    }
}

static void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx)
{
    if(ctx->gc.generacion == INT_MAX)
        ctx->gc.generacion = 0;
    else
        ctx->gc.generacion += 1;

    pdcrt_gc_marcar_todo(ctx);
    pdcrt_gc_recolectar(ctx);
}


static pdcrt_texto* pdcrt_crear_nuevo_texto(pdcrt_ctx *ctx, const char *str, size_t len)
{
    pdcrt_texto *txt = pdcrt_alojar_obj(ctx, PDCRT_TGC_TEXTO, sizeof(pdcrt_texto) + len + 1);
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

static pdcrt_texto* pdcrt_crear_texto(pdcrt_ctx *ctx, const char *str, size_t len)
{
    // Al alojar un texto nuevo (en caso de que no exista) se podría invocar al
    // recolector de basura. En ese caso, la lista de textos sobre la que
    // acabamos de buscar cambiaría, invalidando los índices que tenemos y
    // dañando todo. Para prevenir esto, llamo al recolector ahora mismo y lo
    // desactivo al crear el texto. No debería fallar por memoria ya que de
    // faltar memoria esta llamada debería abrir espacio.
    pdcrt_intenta_invocar_al_recolector(ctx);

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
        pdcrt_texto **textos = realloc(ctx->textos, sizeof(pdcrt_texto *) * ncap);
        if(!textos)
            pdcrt_enomem(ctx);
        ctx->textos = textos;
        ctx->cap_textos = ncap;
    }

    const size_t exp_ind = lo;
    pdcrt_desactivar_recolector_de_basura(ctx);
    pdcrt_texto *txt = pdcrt_crear_nuevo_texto(ctx, str, len);
    pdcrt_activar_recolector_de_basura(ctx);
    for(ssize_t i = ctx->tam_textos - 1; i >= (ssize_t) exp_ind; i--)
    {
        ctx->textos[i + 1] = ctx->textos[i];
    }
    ctx->textos[exp_ind] = txt; // #1
    ctx->tam_textos += 1;
    return txt;
}

#define pdcrt_crear_texto_desde_cstr(ctx, cstr) \
    pdcrt_crear_texto(ctx, cstr, sizeof(cstr) - 1)

static bool pdcrt_comparar_textos(pdcrt_texto *a, pdcrt_texto *b)
{
    return pdcrt_comparar_str(a->contenido, a->longitud,
                              b->contenido, b->longitud) == 0;
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

// Sacado de
// <https://stackoverflow.com/questions/64842669/how-to-test-if-a-target-has-twos-complement-integers-with-the-c-preprocessor>
_Static_assert((-1 & 3) == 3,
               u8"tu compilador debe implementar los enteros como números en complemento a 2");

#if !__STDC_IEC_559__ || !__STDC_IEC_60559_BFP__
#error tu compilador debe implementar float/double/long double como números IEEE-754 / IEC 60559
#endif

_Static_assert(FLT_RADIX == 2, "float/double/long double deben ser binarios");
_Static_assert(sizeof(float) == 4, "float debe ser de 32 bits");
_Static_assert(sizeof(double) == 8, "double debe ser de 64 bits");

// La cantidad de bits en un entero.
#define PDCRT_ENTERO_BITS (sizeof(pdcrt_entero) * 8)

enum pdcrt_comparacion
{
    //                              ABC
    PDCRT_MENOR_QUE       = 1,  // 0001
    PDCRT_MENOR_O_IGUAL_A = 3,  // 0011
    PDCRT_MAYOR_QUE       = 4,  // 0100
    PDCRT_MAYOR_O_IGUAL_A = 6,  // 0110
    PDCRT_IGUAL_A         = 10  // 1010
    // El bit A es si es "mayor que", el bit B es el bit "igual a", el bit C es
    // el bit "menor que".
};

static inline bool pdcrt_es_menor_que(enum pdcrt_comparacion op)
{
    return op & 1;
}

static inline bool pdcrt_es_igual_a(enum pdcrt_comparacion op)
{
    return op & 2;
}

static inline bool pdcrt_es_mayor_que(enum pdcrt_comparacion op)
{
    return op & 4;
}

static bool pdcrt_comparar_floats(pdcrt_float a, pdcrt_float b, enum pdcrt_comparacion op)
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

static bool pdcrt_comparar_enteros(pdcrt_entero a, pdcrt_entero b, enum pdcrt_comparacion op)
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

static bool pdcrt_comparar_entero_y_float(pdcrt_entero e, pdcrt_float f, enum pdcrt_comparacion op)
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
    // del punto decimal (¿punto decimal? ¿o punto binario?).
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

        // Este cast es seguro (no hará overflow) ya que sabemos que f tiene la
        // misma cantidad de bits *en su parte entera*.
        return pdcrt_comparar_enteros(e, (pdcrt_entero) f_ent, op);
    }
}


#define PDCRT_CALC_INICIO() (ctx->tam_pila - args) - 2;
// 2 elementos: yo + el mensaje.
#define PDCRT_SACAR_PRELUDIO() pdcrt_eliminar_elementos(ctx, inic, 2 + args);

static pdcrt_k pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): comoTexto no acepta argumentos");
        static_assert(sizeof(pdcrt_entero) <= 64, "pdcrt_entero no debe tener más de 64 bits");
        char texto[21]; // 64 bits => máx. 20 caracteres + '\0'
        int len = snprintf(texto, sizeof(texto), "%" PDCRT_ENTERO_PRId, yo.ival);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, texto, len));
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar(ctx, txt);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.sumar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_mas))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al sumar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_entero(ctx, yo.ival + otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, ((pdcrt_float) yo.ival) + otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden sumar dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.restar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_menos))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al restar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_entero(ctx, yo.ival - otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, ((pdcrt_float) yo.ival) - otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden restar dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_entero(ctx, yo.ival * otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, ((pdcrt_float) yo.ival) * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al dividir se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, ((pdcrt_float) yo.ival) / ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, ((pdcrt_float) yo.ival) / otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden dividir dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, yo.ival == arg.ival);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, pdcrt_comparar_entero_y_float(yo.ival, arg.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, false);
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): operador_no= / dístintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, yo.ival != arg.ival);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, !pdcrt_comparar_entero_y_float(yo.ival, arg.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, true);
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
#define PDCRT_COMPARAR_ENTERO(m, opm, ms, opms, cmp, op)                \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (entero): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_extender_pila(ctx, 1);                                    \
        pdcrt_obj arg = ctx->pila[argp];                                \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                 \
            pdcrt_empujar_booleano(ctx, yo.ival op arg.ival);            \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)             \
            pdcrt_empujar_booleano(ctx, pdcrt_comparar_entero_y_float(yo.ival, arg.fval, cmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (entero): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return k.kf(ctx, k.marco);                                      \
    }
    PDCRT_COMPARAR_ENTERO(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MENOR_QUE, <)
    PDCRT_COMPARAR_ENTERO(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MAYOR_QUE, >)
    PDCRT_COMPARAR_ENTERO(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MENOR_O_IGUAL_A, <=)
    PDCRT_COMPARAR_ENTERO(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MAYOR_O_IGUAL_A, >=)
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): negar no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_entero(ctx, -yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): piso no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_entero(ctx, yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): techo no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_entero(ctx, yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): truncar no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_entero(ctx, yo.ival);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): comoTexto no acepta argumentos");
        char texto[30];
        int len = snprintf(texto, sizeof(texto), "%" PDCRT_FLOAT_PRIg, yo.fval);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, texto, len));
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar(ctx, txt);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.sumar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_mas))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al sumar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, yo.fval + ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, yo.fval + otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden sumar dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.restar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_menos))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al restar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, yo.fval - ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, yo.fval - otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden restar dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, yo.fval * ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, yo.fval * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al dividir se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, yo.fval / ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, yo.fval / otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden dividir dos números");
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, yo.fval == arg.fval);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, pdcrt_comparar_entero_y_float(arg.ival, yo.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, false);
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_no= / dístintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, yo.fval != arg.fval);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, !pdcrt_comparar_entero_y_float(arg.ival, yo.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, true);
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
#define PDCRT_COMPARAR_FLOAT(m, opm, ms, opms, rcmp, op)                \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (float): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_extender_pila(ctx, 1);                                    \
        pdcrt_obj arg = ctx->pila[argp];                                \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                  \
            pdcrt_empujar_booleano(ctx, yo.fval op arg.fval);           \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)            \
            pdcrt_empujar_booleano(ctx, pdcrt_comparar_entero_y_float(arg.ival, yo.fval, rcmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (float): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return k.kf(ctx, k.marco);                                      \
    }
    PDCRT_COMPARAR_FLOAT(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MAYOR_O_IGUAL_A, <)
    PDCRT_COMPARAR_FLOAT(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MENOR_O_IGUAL_A, >)
    PDCRT_COMPARAR_FLOAT(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MAYOR_QUE, <=)
    PDCRT_COMPARAR_FLOAT(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MENOR_QUE, >=)
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): negar no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_float(ctx, -yo.fval);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): piso no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_float(ctx, PDCRT_FLOAT_FLOOR(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): techo no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_float(ctx, PDCRT_FLOAT_CEIL(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): truncar no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_float(ctx, PDCRT_FLOAT_TRUNC(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Booleano: comoTexto no acepta argumentos");
        pdcrt_extender_pila(ctx, 1);
        if(yo.bval)
            pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.verdadero));
        else
            pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.falso));
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
        {
            pdcrt_empujar_booleano(ctx, false);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, yo.bval == arg.bval);
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_no= / dístintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
        {
            pdcrt_empujar_booleano(ctx, true);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, yo.bval != arg.bval);
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    assert(0 && "sin implementar");
}

static bool pdcrt_es_digito(char c)
{
    return c >= '0' && c <= '9';
}

static pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.concatenar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: concatenar necesita 1 argumento");
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_TEXTO);
        size_t bufflen = yo.texto->longitud + arg.texto->longitud;
        char *buff = malloc(bufflen);
        if(!buff)
            pdcrt_enomem(ctx);
        memcpy(buff, yo.texto->contenido, yo.texto->longitud);
        memcpy(buff + yo.texto->longitud, arg.texto->contenido, arg.texto->longitud);
        pdcrt_extender_pila(ctx, 1);
        pdcrt_texto *res = pdcrt_crear_texto(ctx, buff, bufflen);
        free(buff);
        pdcrt_empujar(ctx, pdcrt_objeto_texto(res));
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        {
            pdcrt_empujar_booleano(ctx, false);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, pdcrt_comparar_textos(yo.texto, arg.texto));
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_no= / dístintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        {
            pdcrt_empujar_booleano(ctx, true);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, !pdcrt_comparar_textos(yo.texto, arg.texto));
        }
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_numero_entero))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroEntero no necesita argumentos");
        pdcrt_extender_pila(ctx, 1);
        const char* s = yo.texto->contenido;
        if(*s == '-')
            s += 1;
        for(; *s; s++)
            if(!pdcrt_es_digito(*s))
                goto error_como_entero;

        pdcrt_entero i;
        i = strtoll(yo.texto->contenido, NULL, 10);
        pdcrt_empujar_entero(ctx, i);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    error_como_entero:
        pdcrt_empujar_nulo(ctx);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_numero_real))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroReal no necesita argumentos");
        pdcrt_extender_pila(ctx, 1);
        const char* s = yo.texto->contenido;
        if(*s == '-')
            s += 1;
        bool dot = false;
        for(; *s; s++)
            if(*s == '.' && !dot)
                dot = true;
            else if(*s == '.' && dot)
                goto error_como_real;
            else if(!pdcrt_es_digito(*s))
                goto error_como_real;

        pdcrt_float f;
        f = strtold(yo.texto->contenido, NULL);
        pdcrt_empujar_float(ctx, f);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    error_como_real:
        pdcrt_empujar_nulo(ctx);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: longitud no necesita argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_entero(ctx, yo.texto->longitud);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: en necesita 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: en necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= yo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #en");
        pdcrt_empujar_texto(ctx, yo.texto->contenido + i, 1);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_NULO);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_no= / dístintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_NULO);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Nulo: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, 1);
        pdcrt_empujar_texto(ctx, "NULO", 4);
        PDCRT_SACAR_PRELUDIO();
        return k.kf(ctx, k.marco);
    }

    assert(0 && "sin implementar");
}


static pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o)
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
    else
    {
        assert(0 && "inalcanzable");
    }
}


pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, size_t locales, size_t capturas, int args, pdcrt_k k)
{
    pdcrt_marco *m = pdcrt_alojar_obj(ctx, PDCRT_TGC_MARCO, sizeof(pdcrt_marco) + sizeof(pdcrt_obj) * (locales + capturas));
    if(!m)
        pdcrt_enomem(ctx);
    m->args = args;
    m->k = k;
    m->num_locales = locales;
    m->num_capturas = capturas;
    size_t top = ctx->tam_pila;
    for(size_t i = 0; i < capturas; i++)
    {
        m->locales_y_capturas[locales + i] = ctx->pila[(top - capturas) + i];
    }
    ctx->tam_pila -= capturas;

    if(!ctx->primer_marco_activo)
        ctx->primer_marco_activo = m;
    m->siguiente = NULL;
    m->anterior = ctx->ultimo_marco_activo;
    if(m->anterior)
        m->anterior->siguiente = m;
    ctx->ultimo_marco_activo = m;

    return m;
}

static void pdcrt_desactivar_marco(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    if(m->anterior)
        m->anterior->siguiente = m->siguiente;
    if(m->siguiente)
        m->siguiente->anterior = m->anterior;
    if(m == ctx->primer_marco_activo)
        ctx->primer_marco_activo = ctx->primer_marco_activo->siguiente;
    if(m == ctx->ultimo_marco_activo)
        ctx->ultimo_marco_activo = ctx->ultimo_marco_activo->anterior;
    m->anterior = m->siguiente = NULL;
}


pdcrt_ctx *pdcrt_crear_contexto(void)
{
    pdcrt_ctx *ctx = malloc(sizeof(pdcrt_ctx));
    assert(ctx);

    ctx->recolector_de_basura_activo = true;

    ctx->tam_pila = 0;
    ctx->pila = NULL;
    ctx->cap_pila = 0;

    ctx->gc.primero = ctx->gc.ultimo = NULL;
    ctx->gc.generacion = 0;
    ctx->primer_marco_activo = ctx->ultimo_marco_activo = NULL;
    ctx->cnt = 0;

    ctx->tam_textos = ctx->cap_textos = 0;
    ctx->textos = NULL;

    volatile int x;
    ctx->inicio_del_stack = (uintptr_t) &x;
    ctx->tam_stack = 1 * 1024 * 1024; // 1 MiB

    ctx->hay_un_manejador_de_errores = false;
    ctx->mensaje_de_error = NULL;

    ctx->hay_una_salida_del_trampolin = false;
    ctx->continuacion_actual = (pdcrt_k) { .kf = NULL, .marco = NULL };

#define PDCRT_X(nombre, texto) ctx->textos_globales.nombre = NULL;
    PDCRT_TABLA_TEXTOS(PDCRT_X);
#undef PDCRT_X
#define PDCRT_X(nombre, texto) ctx->textos_globales.nombre = pdcrt_crear_texto_desde_cstr(ctx, texto);
    PDCRT_TABLA_TEXTOS(PDCRT_X);
#undef PDCRT_X

    return ctx;
}

void pdcrt_cerrar_contexto(pdcrt_ctx *ctx)
{
    for(pdcrt_cabecera_gc *h = ctx->gc.primero; h != NULL;)
    {
        pdcrt_cabecera_gc *s = h->siguiente;
        free(h);
        h = s;
    }

    free(ctx->textos);
    free(ctx->pila);
    free(ctx);
}


void pdcrt_extender_pila(pdcrt_ctx *ctx, size_t num_elem)
{
    if((num_elem + ctx->tam_pila) > ctx->cap_pila)
    {
        size_t nueva_cap = num_elem + ctx->tam_pila;
        void *nueva_pila = realloc(ctx->pila, sizeof(pdcrt_obj) * nueva_cap);
        if(!nueva_pila)
            pdcrt_enomem(ctx);
        ctx->pila = nueva_pila;
        ctx->cap_pila = nueva_cap;
    }
}

void pdcrt_empujar_entero(pdcrt_ctx *ctx, pdcrt_entero i)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_entero(i));
}

void pdcrt_empujar_booleano(pdcrt_ctx *ctx, bool v)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_booleano(v));
}

void pdcrt_empujar_float(pdcrt_ctx *ctx, pdcrt_float f)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_float(f));
}

void pdcrt_empujar_espacio_de_nombres(pdcrt_ctx *ctx)
{
    // TODO
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_entero(1));
}

void pdcrt_empujar_texto(pdcrt_ctx *ctx, const char *str, size_t len)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_texto *txt = pdcrt_crear_texto(ctx, str, len);
    pdcrt_empujar(ctx, pdcrt_objeto_texto(txt));
}

void pdcrt_empujar_nulo(pdcrt_ctx *ctx)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_nulo());
}

static inline size_t pdcrt_stp_a_pos(pdcrt_ctx *ctx, pdcrt_stp i)
{
    if(i < 0)
        return ctx->tam_pila + i;
    else
        return i;
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

size_t pdcrt_obtener_texto(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok, char *buffer)
{
    size_t p = pdcrt_stp_a_pos(ctx, i);
    pdcrt_obj o = ctx->pila[p];
    if(pdcrt_tipo_de_obj(o) == PDCRT_TOBJ_TEXTO)
    {
        *ok = true;
        if(buffer)
        {
            memcpy(buffer, o.texto->contenido, o.texto->longitud);
        }
        return o.texto->longitud;
    }
    else
    {
        *ok = false;
        return 0;
    }
}

void pdcrt_negar(pdcrt_ctx *ctx)
{
    pdcrt_obj cima = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(cima) != PDCRT_TOBJ_BOOLEANO)
        pdcrt_error(ctx, "Valor no booleano pasado al operador `no`");
    pdcrt_sacar(ctx);
    pdcrt_empujar(ctx, pdcrt_objeto_booleano(!cima.bval));
}

void pdcrt_eliminar_elemento(pdcrt_ctx *ctx, pdcrt_stp pos)
{
    size_t rpos = pdcrt_stp_a_pos(ctx, pos);
    size_t n = (ctx->tam_pila - rpos) - 1;
    if(n > 0)
        memmove(ctx->pila + rpos, ctx->pila + rpos + 1, n * sizeof(pdcrt_obj));
    ctx->tam_pila -= 1;
}

void pdcrt_eliminar_elementos(pdcrt_ctx *ctx, pdcrt_stp inic, size_t cnt)
{
    for(; cnt > 0; cnt--)
    {
        pdcrt_eliminar_elemento(ctx, inic);
    }
}

bool pdcrt_saltar_condicional(pdcrt_ctx *ctx)
{
    pdcrt_obj o = pdcrt_sacar(ctx);
    if(pdcrt_tipo_de_obj(o) != PDCRT_TOBJ_BOOLEANO)
    {
        pdcrt_error(ctx, "Se esperaba un objeto de tipo Booleano");
    }
    return o.bval;
}

pdcrt_k pdcrt_saltar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf)
{
    return (pdcrt_k) {
        .kf = kf,
        .marco = m,
    };
}

pdcrt_k pdcrt_enviar_mensaje(pdcrt_ctx *ctx, pdcrt_marco *m,
                             const char* msj, size_t tam_msj,
                             const int* proto, size_t nproto,
                             pdcrt_kf kf)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_k k = {
        .kf = kf,
        .marco = m,
    };
    pdcrt_obj msj_o = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, msj, tam_msj));
    for(ssize_t i = nproto - 1; i >= 0; i--)
    {
        assert(proto[i] == 0);
        ctx->pila[ctx->tam_pila - i] = ctx->pila[(ctx->tam_pila - i) - 1];
    }
    ctx->pila[(ctx->tam_pila++) - nproto] = msj_o;
    pdcrt_obj t = ctx->pila[(ctx->tam_pila - nproto) - 2];
    return (*t.recv)(ctx, nproto, k);
}

pdcrt_k pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf)
{
    pdcrt_obj o = pdcrt_sacar(ctx);
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
        printf("%" PDCRT_FLOAT_PRIf, o.fval);
        break;
    case PDCRT_TOBJ_TEXTO:
        for(size_t i = 0; i < o.texto->longitud; i++)
        {
            putchar(o.texto->contenido[i]);
        }
        break;
    case PDCRT_TOBJ_MARCO:
        printf("Marco %p", (void *) o.marco);
        break;
    }
    return (pdcrt_k) {
        .kf = kf,
        .marco = m,
    };
}

void pdcrt_prnl(pdcrt_ctx *ctx)
{
    (void) ctx;
    puts("");
}

pdcrt_k pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets)
{
    (void) ctx;
    pdcrt_desactivar_marco(ctx, m);
    assert(rets == 1);
    return m->k;
}

void pdcrt_agregar_nombre(pdcrt_ctx *ctx, const char *nombre, size_t tam_nombre, bool autoejec)
{
    // TODO
    (void) pdcrt_sacar(ctx);
}

pdcrt_k pdcrt_exportar(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    // TODO
    (void) ctx;
    pdcrt_desactivar_marco(ctx, m);
    return m->k;
}

bool pdcrt_stack_lleno(pdcrt_ctx *ctx)
{
    volatile int x;
    uintptr_t xp = (uintptr_t) &x;
    size_t diff;
    if(xp > ctx->inicio_del_stack)
        diff = xp - ctx->inicio_del_stack;
    else
        diff = ctx->inicio_del_stack - xp;
    return diff > ctx->tam_stack;
}

static pdcrt_k pdcrt_continuacion_de_ejecutar(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    assert(ctx->hay_una_salida_del_trampolin);
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
    memcpy((jmp_buf *) &viejo_s, &ctx->salir_del_trampolin, sizeof(jmp_buf));
    memcpy((jmp_buf *) &viejo_e, &ctx->manejador_de_errores, sizeof(jmp_buf));
    volatile bool habia_s = ctx->hay_una_salida_del_trampolin;
    volatile bool habia_e = ctx->hay_un_manejador_de_errores;

#define CLEANUP()                                                       \
    do                                                                  \
    {                                                                   \
        memcpy(&ctx->salir_del_trampolin, (jmp_buf *) &viejo_s, sizeof(jmp_buf)); \
        memcpy(&ctx->manejador_de_errores, (jmp_buf *) &viejo_e, sizeof(jmp_buf)); \
        ctx->hay_una_salida_del_trampolin = habia_s;                    \
        ctx->hay_un_manejador_de_errores = habia_e;                     \
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
    ctx->continuacion_actual = f(ctx, args, k);
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

int pdcrt_main(int argc, char **argv, pdcrt_f f)
{
    (void) argc;
    (void) argv;
    pdcrt_ctx *ctx = pdcrt_crear_contexto();
    pdcrt_ejecutar(ctx, 0, f);
    pdcrt_cerrar_contexto(ctx);
    return 0;
}

static void pdcrt_inspeccionar_pila(pdcrt_ctx *ctx)
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
            printf("float %" PDCRT_FLOAT_PRIf "\n", o.fval);
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
        default:
            printf("unk %d\n", pdcrt_tipo_de_obj(o));
            break;
        }
    }
}
