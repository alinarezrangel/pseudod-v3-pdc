#include <stdio.h>
#include <limits.h>

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


static pdcrt_k pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k);

#define pdcrt_objeto_entero(i) ((pdcrt_obj) { .recv = &pdcrt_recv_entero, .ival = (i) })
#define pdcrt_objeto_float(f) ((pdcrt_obj) { .recv = &pdcrt_recv_float, .fval = (f) })
#define pdcrt_objeto_booleano(b) ((pdcrt_obj) { .recv = &pdcrt_recv_booleano, .bval = (b) })
#define pdcrt_objeto_marco(m) ((pdcrt_obj) { .recv = &pdcrt_recv_marco, .marco = (m) })
#define pdcrt_objeto_texto(txt) ((pdcrt_obj) { .recv = &pdcrt_recv_texto, .texto = (txt) })


static void pdcrt_gc_marcar(pdcrt_ctx *ctx, pdcrt_obj obj);
static void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx);

static void *pdcrt_alojar_obj(pdcrt_ctx *ctx, pdcrt_tipo_obj_gc tipo, size_t sz)
{
    if(ctx->cnt % 100 == 0)
    {
        pdcrt_recolectar_basura_simple(ctx);
    }

    pdcrt_cabecera_gc *h = malloc(sz);
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

static pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o);

static void pdcrt_gc_marcar(pdcrt_ctx *ctx, pdcrt_obj obj)
{
    pdcrt_cabecera_gc *h;
    switch(pdcrt_tipo_de_obj(obj))
    {
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
    if(ctx->continuacion_actual.marco)
    {
        pdcrt_obj obj = pdcrt_objeto_marco(ctx->continuacion_actual.marco);
        pdcrt_gc_marcar(ctx, obj);
    }
}

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
                ctx->gc.primero = NULL;
            if(h == ctx->gc.ultimo)
                ctx->gc.ultimo = NULL;

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
    pdcrt_texto *txt = pdcrt_crear_nuevo_texto(ctx, str, len);
    for(ssize_t i = ctx->tam_textos; i > (ssize_t) exp_ind; i--)
    {
        ctx->textos[i + 1] = ctx->textos[i];
    }
    ctx->textos[exp_ind] = txt;
    ctx->tam_textos += 1;
    return txt;
}

static int pdcrt_comparar_textos(pdcrt_texto *a, pdcrt_texto *b)
{
    return pdcrt_comparar_str(a->contenido, a->longitud,
                              b->contenido, b->longitud);
}


static pdcrt_k pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
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
    return m;
}


pdcrt_ctx *pdcrt_crear_contexto(void)
{
    pdcrt_ctx *ctx = malloc(sizeof(pdcrt_ctx));

    ctx->tam_pila = 0;
    ctx->pila = NULL;
    ctx->cap_pila = 0;

    ctx->gc.primero = ctx->gc.ultimo = NULL;
    ctx->gc.generacion = 0;
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

static void pdcrt_extender_pila(pdcrt_ctx *ctx, size_t num_elem)
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

#define pdcrt_empujar(ctx, v) (ctx)->pila[(ctx)->tam_pila++] = (v)
#define pdcrt_sacar(ctx) (ctx)->pila[--(ctx)->tam_pila]

void pdcrt_empujar_entero(pdcrt_ctx *ctx, pdcrt_entero i)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_empujar(ctx, pdcrt_objeto_entero(i));
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
                             const char* msj,
                             const int* proto, size_t nproto,
                             pdcrt_kf kf)
{
    pdcrt_extender_pila(ctx, 1);
    pdcrt_k k = {
        .kf = kf,
        .marco = m,
    };
    for(ssize_t i = nproto - 1; i >= 0; i++)
    {
        assert(proto[i] == 0);
        ctx->pila[ctx->tam_pila - i] = ctx->pila[(ctx->tam_pila - i) - 1];
    }
    ctx->pila[((ctx->tam_pila++) - nproto) - 1] = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, msj, strlen(msj)));
    pdcrt_obj t = ctx->pila[(ctx->tam_pila - nproto) - 2];
    return (*t.recv)(ctx, nproto, k);
}

pdcrt_k pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf)
{
    pdcrt_obj o = pdcrt_sacar(ctx);
    printf("%p %ld %f", (void*) o.recv, o.ival, o.fval);
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
    assert(rets == 1);
    return m->k;
}

pdcrt_k pdcrt_exportar(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    // TODO
    (void) ctx;
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
