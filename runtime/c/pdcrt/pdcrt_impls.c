//
// Created by alinarezrangel on 15/3/26.
//

#define PDCRT_INTERNO
#include "pdcrt.h"
#include "pdcrt_base.h"
#include "pdcrt_ops.h"

#define PDCRT_CALC_ARGS() (ctx->tam_pila - (args < 8 ? 0 : args - 8));
#define PDCRT_SACAR_PRELUDIO() do { if(args >= 8) pdcrt_eliminar_elementos(ctx, argp, args - 8); } while(0)

typedef enum pdcrt_clase
{
    PDCRT_CLASE_NUMERO,
    PDCRT_CLASE_ARREGLO,
    PDCRT_CLASE_BOOLE,
    PDCRT_CLASE_PROCEDIMIENTO,
    PDCRT_CLASE_TIPO_NULO,
    PDCRT_CLASE_TEXTO,
} pdcrt_clase;

static pdcrt_tk pdcrt_recv_fallback_a_clase_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res);

static pdcrt_tk pdcrt_recv_fallback_a_clase(pdcrt_ctx *ctx, int args, pdcrt_k k, pdcrt_clase clase, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();

    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);

    pdcrt_marco *m = pdcrt_crear_marco(ctx, 10, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, pdcrt_objeto_entero(clase));
    pdcrt_fijar_local(ctx, m, 1, pdcrt_obj_desde_xmm(yo));
    pdcrt_fijar_local(ctx, m, 2, omsj);
    pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(argp));
    pdcrt_extender_pila(ctx, m, 6);
    memmove(ctx->pila + argp + 6, ctx->pila + argp, 6 * sizeof(pdcrt_obj));
    pdcrt_fijar_pila(ctx, argp + 0, pdcrt_obj_desde_xmm(a1));
    pdcrt_fijar_pila(ctx, argp + 1, pdcrt_obj_desde_xmm(a2));
    pdcrt_fijar_pila(ctx, argp + 2, pdcrt_obj_desde_xmm(a3));
    pdcrt_fijar_pila(ctx, argp + 3, pdcrt_obj_desde_xmm(a4));
    pdcrt_fijar_pila(ctx, argp + 4, pdcrt_obj_desde_xmm(a5));
    pdcrt_fijar_pila(ctx, argp + 5, pdcrt_obj_desde_xmm(a6));

    pdcrt_extender_pila(ctx, m, 2);

    switch(clase)
    {
    case PDCRT_CLASE_NUMERO:
        pdcrt_empujar(ctx, ctx->clase_numero);
        break;
    case PDCRT_CLASE_ARREGLO:
        pdcrt_empujar(ctx, ctx->clase_arreglo);
        break;
    case PDCRT_CLASE_BOOLE:
        pdcrt_empujar(ctx, ctx->clase_boole);
        break;
    case PDCRT_CLASE_PROCEDIMIENTO:
        pdcrt_empujar(ctx, ctx->clase_procedimiento);
        break;
    case PDCRT_CLASE_TIPO_NULO:
        pdcrt_empujar(ctx, ctx->clase_tipo_nulo);
        break;
    case PDCRT_CLASE_TEXTO:
        pdcrt_empujar(ctx, ctx->clase_texto);
        break;
    }
    pdcrt_obj obj_clase = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(obj_clase) == PDCRT_TOBJ_NULO)
    {
        pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
        pdcrt_inspeccionar_texto(omsj.texto);
        pdcrt_error(ctx, "Método no encontrado");
    }

    pdcrt_empujar(ctx, omsj);

    a1 = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
    __m128i obj = pdcrt_xmm_desde_obj(pdcrt_sacar(ctx));
    return pdcrt_llamar1(ctx, m, &pdcrt_recv_fallback_a_clase_k1,
                         obj, PDCRT_XMM_TEXTO(ctx->textos_globales.obtener_metodo_de_instancia), a1);
}

static pdcrt_tk pdcrt_recv_fallback_a_clase_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res)
{
    pdcrt_entero clase = pdcrt_obtener_local(ctx, m, 0).ival;
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj omsj = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_entero argp = pdcrt_obtener_local(ctx, m, 4).ival;

    (void) clase;
    (void) yo;
    (void) argp;

    // [args...]
    pdcrt_obj metodo = pdcrt_obj_desde_xmm(res);
    if(pdcrt_tipo_de_obj(metodo) == PDCRT_TOBJ_NULO)
    {
        pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
        pdcrt_inspeccionar_texto(omsj.texto);
        pdcrt_error(ctx, "Método no encontrado");
    }
    else
    {
        // [args...]
        return pdcrt_llamarn(ctx, m->k.marco, m->k.kf, m->args,
                             res, PDCRT_XMM_TEXTO(ctx->textos_globales.llamar));
    }
}

pdcrt_tk pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): comoTexto no acepta argumentos");
        static_assert(sizeof(pdcrt_entero) <= 64, "pdcrt_entero no debe tener más de 64 bits");
        char texto[21]; // 64 bits => máx. 20 caracteres + '\0'
        int len = snprintf(texto, sizeof(texto), "%" PDCRT_ENTERO_PRId, oyo.ival);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &k.marco, texto, len));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.sumar)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_mas))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al sumar se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_entero(oyo.ival + otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(((pdcrt_float) oyo.ival) + otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden sumar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.restar)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_menos))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al restar se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_entero(oyo.ival - otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(((pdcrt_float) oyo.ival) - otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden restar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_entero(oyo.ival * otro.ival);
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(((pdcrt_float) oyo.ival) * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): al dividir se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
        {
            if(otro.ival == 0)
                pdcrt_error(ctx, u8"división por 0");
            res = pdcrt_objeto_entero(((pdcrt_float) oyo.ival) / ((pdcrt_float) otro.ival));
        }
        else if(t_otro == PDCRT_TOBJ_FLOAT)
        {
            if(otro.fval == 0)
                pdcrt_error(ctx, u8"división por 0.0");
            res = pdcrt_objeto_float(((pdcrt_float) oyo.ival) / otro.fval);
        }
        else
        {
            pdcrt_error(ctx, u8"Numero (entero): solo se pueden dividir dos números");
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): operador_= / igualA necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_booleano(oyo.ival == arg.ival);
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_booleano(pdcrt_comparar_entero_y_float(oyo.ival, arg.fval, PDCRT_IGUAL_A));
        else
            res = pdcrt_objeto_booleano(false);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (entero): operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_booleano(oyo.ival != arg.ival);
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_booleano(!pdcrt_comparar_entero_y_float(oyo.ival, arg.fval, PDCRT_IGUAL_A));
        else
            res = pdcrt_objeto_booleano(true);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
#define PDCRT_COMPARAR_ENTERO(m, opm, ms, opms, cmp, op)                \
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (entero): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);                        \
        pdcrt_obj res = pdcrt_objeto_nulo();                            \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                 \
            res = pdcrt_objeto_booleano(oyo.ival op arg.ival);          \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)             \
            res = pdcrt_objeto_booleano(pdcrt_comparar_entero_y_float(oyo.ival, arg.fval, cmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (entero): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));       \
    }
    PDCRT_COMPARAR_ENTERO(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MENOR_QUE, <)
    PDCRT_COMPARAR_ENTERO(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MAYOR_QUE, >)
    PDCRT_COMPARAR_ENTERO(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MENOR_O_IGUAL_A, <=)
    PDCRT_COMPARAR_ENTERO(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MAYOR_O_IGUAL_A, >=)
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): negar no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(-oyo.ival)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): piso no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.ival)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): techo no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.ival)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): truncar no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.ival)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.byte_como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): byteComoTexto no acepta argumentos");
        unsigned char c = (unsigned char) oyo.ival;
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &k.marco, (const char *) &c, 1));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.invertir))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): invertir no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(~oyo.ival)));
    }
#define PDCRT_OPERADOR_BIT(txt, nm, op)                                              \
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.txt))             \
    {                                                                                \
        if(args != 1)                                                                \
            pdcrt_error(ctx, "Numero (entero): "nm" acepta solo un argumento");      \
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);                                     \
        pdcrt_obj res = pdcrt_objeto_nulo();                                         \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                              \
            res = pdcrt_objeto_entero(oyo.ival op arg.ival);                         \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                          \
            res = pdcrt_objeto_entero(oyo.ival op (pdcrt_entero) arg.fval);          \
        else                                                                         \
            pdcrt_error(ctx, "Argumento de tipo inesperado");                        \
        PDCRT_SACAR_PRELUDIO();                                                      \
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));                    \
    }
    PDCRT_OPERADOR_BIT(operador_bitand, "operador_<*>", &)
    PDCRT_OPERADOR_BIT(operador_bitor, "operador_<+>", |)
    PDCRT_OPERADOR_BIT(operador_bitxor, "operador_<^>", ^)
    PDCRT_OPERADOR_BIT(operador_bitlshift, "operador_<<", <<)
    PDCRT_OPERADOR_BIT(operador_bitrshift, "operador_>>", >>)
#undef PDCRT_OPERADOR_BIT

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_NUMERO, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): comoTexto no acepta argumentos");
        char texto[30];
        int len = snprintf(texto, sizeof(texto), "%g", (double) oyo.fval);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &k.marco, texto, len));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.sumar)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_mas))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al sumar se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_float(oyo.fval + ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(oyo.fval + otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden sumar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.restar)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_menos))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al restar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_float(oyo.fval - ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(oyo.fval - otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden restar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, yo.fval * ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, yo.fval * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al dividir se debe especificar un argumento");
        pdcrt_obj otro = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        if(t_otro == PDCRT_TOBJ_ENTERO)
            pdcrt_empujar_float(ctx, k.marco, yo.fval / ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            pdcrt_empujar_float(ctx, k.marco, yo.fval / otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden dividir dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.fval == arg.fval);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_entero_y_float(arg.ival, yo.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.fval != arg.fval);
        }
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, !pdcrt_comparar_entero_y_float(arg.ival, yo.fval, PDCRT_IGUAL_A));
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
#define PDCRT_COMPARAR_FLOAT(m, opm, ms, opms, rcmp, op)                \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (float): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_extender_pila(ctx, k.marco, 1);                           \
        pdcrt_obj arg = ctx->pila[argp];                                \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                  \
            pdcrt_empujar_booleano(ctx, k.marco, yo.fval op arg.fval);           \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)            \
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_entero_y_float(arg.ival, yo.fval, rcmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (float): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return pdcrt_continuar(ctx, k);                                 \
    }
    PDCRT_COMPARAR_FLOAT(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MAYOR_O_IGUAL_A, <)
    PDCRT_COMPARAR_FLOAT(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MENOR_O_IGUAL_A, >)
    PDCRT_COMPARAR_FLOAT(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MAYOR_QUE, <=)
    PDCRT_COMPARAR_FLOAT(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MENOR_QUE, >=)
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): negar no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, -yo.fval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): piso no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, PDCRT_FLOAT_FLOOR(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): techo no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, PDCRT_FLOAT_CEIL(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): truncar no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_float(ctx, k.marco, PDCRT_FLOAT_TRUNC(yo.fval));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.byte_como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): byteComoTexto no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        char c = (char) yo.fval;
        pdcrt_empujar_texto(ctx, &k.marco, &c, 1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.invertir))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): invertir no acepta argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, ~(pdcrt_entero) yo.fval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
#define PDCRT_OPERADOR_BIT(txt, nm, op)                                              \
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.txt))              \
    {                                                                                \
        if(args != 1)                                                                \
            pdcrt_error(ctx, "Numero (float): "nm" acepta solo un argumento");       \
        pdcrt_extender_pila(ctx, k.marco, 1);                                        \
        pdcrt_obj arg = ctx->pila[argp];                                             \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                              \
            pdcrt_empujar_entero(ctx, k.marco, ((pdcrt_entero) yo.fval) op arg.ival); \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                          \
            pdcrt_empujar_entero(ctx, k.marco, ((pdcrt_entero) yo.fval) op (pdcrt_entero) arg.fval); \
        else                                                                         \
            pdcrt_error(ctx, "Argumento de tipo inesperado");                        \
        PDCRT_SACAR_PRELUDIO();                                                      \
        return pdcrt_continuar(ctx, k);                                              \
    }
    PDCRT_OPERADOR_BIT(operador_bitand, "operador_<*>", &)
    PDCRT_OPERADOR_BIT(operador_bitor, "operador_<+>", |)
    PDCRT_OPERADOR_BIT(operador_bitxor, "operador_<^>", ^)
    PDCRT_OPERADOR_BIT(operador_bitlshift, "operador_<<", <<)
    PDCRT_OPERADOR_BIT(operador_bitrshift, "operador_>>", >>)
#undef PDCRT_OPERADOR_BIT

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_NUMERO);
}

pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k)
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
        pdcrt_extender_pila(ctx, k.marco, 1);
        if(yo.bval)
            pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.verdadero));
        else
            pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.falso));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.bval == arg.bval);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.bval != arg.bval);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.escoger))
    {
        if(args != 2)
            pdcrt_error(ctx, "Booleano: escoger necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj siVerdadero = ctx->pila[argp];
        pdcrt_obj siFalso = ctx->pila[argp + 1];
        pdcrt_empujar(ctx, yo.bval ? siVerdadero : siFalso);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.llamarSegun)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.llamarSegun2))
    {
        if(args != 2)
            pdcrt_error(ctx, u8"Booleano: llamarSegún necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj siVerdadero = ctx->pila[argp];
        pdcrt_obj siFalso = ctx->pila[argp + 1];
        pdcrt_empujar(ctx, yo.bval ? siVerdadero : siFalso);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_enviar_mensaje(ctx, k.marco, "llamar", 6, NULL, 0, k.kf);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.o)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_o))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: \"||\" necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj v = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
        pdcrt_empujar_booleano(ctx, k.marco, yo.bval || v.bval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.y)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_y))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: \"&&\" necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj v = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
        pdcrt_empujar_booleano(ctx, k.marco, yo.bval && v.bval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_BOOLE);
}

pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) yo;
    (void) argp;
    (void) k;
    assert(0 && "sin implementar");
}

static bool pdcrt_es_digito(char c)
{
    return c >= '0' && c <= '9';
}

static bool pdcrt_prefijo_de_texto(pdcrt_texto *txt, size_t pos, const char *prefix)
{
    for(size_t i = pos; i < txt->longitud; i++)
    {
        char c = *prefix++;
        if(!c)
            break;
        else if(c != txt->contenido[i])
            return false;
    }
    return true;
}

static pdcrt_k pdcrt_texto_formatear_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_texto_formatear_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_texto_formatear_k3(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_texto_formatear_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_texto_formatear_k1);

    pdcrt_obj arr = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj oargs_consumidos = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_obj oarg_ptr = pdcrt_obtener_local(ctx, m, 3);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 4);
#define PDCRT_RECARGAR_LOCALES()                                    \
    do                                                              \
    {                                                               \
        arr = pdcrt_obtener_local(ctx, m, 0);                       \
        oi = pdcrt_obtener_local(ctx, m, 1);                        \
        oargs_consumidos = pdcrt_obtener_local(ctx, m, 2);          \
        oarg_ptr = pdcrt_obtener_local(ctx, m, 3);                  \
        yo = pdcrt_obtener_local(ctx, m, 4);                        \
        i = oi.ival;                                                \
        args_consumidos = oargs_consumidos.ival;                    \
        arg_ptr = oarg_ptr.ival;                                    \
        args = m->args;                                             \
    }                                                               \
    while(0)

    pdcrt_entero i = oi.ival;
    pdcrt_entero args_consumidos = oargs_consumidos.ival;
    pdcrt_entero arg_ptr = oarg_ptr.ival;
    pdcrt_entero args = m->args;
    pdcrt_extender_pila(ctx, m, 2);

    if((size_t) i >= yo.texto->longitud)
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "");
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "unir", 4, proto, 1, &pdcrt_texto_formatear_k3);
    }

    if(pdcrt_prefijo_de_texto(yo.texto, i, "~T"))
    {
        if(args_consumidos >= args)
            pdcrt_error(ctx, u8"Texto#formatear: más formatos que argumentos");
        args_consumidos += 1;
        i += 2;
        pdcrt_empujar(ctx, arr);
        pdcrt_duplicar(ctx, m,arg_ptr++);
        pdcrt_obj arg = pdcrt_cima(ctx);
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
            pdcrt_error(ctx, "");
        pdcrt_arreglo_empujar_al_final(ctx, m, -2);
        (void) pdcrt_sacar(ctx);
        goto final;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~t"))
    {
        if(args_consumidos >= args)
            pdcrt_error(ctx, u8"Texto#formatear: más formatos que argumentos");
        args_consumidos += 1;
        i += 2;
        pdcrt_empujar(ctx, arr);
        pdcrt_duplicar(ctx, m, arg_ptr++);
        // [arr, arg]
        pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_entero(i));
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(args_consumidos));
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(arg_ptr));
        return pdcrt_enviar_mensaje(ctx, m, "comoTexto", 9, NULL, 0, &pdcrt_texto_formatear_k2);
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~~"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "~");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~%"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "\n");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~e"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "}");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~E"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "»");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~q"))
    {
        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto_cstr(ctx, &m, "\"");
        PDCRT_RECARGAR_LOCALES();
        i += 2;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~|%\n"))
    {
        i += 4;
        goto final;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~|%\r\n"))
    {
        i += 5;
        goto final;
    }
    else if(pdcrt_prefijo_de_texto(yo.texto, i, "~"))
    {
        pdcrt_error(ctx, u8"Formato inválido para Texto#formatear");
    }
    else
    {
        size_t len = 0;
        for(size_t j = i; j < yo.texto->longitud; j++)
        {
            if(yo.texto->contenido[j] == '~')
                break;
            len += 1;
        }

        pdcrt_empujar(ctx, arr);
        pdcrt_empujar_texto(ctx, &m, yo.texto->contenido + i, len);
        PDCRT_RECARGAR_LOCALES();
        i += len;
    }

    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    (void) pdcrt_sacar(ctx);
    final:
    pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_entero(i));
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(args_consumidos));
    pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(arg_ptr));
    return pdcrt_texto_formatear_k1(ctx, m);

#undef PDCRT_RECARGAR_LOCALES
}

static pdcrt_k pdcrt_texto_formatear_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_texto_formatear_k2);

    pdcrt_obj arg = pdcrt_cima(ctx);
    if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        pdcrt_error(ctx, "");
    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    (void) pdcrt_sacar(ctx);
    return pdcrt_texto_formatear_k1(ctx, m);
}

static pdcrt_k pdcrt_texto_formatear_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_texto_formatear_k3);
    pdcrt_obj oarg_inic = pdcrt_obtener_local(ctx, m, 5);
    pdcrt_entero arg_inic = oarg_inic.ival;
    pdcrt_eliminar_elementos(ctx, arg_inic, m->args);
    return pdcrt_devolver(ctx, m, 1);
}

pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k)
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
        char *buff = pdcrt_alojar_ctx(ctx, bufflen);
        if(!buff)
            pdcrt_enomem(ctx);
        memcpy(buff, yo.texto->contenido, yo.texto->longitud);
        memcpy(buff + yo.texto->longitud, arg.texto->contenido, arg.texto->longitud);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_texto *res = pdcrt_crear_texto(ctx, &k.marco, buff, bufflen);
        pdcrt_desalojar_ctx(ctx, buff, bufflen);
        pdcrt_empujar(ctx, pdcrt_objeto_texto(res));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, pdcrt_comparar_textos(yo.texto, arg.texto));
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, !pdcrt_comparar_textos(yo.texto, arg.texto));
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_numero_entero))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroEntero no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        const char* s = yo.texto->contenido;
        if(*s == '-')
            s += 1;
        for(; *s; s++)
            if(!pdcrt_es_digito(*s))
                goto error_como_entero;

        pdcrt_entero i;
        i = strtoll(yo.texto->contenido, NULL, 10);
        pdcrt_empujar_entero(ctx, k.marco, i);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    error_como_entero:
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_numero_real))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroReal no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
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
        pdcrt_empujar_float(ctx, k.marco, f);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    error_como_real:
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, yo);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: longitud no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.texto->longitud);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: en necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: en necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= yo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #en");
        pdcrt_empujar_texto(ctx, &k.marco, yo.texto->contenido + i, 1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.byte_en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: byteEn necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: byteEn necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= yo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #byteEn");
        pdcrt_empujar_entero(ctx, k.marco, yo.texto->contenido[i]);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.subtexto))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);

        bool ok = false;
        pdcrt_entero inicio, longitud;
        inicio = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 enteros como argumentos");
        longitud = pdcrt_obtener_entero(ctx, argp + 1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 enteros como argumentos");

        if(inicio < 0 || (size_t) inicio > yo.texto->longitud)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el primer argumento de #subTexto");
        if(longitud < 0)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el segundo argumento de #subTexto");

        if((size_t) (inicio + longitud) > yo.texto->longitud)
            longitud = yo.texto->longitud - inicio;

        if(longitud == 0)
        {
            pdcrt_empujar_texto(ctx, &k.marco, "", 0);
        }
        else
        {
            char *buffer = pdcrt_alojar_ctx(ctx, longitud);
            assert(buffer);
            memcpy(buffer, yo.texto->contenido + inicio, longitud);
            pdcrt_empujar_texto(ctx, &k.marco, buffer, longitud);
            pdcrt_desalojar_ctx(ctx, buffer, longitud);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.parte_del_texto))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);

        bool ok = false;
        pdcrt_entero inicio, final;
        inicio = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 enteros como argumentos");
        final = pdcrt_obtener_entero(ctx, argp + 1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 enteros como argumentos");

        if(inicio < 0 || (size_t) inicio > yo.texto->longitud)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el primer argumento de #parteDelTexto");
        if(final < 0)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el segundo argumento de #parteDelTexto");
        if((size_t) final > yo.texto->longitud)
            final = yo.texto->longitud;

        if(final <= inicio)
        {
            pdcrt_empujar_texto(ctx, &k.marco, "", 0);
        }
        else
        {
            char *buffer = pdcrt_alojar_ctx(ctx, final - inicio);
            assert(buffer);
            memcpy(buffer, yo.texto->contenido + inicio, final - inicio);
            pdcrt_empujar_texto(ctx, &k.marco, buffer, final - inicio);
            pdcrt_desalojar_ctx(ctx, buffer, final - inicio);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.buscar))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: buscar necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);

        /* Algoritmo Knuth-Morris-Pratt, sacado de wikipedia:
         * <https://en.wikipedia.org/wiki/Knuth%E2%80%93Morris%E2%80%93Pratt_algorithm>
         * el 2024-05-05.
         */

        bool ok = false;
        pdcrt_entero desde = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: buscar necesita un entero como primer argumento");
        if(desde < 0 || (size_t) desde > yo.texto->longitud)
            pdcrt_error(ctx, "Texto: buscar primer argumento fuera de rango");
        size_t buffer_len = pdcrt_obtener_tam_texto(ctx, argp + 1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: buscar necesita un texto como segundo argumento");

        if(buffer_len == 0)
        {
            pdcrt_empujar_entero(ctx, k.marco, desde);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }

        char *buffer = pdcrt_alojar_ctx(ctx, buffer_len + 1);
        assert(buffer);
        ok = pdcrt_obtener_texto(ctx, argp + 1, buffer, buffer_len + 1);
        assert(ok);
        ssize_t *skip_table = pdcrt_alojar_ctx(ctx, buffer_len * sizeof(ssize_t));
        assert(skip_table);

        // Llena la tabla.
        skip_table[0] = -1;
        ssize_t buffer_candidato = 0;
        for(size_t i = 1; i < buffer_len; i++, buffer_candidato++)
        {
            if(buffer[i] == buffer[buffer_candidato])
            {
                skip_table[i] = skip_table[buffer_candidato];
            }
            else
            {
                skip_table[i] = buffer_candidato;
                while(buffer_candidato >= 0 && buffer[buffer_candidato] != buffer[i])
                {
                    buffer_candidato = skip_table[buffer_candidato];
                }
            }
        }

        // Busca el texto.
        size_t yo_pos = desde, buffer_pos = 0;

        while(yo_pos < yo.texto->longitud)
        {
            if(yo.texto->contenido[yo_pos] == buffer[buffer_pos])
            {
                buffer_pos += 1;
                yo_pos += 1;
                if(buffer_pos >= buffer_len)
                {
                    pdcrt_empujar_entero(ctx, k.marco, yo_pos - buffer_pos);
                    goto buscar_encontrado;
                }
            }
            else
            {
                ssize_t el = skip_table[buffer_pos];
                if(el < 0)
                {
                    yo_pos += 1;
                    buffer_pos = 0;
                }
                else
                {
                    buffer_pos = el;
                }
            }
        }

        pdcrt_empujar_nulo(ctx, k.marco);
    buscar_encontrado:

        pdcrt_desalojar_ctx(ctx, buffer, buffer_len + 1);
        pdcrt_desalojar_ctx(ctx, skip_table, buffer_len * sizeof(ssize_t));

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.formatear))
    {
        const char *str = yo.texto->contenido;
        size_t len = yo.texto->longitud;
        pdcrt_extender_pila(ctx, k.marco, 2);

        // Vamos a calcular la cantidad de formatos en el texto
        // Al hacer esto podemos computar la capacidad del arreglo
        // intermediario y así evitar consumir más memoria de la
        // necesaria.
        bool enFormato = false; // Si el carácter anterior fue '~'
        size_t formatos = 0;
        for(size_t i = 0; i < len; i++)
        {
            if(str[i] == '~')
            {
                if(enFormato)
                    formatos += 1;
                enFormato = !enFormato;
            }
            else if(enFormato)
            {
                formatos += 1;
                enFormato = false;
            }
        }

        pdcrt_empujar_arreglo_vacio(ctx, &k.marco, 2 * formatos + 1);

        pdcrt_marco *m = pdcrt_crear_marco(ctx, 6, 0, args, k);

        // Elimina `yo` y el mensaje
        pdcrt_eliminar_elementos(ctx, inic, 2);

        // arr
        pdcrt_fijar_local(ctx, m, 0, pdcrt_sacar(ctx));
        // oi
        pdcrt_fijar_local(ctx, m, 1, pdcrt_objeto_entero(0));
        // oargs_consumidos
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0));
        // oarg_ptr
        pdcrt_fijar_local(ctx, m, 3, pdcrt_objeto_entero(argp - 2));
        // yo
        pdcrt_fijar_local(ctx, m, 4, yo);
        // oarg_inic
        pdcrt_fijar_local(ctx, m, 5, pdcrt_objeto_entero(argp - 2));

        return pdcrt_texto_formatear_k1(ctx, m);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TEXTO);
}

pdcrt_k pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    (void) yo;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, k.marco, pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_NULO);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, k.marco, pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_NULO);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Nulo: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, "NULO");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TIPO_NULO);
}

static pdcrt_k pdcrt_arreglo_igual_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_igual_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_arreglo_igual_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_igual_k1);
    // []
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj otro = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    if((size_t) i.ival < yo.arreglo->longitud && (size_t) i.ival < otro.arreglo->longitud)
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.arreglo->valores[i.ival]);
        pdcrt_empujar(ctx, otro.arreglo->valores[i.ival]);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "igualA", 6, proto, 1, &pdcrt_arreglo_igual_k2);
    }
    else if(yo.arreglo->longitud == otro.arreglo->longitud)
    {
        // Fin del arreglo, todos los elementos fueron iguales
        pdcrt_empujar_booleano(ctx, m, true);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        // Fin del arreglo, tenían tamaños distintos
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar_booleano(ctx, m, false);
        return pdcrt_devolver(ctx, m, 1);
    }
}

static pdcrt_k pdcrt_arreglo_igual_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_igual_k2);
    // [eq]
    bool ok = false;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Arreglo: igualA / operador_=: el metodo igualA de un elemento no devolvio un booleano");
    if(!eq)
    {
        (void) pdcrt_sacar(ctx);
        pdcrt_empujar_booleano(ctx, m, false);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        pdcrt_entero i = pdcrt_obtener_local(ctx, m, 2).ival;
        (void) pdcrt_sacar(ctx); // eq
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));
        return pdcrt_arreglo_igual_k1(ctx, m);
    }
}

static pdcrt_k pdcrt_arreglo_distinto_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_distinto_k2(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_arreglo_distinto_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_distinto_k1);
    // []
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj otro = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    if((size_t) i.ival < yo.arreglo->longitud && (size_t) i.ival < otro.arreglo->longitud)
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.arreglo->valores[i.ival]);
        pdcrt_empujar(ctx, otro.arreglo->valores[i.ival]);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, u8"distintoDe", 11, proto, 1, &pdcrt_arreglo_distinto_k2);
    }
    else if(yo.arreglo->longitud == otro.arreglo->longitud)
    {
        // Fin del arreglo, todos los elementos fueron iguales
        pdcrt_eliminar_elementos(ctx, -3, 3);
        pdcrt_empujar_booleano(ctx, m, false);
        return pdcrt_continuar(ctx, m->k);
    }
    else
    {
        // Fin del arreglo, tenían tamaños distintos
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar_booleano(ctx, m, true);
        return pdcrt_continuar(ctx, m->k);
    }
}

static pdcrt_k pdcrt_arreglo_distinto_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_distinto_k2);
    // [eq]
    bool ok = false;
    bool eq = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "Arreglo: distintoDe / operador_no=: el metodo distintoDe de un elemento no devolvio un booleano");
    if(!eq)
    {
        (void) pdcrt_sacar(ctx);
        pdcrt_empujar_booleano(ctx, m, true);
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        pdcrt_entero i = pdcrt_obtener_local(ctx, m, 2).ival;
        (void) pdcrt_sacar(ctx); // eq
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));
        return pdcrt_arreglo_distinto_k1(ctx, m);
    }
}

static pdcrt_k pdcrt_arreglo_como_texto_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k4(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_arreglo_como_texto_k5(pdcrt_ctx *ctx, pdcrt_marco *m);

static pdcrt_k pdcrt_arreglo_como_texto_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj buffer = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    if((size_t) i.ival < yo.arreglo->longitud)
    {
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar(ctx, yo.arreglo->valores[i.ival]);
        return pdcrt_enviar_mensaje(ctx, m, u8"comoTexto", 9, NULL, 0, &pdcrt_arreglo_como_texto_k2);
    }
    else
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_obj sep = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, &m, ", ", 2));
        buffer = pdcrt_obtener_local(ctx, m, 1);
        pdcrt_empujar(ctx, buffer);
        pdcrt_empujar(ctx, sep);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, u8"unir", 4, proto, 1, &pdcrt_arreglo_como_texto_k3);
    }
}

static pdcrt_k pdcrt_arreglo_como_texto_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k2);
    // [eltxt]
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj buffer = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj i = pdcrt_obtener_local(ctx, m, 2);
    (void) yo;
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar(ctx, buffer);
    // [eltxt, buffer]
    pdcrt_obj cima = pdcrt_cima(ctx);
    pdcrt_fijar_pila(ctx, ctx->tam_pila - 1, ctx->pila[ctx->tam_pila - 2]);
    pdcrt_fijar_pila(ctx, ctx->tam_pila - 2, cima);
    // [buffer, eltxt]
    pdcrt_arreglo_empujar_al_final(ctx, m, -2);
    // [buffer]
    (void) pdcrt_sacar(ctx);
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i.ival + 1));
    return pdcrt_arreglo_como_texto_k1(ctx, m);
}

static pdcrt_k pdcrt_arreglo_como_texto_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k3);
    // [res]
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar_texto_cstr(ctx, &m, "(Arreglo#crearCon: ");
    pdcrt_extraer(ctx, -2);
    // [pref, res]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "concatenar", 10, proto, 1, &pdcrt_arreglo_como_texto_k4);
}

static pdcrt_k pdcrt_arreglo_como_texto_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k4);
    // [res]
    pdcrt_extender_pila(ctx, m, 1);
    pdcrt_empujar_texto_cstr(ctx, &m, ")");
    // [res, suf]
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "concatenar", 10, proto, 1, &pdcrt_arreglo_como_texto_k5);
}
static pdcrt_k pdcrt_arreglo_como_texto_k5(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_arreglo_como_texto_k5);
    // [res]
    return pdcrt_devolver(ctx, m, 1);
}

pdcrt_k pdcrt_recv_arreglo(pdcrt_ctx *ctx, int args, pdcrt_k k)
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
            pdcrt_error(ctx, "Arreglo: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_ARREGLO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            pdcrt_fijar_local(ctx, m, 1, arg); // otro
            pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0)); // i
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_arreglo_igual_k1(ctx, m);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_ARREGLO)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            pdcrt_fijar_local(ctx, m, 1, arg); // otro
            pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0)); // i
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_arreglo_distinto_k1(ctx, m);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Arreglo: comoTexto no necesita argumentos");
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_obj buffer = pdcrt_objeto_arreglo(
            pdcrt_crear_arreglo_vacio(ctx, &m, yo.arreglo->longitud + 2));
        pdcrt_fijar_local(ctx, m, 1, buffer); // buffer
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0)); // i
        PDCRT_SACAR_PRELUDIO();
        // []
        return pdcrt_arreglo_como_texto_k1(ctx, m);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: en necesita un argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Arreglo: en necesita un entero como argumento");
        if(i < 0 || (size_t) i >= yo.arreglo->longitud)
            pdcrt_error(ctx, "Arreglo: en: indice fuera de rango");
        pdcrt_empujar(ctx, yo.arreglo->valores[i]);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijarEn))
    {
        if(args != 2)
            pdcrt_error(ctx, "Arreglo: fijarEn necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok;
        pdcrt_entero i = pdcrt_obtener_entero(ctx, -2, &ok);
        if(!ok)
            pdcrt_error(ctx, "Arreglo: fijarEn necesita un entero como argumento");
        if(i < 0 || (size_t) i >= yo.arreglo->longitud)
            pdcrt_error(ctx, "Arreglo: fijarEn: indice fuera de rango");
        pdcrt_obj val = pdcrt_cima(ctx);
        pdcrt_barrera_de_escritura(ctx, yo, val);
        yo.arreglo->valores[i] = val;
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Arreglo: longitud no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.arreglo->longitud);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.unir))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: unir necesita un argumento");
        pdcrt_obj separador = pdcrt_cima(ctx);
        if(pdcrt_tipo_de_obj(separador) != PDCRT_TOBJ_TEXTO)
            pdcrt_error(ctx, "Arreglo: el argumento de unir debe ser un texto");
        size_t tam_final = 0;
        for(size_t i = 0; i < yo.arreglo->longitud; i++)
        {
            pdcrt_obj el = yo.arreglo->valores[i];
            if(pdcrt_tipo_de_obj(el) != PDCRT_TOBJ_TEXTO)
                pdcrt_error(ctx, "Arreglo: los elementos del arreglo deben ser textos");
            tam_final += el.texto->longitud;
            if(i > 0)
                tam_final += separador.texto->longitud;
        }

        char *buffer = pdcrt_alojar_ctx(ctx, tam_final);
        if(!buffer)
            pdcrt_enomem(ctx);
        size_t cur = 0;

        for(size_t i = 0; i < yo.arreglo->longitud; i++)
        {
            if(i > 0)
            {
                if(separador.texto->longitud > 0)
                    memcpy(buffer + cur, separador.texto->contenido,
                           separador.texto->longitud);
                cur += separador.texto->longitud;
            }
            pdcrt_obj el = yo.arreglo->valores[i];
            if(el.texto->longitud > 0)
                memcpy(buffer + cur, el.texto->contenido, el.texto->longitud);
            cur += el.texto->longitud;
        }

        pdcrt_empujar_texto(ctx, &k.marco, buffer, tam_final);
        pdcrt_desalojar_ctx(ctx, buffer, tam_final);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.agregar_al_final))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: agregarAlFinal necesita 1 argumento");
        pdcrt_obj arg = ctx->pila[argp];
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, arg);
        pdcrt_arreglo_empujar_al_final(ctx, k.marco, -4);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_ARREGLO);
}

pdcrt_k pdcrt_recv_closure(pdcrt_ctx *ctx, int args, pdcrt_k k)
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
            pdcrt_error(ctx, "Procedimiento: operador_= / igualA necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CLOSURE)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.closure == arg.closure);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Procedimiento: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CLOSURE)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.closure != arg.closure);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Procedimiento: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Procedimiento: %p", yo.closure);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.llamar))
    {
        pdcrt_extender_pila(ctx, k.marco, yo.closure->num_capturas);
        for(size_t i = 0; i < yo.closure->num_capturas; i++)
        {
            pdcrt_empujar(ctx, yo.closure->capturas[i]);
        }
        pdcrt_eliminar_elementos(ctx, inic, 2);
        return (*yo.closure->f)(ctx, args, k);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_PROCEDIMIENTO);
}

pdcrt_k pdcrt_recv_caja(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;
    (void) yo;
    (void) k;

    assert(0 && "sin implementar");
}


static pdcrt_k pdcrt_tabla_para_cada_par_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_tabla(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijarEn))
    {
        if(args != 2)
            pdcrt_error(ctx, "Tabla: fijarEn necesita 2 argumentos");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_obj valor = ctx->pila[argp + 1];
        pdcrt_tabla_fijar(ctx, k.marco, yo.tabla, llave, valor, true);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: en necesita 1 argumento");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_obj valor = pdcrt_objeto_nulo();
        if(!pdcrt_tabla_en(ctx, k.marco, yo.tabla, llave, &valor))
            pdcrt_error(ctx, "Llave no existe en la tabla primitiva");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, valor);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: longitud no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, yo.tabla->buckets_ocupados);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.rehashear))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: rehashear necesita 1 argumento");
        bool ok;
        pdcrt_entero capacidad_adicional = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Tabla#rehashear: necesita un entero como argumento");

        if(capacidad_adicional < 0)
        {
            pdcrt_error(ctx, u8"Tabla#rehashear: Valor inválido para la capacidad adicional");
        }

        pdcrt_tabla_rehashear(ctx, k.marco, yo.tabla, yo.tabla->buckets_ocupados + capacidad_adicional);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.capacidad))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: capacidad no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_entero(ctx, k.marco, pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.contiene))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: contiene necesita 1 argumento");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_obj valor = pdcrt_objeto_nulo();
        bool contiene = pdcrt_tabla_en(ctx, k.marco, yo.tabla, llave, &valor);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_booleano(ctx, k.marco, contiene);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.eliminar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: eliminar necesita 1 argumento");
        pdcrt_obj llave = ctx->pila[argp];
        pdcrt_tabla_eliminar(ctx, k.marco, yo.tabla, llave, true);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.paraCadaPar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: paraCadaPar necesita 1 argumento");
        pdcrt_obj iterador = pdcrt_cima(ctx);
        pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, 0, k);
        pdcrt_fijar_local(ctx, m, 0, yo);
        pdcrt_fijar_local(ctx, m, 1, iterador);
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0));
        PDCRT_SACAR_PRELUDIO();
        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_tabla_para_cada_par_k1(ctx, m);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_tabla_para_cada_par_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_tabla_para_cada_par_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj iterador = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_entero i = oi.ival;

    // [nulo]
    (void) pdcrt_sacar(ctx);

    if(i < pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara))
    {
        pdcrt_extender_pila(ctx, m, 3);
        yo = pdcrt_obtener_local(ctx, m, 0);
        iterador = pdcrt_obtener_local(ctx, m, 1);
        oi = pdcrt_obtener_local(ctx, m, 2);
        i = oi.ival;
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));

        if(yo.tabla->buckets[i].activo)
        {
            pdcrt_empujar(ctx, iterador);
            pdcrt_empujar(ctx, yo.tabla->buckets[i].llave);
            pdcrt_empujar(ctx, yo.tabla->buckets[i].valor);
            static const int proto[] = {0, 0};
            return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2,
                                        &pdcrt_tabla_para_cada_par_k1);
        }
        else
        {
            pdcrt_empujar_nulo(ctx, m);
            return pdcrt_tabla_para_cada_par_k1(ctx, m);
        }
    }
    else if(i - pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara) < yo.tabla->num_colisiones)
    {
        pdcrt_extender_pila(ctx, m, 3);
        yo = pdcrt_obtener_local(ctx, m, 0);
        iterador = pdcrt_obtener_local(ctx, m, 1);
        oi = pdcrt_obtener_local(ctx, m, 2);
        i = oi.ival;
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));

        size_t buckets = pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara);

        assert(i - buckets < yo.tabla->num_colisiones);
        assert(yo.tabla->colisiones[i - buckets].activo);

        pdcrt_empujar(ctx, iterador);
        pdcrt_empujar(ctx, yo.tabla->colisiones[i - buckets].llave);
        pdcrt_empujar(ctx, yo.tabla->colisiones[i - buckets].valor);
        static const int proto[] = {0, 0};
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2,
                                    &pdcrt_tabla_para_cada_par_k1);
    }
    else
    {
        pdcrt_extender_pila(ctx, m, 1);
        pdcrt_empujar_nulo(ctx, m);
        return pdcrt_devolver(ctx, m, 1);
    }
}

pdcrt_k pdcrt_recv_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) yo;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, "Runtime");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.crearTabla))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearTabla necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        bool ok;
        pdcrt_entero n = pdcrt_obtener_entero(ctx, argp, &ok);
        if(!ok)
            pdcrt_error(ctx, u8"Runtime: crearTabla: su único argumento debe ser un entero");
        pdcrt_empujar_tabla_vacia(ctx, &k.marco, n);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.crearCorrutina))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearCorrutina necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_corrutina(ctx, &k.marco, -1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.recolectar_basura))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: recolectarBasura no necesita argumentos");
        pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_memoria(ctx, 0);
        pdcrt_recolectar_basura_simple(ctx, &k.marco, params);
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.crear_instancia))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: crearInstancia necesita 3 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [num_atrs, metodos, metodo_no_encontrado]
        bool ok = false;
        pdcrt_entero num_atrs = pdcrt_obtener_entero(ctx, -3, &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: crearInstancia: numAtrs debe ser un entero");
        pdcrt_empujar_instancia(ctx, &k.marco, -2, -1, num_atrs);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.atributos_de_instancia))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: atributosDeInstancia necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        pdcrt_empujar_entero(ctx, k.marco, (pdcrt_entero) inst.inst->num_atributos);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_metodos))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: obtenerMétodos necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        pdcrt_empujar(ctx, inst.inst->metodos);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_atributo))
    {
        if(args != 2)
            pdcrt_error(ctx, "Runtime: obtenerAtributo necesita 2 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst, atr]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        bool ok = false;
        pdcrt_entero atr = pdcrt_obtener_entero(ctx, -1, &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: obtenerAtributo: el atributo debe ser un entero");
        if(atr < 0 || (size_t) atr >= inst.inst->num_atributos)
            pdcrt_error(ctx, u8"Runtime: obtenerAtributo: índice de atributo inválido");
        pdcrt_empujar(ctx, inst.inst->atributos[atr]);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_atributo))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: fijarAtributo necesita 3 argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst, atr, valor]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        bool ok = false;
        pdcrt_entero atr = pdcrt_obtener_entero(ctx, -2, &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: fijarAtributo: el atributo debe ser un entero");
        if(atr < 0 || (size_t) atr >= inst.inst->num_atributos)
            pdcrt_error(ctx, u8"Runtime: fijarAtributo: índice de atributo inválido");
        pdcrt_obj valor = ctx->pila[argp + 2];
        pdcrt_barrera_de_escritura(ctx, inst, valor);
        inst.inst->atributos[atr] = valor;
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.es_instancia))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: esInstancia necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [inst]
        pdcrt_obj inst = ctx->pila[argp];
        pdcrt_empujar_booleano(ctx, k.marco, pdcrt_tipo_de_obj(inst) == PDCRT_TOBJ_INSTANCIA);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.enviar_mensaje))
    {
        if(args < 2)
            pdcrt_error(ctx, "Runtime: enviarMensaje necesita al menos 2 argumentos");
        pdcrt_eliminar_elementos(ctx, inic, 2); // Saca yo y msj, deja solo los argumentos
        // [obj, msj, ...args]
        return pdcrt_enviar_mensaje_obj(ctx, k.marco, NULL, args - 2, k.kf);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fallar_con_mensaje))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: fallarConMensaje necesita 1 argumento");
        pdcrt_extender_pila(ctx, k.marco, 1);
        // [texto]
        pdcrt_obj texto = ctx->pila[argp];
        pdcrt_debe_tener_tipo(ctx, texto, PDCRT_TOBJ_TEXTO);
        pdcrt_error(ctx, texto.texto->contenido);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.leer_caracter))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: leerCarácter no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        int c = getchar();
        if(c == EOF)
            c = -1;
        pdcrt_empujar_entero(ctx, k.marco, c);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_argv))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerArgv no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, ctx->argv);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_programa))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerPrograma no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, ctx->nombre_del_programa);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.obtener_clase_objeto))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerClaseObjeto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_empujar(ctx, ctx->clase_objeto);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_objeto))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseObjeto necesita 1 argumento");
        ctx->clase_objeto = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_arreglo))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseArreglo necesita 1 argumento");
        ctx->clase_arreglo = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_boole))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseBoole necesita 1 argumento");
        ctx->clase_boole = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_numero))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseNumero necesita 1 argumento");
        ctx->clase_numero = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_procedimiento))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseProcedimieto necesita 1 argumento");
        ctx->clase_procedimiento = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_tipo_nulo))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTipoNulo necesita 1 argumento");
        ctx->clase_tipo_nulo = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.fijar_clase_texto))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTexto necesita 1 argumento");
        ctx->clase_texto = ctx->pila[argp];
        pdcrt_empujar_nulo(ctx, k.marco);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    assert(0 && "sin implementar");
}

pdcrt_k pdcrt_recv_voidptr(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Voidptr: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Voidptr: %p", yo.pval);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    assert(0 && "sin implementar");
}

pdcrt_k pdcrt_recv_valop(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Valop: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Valop: %p", yo.valop);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k4(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_recv_espacio_de_nombres_k5(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_espacio_de_nombres(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    pdcrt_marco *m = pdcrt_crear_marco(ctx, 3, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, pdcrt_objeto_tabla(yo.tabla)); // yo_tbl
    pdcrt_fijar_local(ctx, m, 1, msj); // msj
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_booleano(false)); // esAutoejecutable

    pdcrt_eliminar_elementos(ctx, inic, 2); // elimina yo y msj
    return pdcrt_recv_espacio_de_nombres_k1(ctx, m);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k1);
    pdcrt_obj yo_tbl = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_empujar(ctx, yo_tbl);
    pdcrt_empujar(ctx, msj);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_recv_espacio_de_nombres_k2);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k2);
    // [tupla(valor,esAuto)]
    pdcrt_duplicar(ctx, m, -1);
    // [tupla, tupla]
    pdcrt_empujar_entero(ctx, m, 1);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_recv_espacio_de_nombres_k3);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k3);
    pdcrt_obj yo_tbl = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    (void) yo_tbl;
    (void) msj;
    // [tupla, esAuto]
    bool ok;
    bool esAutoejecutable = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, "se esperaba un booleano como 'esAutoejecutable' del espacio de nombres");
    (void) pdcrt_sacar(ctx);
    pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_booleano(esAutoejecutable));
    // [tupla]
    pdcrt_empujar_entero(ctx, m, 0);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_recv_espacio_de_nombres_k4);
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k4);
    // [args..., valor]
    pdcrt_obj esAutoejecutable = pdcrt_obtener_local(ctx, m, 2);
    if(!esAutoejecutable.bval)
    {
        if(m->args != 0)
            pdcrt_error(ctx, "tratando de llamar a valor exportado no autoejecutable");
        return pdcrt_devolver(ctx, m, 1);
    }
    else
    {
        pdcrt_insertar(ctx, -(1 + (pdcrt_stp) m->args));
        return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, NULL, m->args,
                                    &pdcrt_recv_espacio_de_nombres_k5);
    }
}

static pdcrt_k pdcrt_recv_espacio_de_nombres_k5(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_espacio_de_nombres_k5);
    return pdcrt_devolver(ctx, m, 1);
}

static pdcrt_k pdcrt_corrutina_generar(pdcrt_ctx *ctx, int args, pdcrt_k k);
static pdcrt_k pdcrt_recv_corrutina_avanzar_k1(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_corrutina(pdcrt_ctx *ctx, int args, pdcrt_k k)
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
            pdcrt_error(ctx, "Corrutina: comoTexto no necesita argumentos");
        pdcrt_extender_pila(ctx, k.marco, 1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Corrutina: %p", yo.coro);
        pdcrt_empujar_texto_cstr(ctx, &k.marco, buffer);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.igual)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Corrutina: igualA / operador_= necesita 1 argumento");

        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CORRUTINA)
        {
            pdcrt_empujar_booleano(ctx, k.marco, false);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.coro == arg.coro);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.distinto)
            || pdcrt_comparar_textos(msj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Corrutina: distintoDe / operador_no= necesita 1 argumento");

        pdcrt_extender_pila(ctx, k.marco, 1);
        pdcrt_obj arg = ctx->pila[argp];
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CORRUTINA)
        {
            pdcrt_empujar_booleano(ctx, k.marco, true);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
        else
        {
            pdcrt_empujar_booleano(ctx, k.marco, yo.coro != arg.coro);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k);
        }
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.finalizada))
    {
        if(args != 0)
            pdcrt_error(ctx, "Corrutina: finalizada no necesita argumentos");
        pdcrt_empujar_booleano(ctx, k.marco, yo.coro->estado == PDCRT_CORO_FINALIZADA);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k);
    }
    else if(pdcrt_comparar_textos(msj.texto, ctx->textos_globales.avanzar))
    {
        if(args != 0 && args != 1)
            pdcrt_error(ctx, "Corrutina: avanzar necesita 0 o 1 argumentos");

        pdcrt_obj kval;
        if(args == 1)
            kval = ctx->pila[argp];
        else
            kval = pdcrt_objeto_nulo();

        if(yo.coro->estado == PDCRT_CORO_INICIAL)
        {
            pdcrt_extender_pila(ctx, k.marco, 4);
            pdcrt_empujar(ctx, yo.coro->punto_de_inicio);
            pdcrt_marco *m = pdcrt_crear_marco(ctx, 1, 0, 0, k);
            pdcrt_fijar_local(ctx, m, 0, yo);
            yo.coro->estado = PDCRT_CORO_EJECUTANDOSE;
            yo.coro->punto_de_continuacion = k;
            pdcrt_empujar(ctx, yo);
            pdcrt_empujar_closure(ctx, &m, &pdcrt_corrutina_generar, 1);
            pdcrt_empujar(ctx, kval);
            PDCRT_SACAR_PRELUDIO();
            static const int proto[] = {0, 0};
            return pdcrt_enviar_mensaje(ctx, m, "llamar", 6, proto, 2, &pdcrt_recv_corrutina_avanzar_k1);
        }
        else if(yo.coro->estado == PDCRT_CORO_SUSPENDIDA)
        {
            pdcrt_k sus = yo.coro->punto_de_suspencion;
            yo.coro->estado = PDCRT_CORO_EJECUTANDOSE;
            yo.coro->punto_de_continuacion = k;
            pdcrt_extender_pila(ctx, k.marco, 1);
            pdcrt_empujar(ctx, kval);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, sus);
        }
        else
        {
            pdcrt_error(ctx, "No se puede avanzar una corrutina que se esta ejecutando o esta finalizada");
        }
    }

    assert(0 && "sin implementar");
}

static pdcrt_k pdcrt_corrutina_generar(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    if(args != 1)
        pdcrt_error(ctx, "Corrutina: generador debe llamarse con un argumento");
    pdcrt_marco *m = pdcrt_crear_marco(ctx, 1, 1, args, k);
    pdcrt_obj obj_coro = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_corrutina *coro = obj_coro.coro;
    // [res]
    if(coro->estado != PDCRT_CORO_EJECUTANDOSE)
    {
        pdcrt_error(ctx, "Corrutina: no se puede generar un valor para una corrutina que no se esta ejecutando");
    }
    pdcrt_k coro_k = coro->punto_de_continuacion;
    coro->estado = PDCRT_CORO_SUSPENDIDA;
    coro->punto_de_suspencion = k;
    return pdcrt_continuar(ctx, coro_k);
}

static pdcrt_k pdcrt_recv_corrutina_avanzar_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_recv_corrutina_avanzar_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    if(yo.coro->estado != PDCRT_CORO_EJECUTANDOSE)
    {
        pdcrt_error(ctx, "No se puede devolver de una corrutina que no se esta ejecutando");
    }
    yo.coro->estado = PDCRT_CORO_FINALIZADA;
    return pdcrt_devolver(ctx, m, 1);
}

static pdcrt_k pdcrt_instancia_k1(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_instancia_k2(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_instancia_k3(pdcrt_ctx *ctx, pdcrt_marco *m);
static pdcrt_k pdcrt_instancia_k4(pdcrt_ctx *ctx, pdcrt_marco *m);

pdcrt_k pdcrt_recv_instancia(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    pdcrt_marco *m = pdcrt_crear_marco(ctx, 2, 0, args, k);
    pdcrt_fijar_local(ctx, m, 0, yo);
    pdcrt_fijar_local(ctx, m, 1, msj);
    pdcrt_eliminar_elementos(ctx, (pdcrt_stp) inic, 2);
    // [...#args]

    pdcrt_extender_pila(ctx, m, 2);
    pdcrt_empujar(ctx, yo.inst->metodos);
    pdcrt_empujar(ctx, msj);
    static const int proto[] = {0};
    return pdcrt_enviar_mensaje(ctx, m, "contiene", 8, proto, 1, &pdcrt_instancia_k1);
}

static pdcrt_k pdcrt_instancia_k1(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);

    // [...#args, contiene?]
    bool ok = false;
    bool res = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Se esperaba booleano al llamar a #contiene en los métodos de una instancia");
    (void) pdcrt_sacar(ctx);

    if(res)
    {
        // Obtén y llama al método
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.inst->metodos);
        pdcrt_empujar(ctx, msj);
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_instancia_k2);
    }
    else
    {
        // Llama al metodo_no_encontrado

        if(pdcrt_tipo_de_obj(yo.inst->metodo_no_encontrado) == PDCRT_TOBJ_BOOLEANO)
        {
            if(yo.inst->metodo_no_encontrado.bval)
            {
                pdcrt_extender_pila(ctx, m, 2);
                pdcrt_empujar(ctx, yo.inst->metodos);
                pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.mensaje_no_encontrado));
                static const int proto[] = {0};
                return pdcrt_enviar_mensaje(ctx, m, "contiene", 8, proto, 1, &pdcrt_instancia_k3);
            }
            else
            {
                pdcrt_inspeccionar_texto(msj.texto);
                pdcrt_error(ctx, u8"Método no encontrado");
            }
        }
        else
        {
            pdcrt_extender_pila(ctx, m, 2);
            pdcrt_empujar(ctx, yo.inst->metodo_no_encontrado);
            pdcrt_empujar(ctx, yo);
            pdcrt_empujar(ctx, msj);
            pdcrt_mover_a_cima(ctx, m, -(m->args + 3), m->args);
            // [metodo_no_encontrado, yo, msj, ...#args]
            return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 2, m->k.kf);
        }
    }
}

static pdcrt_k pdcrt_instancia_k2(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k2);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    (void) msj;
    // [...#args, método]
    pdcrt_empujar(ctx, yo);
    pdcrt_mover_a_cima(ctx, m, -(m->args + 2), m->args);
    // [metodo, yo, ...#args]
    return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 1, m->k.kf);
}

static pdcrt_k pdcrt_instancia_k3(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k3);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    // [...#args, contiene?]
    bool ok = false;
    bool res = pdcrt_obtener_booleano(ctx, -1, &ok);
    if(!ok)
        pdcrt_error(ctx, u8"Se esperaba booleano al llamar a #contiene en los métodos de una instancia");
    (void) pdcrt_sacar(ctx);
    if(res)
    {
        pdcrt_extender_pila(ctx, m, 2);
        pdcrt_empujar(ctx, yo.inst->metodos);
        pdcrt_empujar(ctx, pdcrt_objeto_texto(ctx->textos_globales.mensaje_no_encontrado));
        static const int proto[] = {0};
        return pdcrt_enviar_mensaje(ctx, m, "en", 2, proto, 1, &pdcrt_instancia_k4);
    }
    else
    {
        pdcrt_inspeccionar_texto(msj.texto);
        pdcrt_error(ctx, u8"Método no encontrado");
    }
}

static pdcrt_k pdcrt_instancia_k4(pdcrt_ctx *ctx, pdcrt_marco *m)
{
    PDCRT_K(pdcrt_instancia_k4);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj msj = pdcrt_obtener_local(ctx, m, 1);
    // [...#args, método]
    pdcrt_empujar(ctx, yo);
    pdcrt_empujar(ctx, msj);
    pdcrt_mover_a_cima(ctx, m, -(m->args + 3), m->args);
    // [metodo, yo, msj, ...#args]
    return pdcrt_enviar_mensaje(ctx, m->k.marco, "llamar", 6, NULL, m->args + 2, m->k.kf);
}

pdcrt_k pdcrt_recv_reubicado(pdcrt_ctx *ctx, int args, pdcrt_k k)
{
    // [yo, msj, ...#args]
    size_t inic = PDCRT_CALC_INICIO();
    size_t argp = inic + 2;
    pdcrt_obj yo = ctx->pila[inic];
    pdcrt_obj msj = ctx->pila[inic + 1];
    pdcrt_debe_tener_tipo(ctx, msj, PDCRT_TOBJ_TEXTO);
    (void) argp;
    (void) yo;
    (void) k;

    assert(0 && "sin implementar");
}
