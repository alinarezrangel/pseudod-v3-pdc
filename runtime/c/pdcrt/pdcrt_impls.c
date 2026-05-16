//
// Created by alinarezrangel on 15/3/26.
//

#define PDCRT_INTERNO
#include <unistd.h>

#include "pdcrt.h"
#include "pdcrt_base.h"
#include "pdcrt_ops.h"
#include "pdcrt_impls/archivo.h"

typedef enum pdcrt_clase
{
    PDCRT_CLASE_NUMERO,
    PDCRT_CLASE_ARREGLO,
    PDCRT_CLASE_BOOLE,
    PDCRT_CLASE_PROCEDIMIENTO,
    PDCRT_CLASE_TIPO_NULO,
    PDCRT_CLASE_TEXTO,
    PDCRT_CLASE_TABLA,
} pdcrt_clase;

static pdcrt_tk pdcrt_recv_fallback_a_clase(pdcrt_ctx *ctx, int args, pdcrt_k k, pdcrt_clase clase, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE1(fallback_a_clase, clase);

    pdcrt_obj oclase;
    switch(clase)
    {
    case PDCRT_CLASE_NUMERO:
        oclase = ctx->clase_numero;
        break;
    case PDCRT_CLASE_ARREGLO:
        oclase = ctx->clase_arreglo;
        break;
    case PDCRT_CLASE_BOOLE:
        oclase = ctx->clase_boole;
        break;
    case PDCRT_CLASE_PROCEDIMIENTO:
        oclase = ctx->clase_procedimiento;
        break;
    case PDCRT_CLASE_TIPO_NULO:
        oclase = ctx->clase_tipo_nulo;
        break;
    case PDCRT_CLASE_TEXTO:
        oclase = ctx->clase_texto;
        break;
    case PDCRT_CLASE_TABLA:
        oclase = ctx->clase_tabla;
        break;
    }

    if(pdcrt_tipo_de_obj(oclase) == PDCRT_TOBJ_NULO)
    {
        pdcrt_inspeccionar_texto(omsj.texto);
        pdcrt_error(ctx, "Método no encontrado");
    }
    pdcrt_debe_tener_tipo(ctx, oclase, PDCRT_TOBJ_INSTANCIA);

    if(oclase.inst->num_atributos != 6)
        pdcrt_error(ctx, "La clase debe tener 6 atributos");

    pdcrt_obj metodos_inst = oclase.inst->atributos[2];
    pdcrt_debe_tener_tipo(ctx, metodos_inst, PDCRT_TOBJ_TABLA);

    pdcrt_obj metodo = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx, metodos_inst.tabla, omsj, &metodo);
    if(contiene)
    {
        if(args >= 6)
        {
            pdcrt_extender_pila(ctx, 1);
            pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a6));
            pdcrt_insertar(ctx, argp);
        }
        return pdcrt_llamarnr(ctx, k.marco, k.kf, args + 1,
            pdcrt_xmm_desde_obj(metodo),
            pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
            yo, a1, a2, a3, a4, a5);
    }
    else
    {
        pdcrt_inspeccionar_texto(omsj.texto);
        pdcrt_error(ctx, "Método no encontrado");
    }
}

pdcrt_tk pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_entero);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (entero): comoTexto no acepta argumentos");
        PDCRT_DEFINE_RAICES(1);
        static_assert(sizeof(pdcrt_entero) <= 64, "pdcrt_entero no debe tener más de 64 bits");
        char texto[21]; // 64 bits => máx. 20 caracteres + '\0'
        int len = snprintf(texto, sizeof(texto), "%" PDCRT_ENTERO_PRId, oyo.ival);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), texto, len));
        PDCRT_CARGAR_RAIZ_K(0, k);
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
        PDCRT_DEFINE_RAICES(1);
        unsigned char c = (unsigned char) oyo.ival;
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), (const char *) &c, 1));
        PDCRT_CARGAR_RAIZ_K(0, k);
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

    PDCRT_PROBE0(recv_float);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): comoTexto no acepta argumentos");
        PDCRT_DEFINE_RAICES(1);
        char texto[30];
        int len = snprintf(texto, sizeof(texto), "%g", (double) oyo.fval);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), texto, len));
        PDCRT_CARGAR_RAIZ_K(0, k);
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
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
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
    // =========== Desde aquí todo está sin cambiar: =============
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.multiplicar)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_por))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al multiplicar se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_float(oyo.fval * ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(oyo.fval * otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden multiplicar dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.dividir)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_entre))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): al dividir se debe especificar un argumento");
        pdcrt_obj otro = pdcrt_obj_desde_xmm(a1);
        pdcrt_tipo t_otro = pdcrt_tipo_de_obj(otro);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(t_otro == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_float(oyo.fval / ((pdcrt_float) otro.ival));
        else if(t_otro == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_float(oyo.fval / otro.fval);
        else
            pdcrt_error(ctx, u8"Numero (float): solo se pueden dividir dos números");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_= / igualA necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_booleano(oyo.fval == arg.fval);
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_booleano(pdcrt_comparar_entero_y_float(arg.ival, oyo.fval, PDCRT_IGUAL_A));
        else
            res = pdcrt_objeto_booleano(false);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Numero (float): operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)
            res = pdcrt_objeto_booleano(oyo.fval != arg.fval);
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)
            res = pdcrt_objeto_booleano(!pdcrt_comparar_entero_y_float(arg.ival, oyo.fval, PDCRT_IGUAL_A));
        else
            res = pdcrt_objeto_booleano(true);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
#define PDCRT_COMPARAR_FLOAT(m, opm, ms, opms, rcmp, op)                \
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.m)    \
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.opm)) \
    {                                                                   \
        if(args != 1)                                                   \
            pdcrt_error(ctx, "Numero (float): "opms" / "ms" necesitan 1 argumento"); \
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);                        \
        pdcrt_obj res = pdcrt_objeto_nulo();                            \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                  \
            res = pdcrt_objeto_booleano(oyo.fval op arg.fval);          \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)            \
            res = pdcrt_objeto_booleano(pdcrt_comparar_entero_y_float(arg.ival, oyo.fval, rcmp)); \
        else                                                            \
            pdcrt_error(ctx, u8"Numero (float): "opms" / "ms" solo pueden comparar dos números"); \
        PDCRT_SACAR_PRELUDIO();                                         \
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));                                 \
    }
    PDCRT_COMPARAR_FLOAT(menor_que, operador_menor_que, "menorQue", "operador_<", PDCRT_MAYOR_O_IGUAL_A, <)
    PDCRT_COMPARAR_FLOAT(mayor_que, operador_mayor_que, "mayorQue", "operador_>", PDCRT_MENOR_O_IGUAL_A, >)
    PDCRT_COMPARAR_FLOAT(menor_o_igual_a, operador_menor_o_igual_a, "menorOIgualA", "operador_=<", PDCRT_MAYOR_QUE, <=)
    PDCRT_COMPARAR_FLOAT(mayor_o_igual_a, operador_mayor_o_igual_a, "mayorOIgualA", "operador_>=", PDCRT_MENOR_QUE, >=)
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.negar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): negar no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_float(-oyo.fval)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.piso))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): piso no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_float(PDCRT_FLOAT_FLOOR(oyo.fval))));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.techo))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): techo no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_float(PDCRT_FLOAT_CEIL(oyo.fval))));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.truncar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): truncar no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_float(PDCRT_FLOAT_TRUNC(oyo.fval))));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.byte_como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): byteComoTexto no acepta argumentos");
        PDCRT_DEFINE_RAICES(1);
        char c = (char) (unsigned char) oyo.fval;
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj res = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), &c, 1));
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.invertir))
    {
        if(args != 0)
            pdcrt_error(ctx, "Numero (float): invertir no acepta argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(~(pdcrt_entero) oyo.fval)));
    }
#define PDCRT_OPERADOR_BIT(txt, nm, op)                                              \
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.txt))             \
    {                                                                                \
        if(args != 1)                                                                \
            pdcrt_error(ctx, "Numero (float): "nm" acepta solo un argumento");       \
        pdcrt_obj res = pdcrt_objeto_nulo();                                         \
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);                                     \
        if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_ENTERO)                              \
            res = pdcrt_objeto_entero(((pdcrt_entero) oyo.fval) op arg.ival);        \
        else if(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_FLOAT)                          \
            res = pdcrt_objeto_entero(((pdcrt_entero) oyo.fval) op (pdcrt_entero) arg.fval); \
        else                                                                         \
            pdcrt_error(ctx, "Argumento de tipo inesperado");                        \
        PDCRT_SACAR_PRELUDIO();                                                      \
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));                     \
    }
    PDCRT_OPERADOR_BIT(operador_bitand, "operador_<*>", &)
    PDCRT_OPERADOR_BIT(operador_bitor, "operador_<+>", |)
    PDCRT_OPERADOR_BIT(operador_bitxor, "operador_<^>", ^)
    PDCRT_OPERADOR_BIT(operador_bitlshift, "operador_<<", <<)
    PDCRT_OPERADOR_BIT(operador_bitrshift, "operador_>>", >>)
#undef PDCRT_OPERADOR_BIT

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_NUMERO, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_booleano);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Booleano: comoTexto no acepta argumentos");
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(oyo.bval)
            res = pdcrt_objeto_texto(ctx->textos_globales.verdadero);
        else
            res = pdcrt_objeto_texto(ctx->textos_globales.falso);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_= / igualA necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
            res = pdcrt_objeto_booleano(false);
        else
            res = pdcrt_objeto_booleano(oyo.bval == arg.bval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_BOOLEANO)
            res = pdcrt_objeto_booleano(true);
        else
            res = pdcrt_objeto_booleano(oyo.bval != arg.bval);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.escoger))
    {
        if(args != 2)
            pdcrt_error(ctx, "Booleano: escoger necesita 2 argumentos");
        pdcrt_obj siVerdadero = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj siFalso = pdcrt_obj_desde_xmm(a2);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(oyo.bval ? siVerdadero : siFalso));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.llamarSegun)
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.llamarSegun2))
    {
        if(args != 2)
            pdcrt_error(ctx, u8"Booleano: llamarSegún necesita 2 argumentos");
        pdcrt_obj siVerdadero = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj siFalso = pdcrt_obj_desde_xmm(a2);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_llamar0(ctx, k.marco, k.kf,
            pdcrt_xmm_desde_obj(oyo.bval ? siVerdadero : siFalso),
            pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.o)
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_o))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: \"||\" necesita 1 argumento");
        pdcrt_obj v = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.bval || v.bval)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.y)
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_y))
    {
        if(args != 1)
            pdcrt_error(ctx, "Booleano: \"&&\" necesita 1 argumento");
        pdcrt_obj v = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, v, PDCRT_TOBJ_BOOLEANO);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.bval && v.bval)));
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_BOOLE, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
    (void) args;
    (void) k;
    PDCRT_ASSERT(0 && "sin implementar");
}

static bool pdcrt_es_digito(char c)
{
    return c >= '0' && c <= '9';
}

pdcrt_tk pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_texto);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.concatenar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: concatenar necesita 1 argumento");
        PDCRT_DEFINE_RAICES(1);
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_TEXTO);
        size_t bufflen = oyo.texto->longitud + arg.texto->longitud;
        // TODO Optimiza esto
        char *buff = pdcrt_alojar_ctx(ctx, bufflen);
        if(!buff)
            pdcrt_enomem(ctx);
        memcpy(buff, oyo.texto->contenido, oyo.texto->longitud);
        memcpy(buff + oyo.texto->longitud, arg.texto->contenido, arg.texto->longitud);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj res = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), buff, bufflen));
        PDCRT_CARGAR_RAIZ_K(0, k);
        pdcrt_desalojar_ctx(ctx, buff, bufflen);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_= / igualA necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
            res = pdcrt_objeto_booleano(false);
        else
            res = pdcrt_objeto_booleano(pdcrt_comparar_textos(oyo.texto, arg.texto));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj res = pdcrt_objeto_nulo();
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_TEXTO)
            res = pdcrt_objeto_booleano(true);
        else
            res = pdcrt_objeto_booleano(!pdcrt_comparar_textos(oyo.texto, arg.texto));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.menor_que)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_menor_que))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: menorQue / operador_< necesita 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_TEXTO);
        if(oyo.texto->longitud < arg.texto->longitud)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(true)));
        }
        else if(oyo.texto->longitud > arg.texto->longitud)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(false)));
        }
        else
        {
            bool menor = true;
            for(size_t i = 0; i < oyo.texto->longitud; i++)
            {
                if(oyo.texto->contenido[i] >= arg.texto->contenido[i])
                {
                    menor = false;
                    break;
                }
            }
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(menor)));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_numero_entero))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroEntero no necesita argumentos");
        const char* s = oyo.texto->contenido;
        if(*s == '-')
            s += 1;
        for(; *s; s++)
            if(!pdcrt_es_digito(*s))
                goto error_como_entero;

        pdcrt_entero i;
        i = strtoll(oyo.texto->contenido, NULL, 10);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(i)));
    error_como_entero:
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_numero_real))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoNumeroReal no necesita argumentos");
        const char* s = oyo.texto->contenido;
        if(*s == '-')
            s += 1;
        bool dot = false;
        for(; *s; s++)
            if(*s == '.' && !dot)
                dot = true;
            else if((*s == '.' && dot) || !pdcrt_es_digito(*s))
                goto error_como_real;

        pdcrt_float f;
        f = (pdcrt_float) strtold(oyo.texto->contenido, NULL);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_float(f)));
    error_como_real:
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: comoTexto no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, yo);
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Texto: longitud no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.texto->longitud)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: en necesita 1 argumento");
        PDCRT_DEFINE_RAICES(2);
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: en necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= oyo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #en");
        PDCRT_GUARDAR_RAIZ_K(0, k);
        PDCRT_GUARDAR_RAIZ(1, oyo);
        pdcrt_obj res = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), oyo.texto->contenido + i, 1));
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_CARGAR_RAIZ(1, oyo);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.byte_en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Texto: byteEn necesita 1 argumento");
        bool ok = false;
        pdcrt_entero i = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: byteEn necesita un entero como argumento");
        if(i < 0 || ((size_t) i) >= oyo.texto->longitud)
            pdcrt_error(ctx, "Texto: entero fuera de rango pasado a #byteEn");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.texto->contenido[i])));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.subtexto))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 argumentos");

        bool ok = false;
        pdcrt_entero inicio, longitud;
        inicio = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 enteros como argumentos");
        longitud = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a2), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: subTexto necesita 2 enteros como argumentos");

        if(inicio < 0 || (size_t) inicio > oyo.texto->longitud)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el primer argumento de #subTexto");
        if(longitud < 0)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el segundo argumento de #subTexto");

        if((size_t) (inicio + longitud) > oyo.texto->longitud)
            longitud = oyo.texto->longitud - inicio;

        pdcrt_obj res = pdcrt_objeto_nulo();
        if(longitud == 0)
        {
            res = pdcrt_objeto_texto(ctx->textos_globales.texto_vacio);
        }
        else
        {
            PDCRT_DEFINE_RAICES(1);
            char *buffer = pdcrt_alojar_ctx(ctx, longitud);
            PDCRT_ASSERT(buffer);
            memcpy(buffer, oyo.texto->contenido + inicio, longitud);
            PDCRT_GUARDAR_RAIZ_K(0, k);
            res = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), buffer, longitud));
            PDCRT_CARGAR_RAIZ_K(0, k);
            pdcrt_desalojar_ctx(ctx, buffer, longitud);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.parte_del_texto))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 argumentos");
        pdcrt_extender_pila(ctx, 1);

        bool ok = false;
        pdcrt_entero inicio, final;
        inicio = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 enteros como argumentos");
        final = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a2), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: parteDelTexto necesita 2 enteros como argumentos");

        if(inicio < 0 || (size_t) inicio > oyo.texto->longitud)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el primer argumento de #parteDelTexto");
        if(final < 0)
            pdcrt_error(ctx, "Texto: valor fuera de rango para el segundo argumento de #parteDelTexto");
        if((size_t) final > oyo.texto->longitud)
            final = oyo.texto->longitud;

        pdcrt_obj res = pdcrt_objeto_nulo();
        if(final <= inicio)
        {
            res = pdcrt_objeto_texto(ctx->textos_globales.texto_vacio);
        }
        else
        {
            PDCRT_DEFINE_RAICES(1);
            char *buffer = pdcrt_alojar_ctx(ctx, final - inicio);
            PDCRT_ASSERT(buffer);
            memcpy(buffer, oyo.texto->contenido + inicio, final - inicio);
            PDCRT_GUARDAR_RAIZ_K(0, k);
            res = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), buffer, final - inicio));
            PDCRT_CARGAR_RAIZ_K(0, k);
            pdcrt_desalojar_ctx(ctx, buffer, final - inicio);
        }
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.buscar))
    {
        if(args != 2)
            pdcrt_error(ctx, "Texto: buscar necesita 2 argumentos");
        pdcrt_extender_pila(ctx, 1);

        /* Algoritmo Knuth-Morris-Pratt, sacado de wikipedia:
         * <https://en.wikipedia.org/wiki/Knuth%E2%80%93Morris%E2%80%93Pratt_algorithm>
         * el 2024-05-05.
         */

        bool ok = false;
        pdcrt_entero desde = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: buscar necesita un entero como primer argumento");
        if(desde < 0 || (size_t) desde > oyo.texto->longitud)
            pdcrt_error(ctx, "Texto: buscar primer argumento fuera de rango");
        size_t buffer_len = pdcrt_obtener_tam_texto_obj(ctx, pdcrt_obj_desde_xmm(a2), &ok);
        if(!ok)
            pdcrt_error(ctx, "Texto: buscar necesita un texto como segundo argumento");

        if(buffer_len == 0)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(desde)));
        }

        char *buffer = pdcrt_obj_desde_xmm(a2).texto->contenido;
        ssize_t *skip_table = pdcrt_alojar_ctx(ctx, buffer_len * sizeof(ssize_t));
        PDCRT_ASSERT(skip_table);

        pdcrt_obj res = pdcrt_objeto_nulo();

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

        while(yo_pos < oyo.texto->longitud)
        {
            if(oyo.texto->contenido[yo_pos] == buffer[buffer_pos])
            {
                buffer_pos += 1;
                yo_pos += 1;
                if(buffer_pos >= buffer_len)
                {
                    res = pdcrt_objeto_entero(yo_pos - buffer_pos);
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

        res = pdcrt_objeto_nulo();
    buscar_encontrado:

        pdcrt_desalojar_ctx(ctx, skip_table, buffer_len * sizeof(ssize_t));

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(res));
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TEXTO, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    (void) yo;
    PDCRT_PROBE0(recv_nulo);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_= / igualA necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(pdcrt_tipo_de_obj(arg) == PDCRT_TOBJ_NULO)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Nulo: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_NULO)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Nulo: comoTexto no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.nulo_como_texto)));
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TIPO_NULO, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_arreglo(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_arreglo);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: operador_= / igualA necesitan 1 argumento");
        msj = pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.igual));
        return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_ARREGLO, PDCRT_A_IMM);
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: operador_no= / distintoDe necesitan 1 argumento");
        msj = pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.distinto));
        return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_ARREGLO, PDCRT_A_IMM);
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Arreglo: comoTexto no necesita argumentos");
        msj = pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.como_texto));
        return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_ARREGLO, PDCRT_A_IMM);
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: en necesita un argumento");
        bool ok;
        pdcrt_entero i = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Arreglo: en necesita un entero como argumento");
        if(i < 0 || (size_t) i >= oyo.arreglo->longitud)
            pdcrt_error(ctx, "Arreglo: en: indice fuera de rango");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(oyo.arreglo->valores[i]));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijarEn))
    {
        if(args != 2)
            pdcrt_error(ctx, "Arreglo: fijarEn necesita 2 argumentos");
        bool ok;
        pdcrt_entero i = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Arreglo: fijarEn necesita un entero como argumento");
        if(i < 0 || (size_t) i >= oyo.arreglo->longitud)
            pdcrt_error(ctx, "Arreglo: fijarEn: indice fuera de rango");
        pdcrt_obj val = pdcrt_obj_desde_xmm(a2);
        pdcrt_barrera_de_escritura(ctx, oyo, val);
        oyo.arreglo->valores[i] = val;
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Arreglo: longitud no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.arreglo->longitud)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.unir))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: unir necesita un argumento");
        PDCRT_DEFINE_RAICES(1);
        pdcrt_obj separador = pdcrt_obj_desde_xmm(a1);
        if(pdcrt_tipo_de_obj(separador) != PDCRT_TOBJ_TEXTO)
            pdcrt_error(ctx, "Arreglo: el argumento de unir debe ser un texto");
        size_t tam_final = 0;
        for(size_t i = 0; i < oyo.arreglo->longitud; i++)
        {
            pdcrt_obj el = oyo.arreglo->valores[i];
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

        for(size_t i = 0; i < oyo.arreglo->longitud; i++)
        {
            if(i > 0)
            {
                if(separador.texto->longitud > 0)
                    memcpy(buffer + cur, separador.texto->contenido,
                           separador.texto->longitud);
                cur += separador.texto->longitud;
            }
            pdcrt_obj el = oyo.arreglo->valores[i];
            if(el.texto->longitud > 0)
                memcpy(buffer + cur, el.texto->contenido, el.texto->longitud);
            cur += el.texto->longitud;
        }

        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto(ctx, PDCRT_GC(), buffer, tam_final));
        PDCRT_CARGAR_RAIZ_K(0, k);
        pdcrt_desalojar_ctx(ctx, buffer, tam_final);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.agregar_al_final))
    {
        if(args != 1)
            pdcrt_error(ctx, "Arreglo: agregarAlFinal necesita 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_arreglo_abrir_espacio(ctx, k.marco, oyo.arreglo, 1);
        pdcrt_barrera_de_escritura(ctx, oyo, arg);
        oyo.arreglo->valores[oyo.arreglo->longitud++] = arg;
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_ARREGLO, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_closure(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_closure);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
       || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Procedimiento: operador_= / igualA necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CLOSURE)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(false)));
        }
        else
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k,
                pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.closure == arg.closure)));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Procedimiento: operador_no= / distintoDe necesitan 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CLOSURE)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(true)));
        }
        else
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k,
                pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.closure != arg.closure)));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Procedimiento: comoTexto no necesita argumentos");
        PDCRT_DEFINE_RAICES(1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Procedimiento: %p", oyo.closure);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto_desde_cstr(ctx, PDCRT_GC(), buffer));
        PDCRT_CARGAR_RAIZ_K(0, k);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.llamar))
    {
        return (*oyo.closure->f)(ctx, args, k, PDCRT_A_IMM);
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_PROCEDIMIENTO, PDCRT_A_IMM);
}

pdcrt_tk pdcrt_recv_caja(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
    (void) yo;
    (void) args;
    (void) k;

    PDCRT_ASSERT(0 && "sin implementar");
}


static pdcrt_tk pdcrt_tabla_para_cada_par_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res);

pdcrt_tk pdcrt_recv_tabla(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_tabla);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijarEn))
    {
        if(args != 2)
            pdcrt_error(ctx, "Tabla: fijarEn necesita 2 argumentos");
        pdcrt_obj llave = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj valor = pdcrt_obj_desde_xmm(a2);
        pdcrt_tabla_fijar(ctx, oyo.tabla, llave, valor, true);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.en))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: en necesita 1 argumento");
        pdcrt_obj llave = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj valor = pdcrt_objeto_nulo();
        if(!pdcrt_tabla_en(ctx, oyo.tabla, llave, &valor))
            pdcrt_error(ctx, "Llave no existe en la tabla primitiva");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(valor));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.longitud))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: longitud no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_entero(oyo.tabla->buckets_ocupados)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.rehashear))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: rehashear necesita 1 argumento");
        bool ok;
        pdcrt_entero capacidad_adicional = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Tabla#rehashear: necesita un entero como argumento");

        if(capacidad_adicional < 0)
            pdcrt_error(ctx, u8"Tabla#rehashear: Valor inválido para la capacidad adicional");

        pdcrt_tabla_rehashear(ctx, oyo.tabla, oyo.tabla->buckets_ocupados + capacidad_adicional);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.capacidad))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: capacidad no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_entero(pdcrt_tabla_num_buckets_hasheables(oyo.tabla->mascara))));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.contiene))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: contiene necesita 1 argumento");
        pdcrt_obj llave = pdcrt_obj_desde_xmm(a1);
        pdcrt_obj valor = pdcrt_objeto_nulo();
        bool contiene = pdcrt_tabla_en(ctx, oyo.tabla, llave, &valor);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(contiene)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.eliminar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: eliminar necesita 1 argumento");
        pdcrt_obj llave = pdcrt_obj_desde_xmm(a1);
        pdcrt_tabla_eliminar(ctx, oyo.tabla, llave, true);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.vaciar))
    {
        if(args != 0)
            pdcrt_error(ctx, "Tabla: vaciar no necesita argumentos");
        pdcrt_tabla_vaciar(ctx, oyo.tabla, false);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.paraCadaPar))
    {
        if(args != 1)
            pdcrt_error(ctx, "Tabla: paraCadaPar necesita 1 argumento");
        PDCRT_DEFINE_RAICES(1);
        PDCRT_GUARDAR_RAIZ_XMM(0, a1);
        pdcrt_marco *m = pdcrt_crear_marco(ctx, PDCRT_GC(), 3, 0, k, NULL);
        PDCRT_CARGAR_RAIZ_XMM(0, a1);

        pdcrt_fijar_local(ctx, m, 0, oyo);
        pdcrt_fijar_local(ctx, m, 1, pdcrt_obj_desde_xmm(a1));
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(0));
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_tabla_para_cada_par_k1(ctx, m, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }

    return pdcrt_recv_fallback_a_clase(ctx, args, k, PDCRT_CLASE_TABLA, PDCRT_A_IMM);
}

static pdcrt_tk pdcrt_tabla_para_cada_par_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res)
{
    PDCRT_K(pdcrt_tabla_para_cada_par_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    pdcrt_obj iterador = pdcrt_obtener_local(ctx, m, 1);
    pdcrt_obj oi = pdcrt_obtener_local(ctx, m, 2);
    pdcrt_entero i = oi.ival;

    if(i < pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara))
    {
        yo = pdcrt_obtener_local(ctx, m, 0);
        iterador = pdcrt_obtener_local(ctx, m, 1);
        oi = pdcrt_obtener_local(ctx, m, 2);
        i = oi.ival;
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));

        if(yo.tabla->buckets[i].activo)
        {
            return pdcrt_llamar2(ctx, m, &pdcrt_tabla_para_cada_par_k1,
                pdcrt_xmm_desde_obj(iterador), pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
                pdcrt_xmm_desde_obj(yo.tabla->buckets[i].llave), pdcrt_xmm_desde_obj(yo.tabla->buckets[i].valor));
        }
        else
        {
            return pdcrt_tabla_para_cada_par_k1(ctx, m, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
        }
    }
    else if(i - pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara) < yo.tabla->num_colisiones)
    {
        yo = pdcrt_obtener_local(ctx, m, 0);
        iterador = pdcrt_obtener_local(ctx, m, 1);
        oi = pdcrt_obtener_local(ctx, m, 2);
        i = oi.ival;
        pdcrt_fijar_local(ctx, m, 2, pdcrt_objeto_entero(i + 1));

        size_t buckets = pdcrt_tabla_num_buckets_hasheables(yo.tabla->mascara);

        PDCRT_ASSERT(i - buckets < yo.tabla->num_colisiones);
        PDCRT_ASSERT(yo.tabla->colisiones[i - buckets].activo);

        return pdcrt_llamar2(ctx, m, &pdcrt_tabla_para_cada_par_k1,
            pdcrt_xmm_desde_obj(iterador), pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
            pdcrt_xmm_desde_obj(yo.tabla->colisiones[i - buckets].llave),
            pdcrt_xmm_desde_obj(yo.tabla->colisiones[i - buckets].valor));
    }
    else
    {
        return pdcrt_devolver1(ctx, m, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
}

pdcrt_tk pdcrt_recv_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_runtime);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: comoTexto no necesita argumentos");
        PDCRT_DEFINE_RAICES(1);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto_desde_cstr(ctx, PDCRT_GC(), "Runtime"));
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.crearTabla))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearTabla necesita 1 argumento");
        PDCRT_DEFINE_RAICES(1);
        bool ok;
        pdcrt_entero n = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, u8"Runtime: crearTabla: su único argumento debe ser un entero");
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj tbl = pdcrt_objeto_tabla(pdcrt_crear_tabla(ctx, PDCRT_GC(), n));
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(tbl));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.crearCorrutina))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearCorrutina necesita 1 argumento");
        PDCRT_DEFINE_RAICES(1);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj coro = pdcrt_objeto_corrutina(pdcrt_crear_corrutina_obj(ctx, PDCRT_GC(), pdcrt_obj_desde_xmm(a1)));
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(coro));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.recolectar_basura))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: recolectarBasura no necesita argumentos");
        PDCRT_DEFINE_RAICES(1);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_recoleccion params = pdcrt_gc_recoleccion_por_memoria(ctx, 0);
        pdcrt_recolectar_basura_simple(ctx, PDCRT_GC(), params);
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.abrir_archivo))
    {
        if(args != 2)
            pdcrt_error(ctx, "Runtime: abrirArchivo necesita 2 argumentos");

        bool ok = false;
        pdcrt_obj nombre = pdcrt_obj_desde_xmm(a1);
        size_t tam = pdcrt_obtener_tam_texto_obj(ctx, nombre, &ok);
        if(!ok)
            pdcrt_error(ctx, "abrirArchivo necesita un texto como su primer argumento (el nombre del archivo)");
        pdcrt_entero modo = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a2), &ok);
        if(!ok)
            pdcrt_error(ctx, "abrirArchivo necesita un entero como su segundo argumento (el modo del archivo)");

        pdcrt_intencion_abrir_archivo intencion = 0;
        pdcrt_accion_crear accion_crear = PDCRT_ACCION_ERROR_NUEVO;
        if(modo % 10 == 1)
        {
            accion_crear = PDCRT_ACCION_CREAR_NUEVO;
            intencion = PDCRT_ABRIR_ESCRITURA;
        }
        else
        {
            intencion = PDCRT_ABRIR_LECTURA;
        }

        modo /= 100;
        pdcrt_accion_abrir_archivo accion_abrir = PDCRT_ACCION_ABRIR_EXISTENTE;
        if(modo % 10 == 1)
            accion_abrir = PDCRT_ACCION_TRUNCAR_EXISTENTE;

        PDCRT_DEFINE_RAICES(3);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        PDCRT_GUARDAR_RAIZ_XMM(1, a1);
        PDCRT_GUARDAR_RAIZ_XMM(2, a2);
        pdcrt_valop *valop = pdcrt_crear_valop(ctx, PDCRT_GC(), sizeof(pdcrt_rsc_archivo), &pdcrt_liberar_rsc_archivo);
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_CARGAR_RAIZ_XMM(1, a1);
        PDCRT_CARGAR_RAIZ_XMM(2, a2);

        pdcrt_rsc_archivo *arch = (pdcrt_rsc_archivo *) valop->datos;
        pdcrt_opciones_abrir_archivo opciones = {
            .flags = 0,
            .nombre = { .ptr = nombre.texto->contenido, .tam = tam + 1 },
            .intencion = intencion,
            .accion_abrir = accion_abrir,
            .accion_crear = accion_crear,
            .permisos_al_crear = PDCRT_PERMISO_LEER | PDCRT_PERMISO_ESCRIBIR,
            .relativo_a = NULL,
        };
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_abrir_archivo)(ctx->vio.ctx, &arch->archivo, &opciones);
        if(ioerr != PDCRT_IO_OK)
        {
            arch->archivo = NULL;
        }
        arch->eof = false;

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_archivo(valop)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.crear_directorio))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: crearDirectorio necesita 1 argumento");
        pdcrt_obj nombre = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, nombre, PDCRT_TOBJ_TEXTO);

        pdcrt_directorio *dir = NULL;
        pdcrt_opciones_abrir_directorio opciones = {
            .relativo_a = NULL,
            .nombre = { .ptr = nombre.texto->contenido, .tam = nombre.texto->longitud + 1 },
            .accion_crear = PDCRT_ACCION_CREAR_NUEVO,
            .intencion = PDCRT_ABRIR_ITERAR,
            .flags = 0,
        };
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_abrir_directorio)(ctx->vio.ctx, &dir, &opciones);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al crear el directorio");
        ioerr = (*ctx->vio.vtable->op_cerrar_directorio)(ctx->vio.ctx, dir);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al cerrar el directorio");

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.borrar_directorio))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: borrarDirectorio necesita 1 argumento");
        pdcrt_obj nombre = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, nombre, PDCRT_TOBJ_TEXTO);

        pdcrt_vio_cadena cnombre = { .ptr = nombre.texto->contenido, .tam = nombre.texto->longitud + 1 };
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_borrar_directorio_por_ruta)(ctx->vio.ctx, NULL, cnombre);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al borrar el directorio");

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.borrar_archivo))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: borrarArchivo necesita 1 argumento");
        pdcrt_obj nombre = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, nombre, PDCRT_TOBJ_TEXTO);

        pdcrt_vio_cadena cnombre = { .ptr = nombre.texto->contenido, .tam = nombre.texto->longitud + 1 };
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_borrar_archivo_por_ruta)(ctx->vio.ctx, NULL, cnombre);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al borrar el archivo");

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_pid))
    {
        if(args != 0)
            pdcrt_error(ctx, "Runtime: obtenerPid no necesita argumentos");
        int pid = getpid(); // TODO: Mover a VIO
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(pid)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.ejecutar))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: ejecutar necesita 3 argumento");
        pdcrt_obj ejecutable = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, ejecutable, PDCRT_TOBJ_TEXTO);
        pdcrt_obj argumentos = pdcrt_obj_desde_xmm(a2);
        pdcrt_obj entorno = pdcrt_obj_desde_xmm(a3);

        pdcrt_vio_cadena *argv = NULL, cmdline = {0};
        if(pdcrt_tipo_de_obj(argumentos) == PDCRT_TOBJ_ARREGLO)
        {
            argv = malloc(sizeof(pdcrt_vio_cadena) * argumentos.arreglo->longitud);
            if(!argv)
                pdcrt_enomem(ctx);
            for(size_t i = 0; i < argumentos.arreglo->longitud; i++)
            {
                pdcrt_debe_tener_tipo(ctx, argumentos.arreglo->valores[i], PDCRT_TOBJ_TEXTO);
                pdcrt_texto *txt = argumentos.arreglo->valores[i].texto;
                argv[i] = (pdcrt_vio_cadena) { .ptr = txt->contenido, .tam = txt->longitud + 1 };
            }
        }
        else if(pdcrt_tipo_de_obj(argumentos) == PDCRT_TOBJ_TEXTO)
        {
            cmdline.ptr = argumentos.texto->contenido;
            cmdline.tam = argumentos.texto->longitud + 1;
        }
        else
        {
            pdcrt_error(ctx, "Runtime: los argumentos del proceso a ejecutar deben ser un arreglo de textos o un texto");
        }

        pdcrt_variable_de_entorno *envp = NULL;
        bool heredar_entorno = false;
        if(pdcrt_tipo_de_obj(entorno) == PDCRT_TOBJ_ARREGLO && entorno.arreglo->longitud > 0)
        {
            envp = malloc(sizeof(pdcrt_variable_de_entorno) * entorno.arreglo->longitud);
            if(!envp)
                pdcrt_enomem(ctx);

            for(size_t i = 0; i < entorno.arreglo->longitud; i++)
            {
                pdcrt_debe_tener_tipo(ctx, entorno.arreglo->valores[i], PDCRT_TOBJ_TEXTO);
                pdcrt_texto *txt = entorno.arreglo->valores[i].texto;
                size_t eq = SIZE_MAX;
                for(size_t j = 0; j < txt->longitud; j++)
                {
                    if(txt->contenido[j] == '=')
                    {
                        eq = j;
                        break;
                    }
                }

                if(eq == SIZE_MAX)
                {
                    envp[i] = (pdcrt_variable_de_entorno) {
                        .nombre = { .ptr = txt->contenido, .tam = txt->longitud },
                        .valor = { .ptr = "", .tam = 1 },
                    };
                }
                else
                {
                    envp[i] = (pdcrt_variable_de_entorno) {
                        .nombre = { .ptr = txt->contenido, .tam = eq },
                        .valor = { .ptr = txt->contenido + eq + 1, .tam = txt->longitud - eq - 1 },
                    };
                }
            }
        }
        else if(pdcrt_tipo_de_obj(entorno) == PDCRT_TOBJ_NULO)
        {
            heredar_entorno = true;
        }
        else
        {
            pdcrt_error(ctx, "Runtime: el entorno de ejecutar debe ser un arreglo de textos o NULO");
        }

        pdcrt_subproceso *sub = NULL;
        pdcrt_opciones_crear_subproceso opciones = {
            .directorio_actual = NULL,
            .ejecutable = { .ptr = ejecutable.texto->contenido, .tam = ejecutable.texto->longitud + 1 },
            .entorno = heredar_entorno ? NULL : envp,
            .tam_entorno = heredar_entorno ? 0 : entorno.arreglo->longitud,
            .heredar_entorno = heredar_entorno,
        };
        if(argv)
        {
            opciones.tipo_linea_de_comandos = PDCRT_LINEA_DE_COMANDO_UNIX;
            opciones.linea_de_comandos.como_unix.argc = argumentos.arreglo->longitud;
            opciones.linea_de_comandos.como_unix.argv = argv;
        }
        else
        {
            opciones.tipo_linea_de_comandos = PDCRT_LINEA_DE_COMANDO_WINDOWS;
            opciones.linea_de_comandos.como_windows = cmdline;
        }
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_crear_subproceso)(ctx->vio.ctx, &sub, &opciones);
        free(argv);
        free(envp);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al crear el subproceso");

        ioerr = (*ctx->vio.vtable->op_esperar_por_subproceso)(ctx->vio.ctx, sub);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al esperar por el subproceso");

        pdcrt_estado_de_subproceso estado = {0};
        ioerr = (*ctx->vio.vtable->op_estado_de_subproceso)(ctx->vio.ctx, sub, &estado);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: error al esperar por el subproceso");

        pdcrt_arreglo *arr = NULL;
        {
            PDCRT_DEFINE_RAICES(0);
            arr = pdcrt_crear_arreglo_vacio(ctx, PDCRT_GC(), 2);
        }

        if(estado.tipo == PDCRT_ESTADO_OK)
        {
            arr->valores[0] = pdcrt_objeto_texto(ctx->textos_globales.ok);
            arr->valores[1] = pdcrt_objeto_entero(estado.resultado);
        }
        else
        {
            arr->valores[0] = pdcrt_objeto_texto(ctx->textos_globales.otro);
            arr->valores[1] = pdcrt_objeto_nulo();
        }
        arr->longitud = 2;

        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_arreglo(arr)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_variable_de_entorno))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: obtenerVariableDeEntorno necesita 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, arg, PDCRT_TOBJ_TEXTO);
        pdcrt_vio_cadena nombre = {
            .ptr = arg.texto->contenido, .tam = arg.texto->longitud + 1,
        };
        size_t tam = 0;
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_obtener_variable_de_entorno)(ctx->vio.ctx, nombre, NULL, &tam);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "Runtime: no se pudo obtener la variable de entorno");
        if(tam > 0)
        {
            char *bbuf = malloc(tam);
            if(!bbuf)
                pdcrt_enomem(ctx);
            pdcrt_vio_buffer buf = { .ptr = bbuf, .cap = tam, .tam = 0 };
            ioerr = (*ctx->vio.vtable->op_obtener_variable_de_entorno)(ctx->vio.ctx, nombre, &buf, &tam);
            if(tam != buf.tam || ioerr != PDCRT_IO_OK)
                pdcrt_error(ctx, "Runtime: error al obtener la variable de entorno");

            PDCRT_DEFINE_RAICES(0);
            pdcrt_texto *txt = pdcrt_crear_texto(ctx, PDCRT_GC(), buf.ptr, buf.tam);
            free(bbuf);
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_texto(txt)));
        }
        else
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.texto_vacio)));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.crear_instancia))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: crearInstancia necesita 3 argumentos");
        PDCRT_DEFINE_RAICES(1);
        // (num_atrs, metodos, metodo_no_encontrado)
        bool ok = false;
        pdcrt_entero num_atrs = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: crearInstancia: numAtrs debe ser un entero");
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj inst = pdcrt_objeto_instancia(
            pdcrt_crear_instancia_obj(ctx, PDCRT_GC(),
                pdcrt_obj_desde_xmm(a2), pdcrt_obj_desde_xmm(a3), num_atrs));
        PDCRT_CARGAR_RAIZ_K(0, k);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(inst));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.atributos_de_instancia))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: atributosDeInstancia necesita 1 argumento");
        pdcrt_obj inst = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_entero((pdcrt_entero) inst.inst->num_atributos)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_metodos))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: obtenerMétodos necesita 1 argumento");
        pdcrt_obj inst = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(inst.inst->metodos));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_atributo))
    {
        if(args != 2)
            pdcrt_error(ctx, "Runtime: obtenerAtributo necesita 2 argumentos");
        pdcrt_obj inst = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        bool ok = false;
        pdcrt_entero atr = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a2), &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: obtenerAtributo: el atributo debe ser un entero");
        if(atr < 0 || (size_t) atr >= inst.inst->num_atributos)
            pdcrt_error(ctx, u8"Runtime: obtenerAtributo: índice de atributo inválido");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(inst.inst->atributos[atr]));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_atributo))
    {
        if(args != 3)
            pdcrt_error(ctx, "Runtime: fijarAtributo necesita 3 argumentos");
        pdcrt_obj inst = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, inst, PDCRT_TOBJ_INSTANCIA);
        bool ok = false;
        pdcrt_entero atr = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a2), &ok);
        if(!ok)
            pdcrt_error(ctx, "Runtime: fijarAtributo: el atributo debe ser un entero");
        if(atr < 0 || (size_t) atr >= inst.inst->num_atributos)
            pdcrt_error(ctx, u8"Runtime: fijarAtributo: índice de atributo inválido");
        pdcrt_obj valor = pdcrt_obj_desde_xmm(a3);
        pdcrt_barrera_de_escritura(ctx, inst, valor);
        inst.inst->atributos[atr] = valor;
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.es_instancia))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: esInstancia necesita 1 argumento");
        pdcrt_obj inst = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(pdcrt_tipo_de_obj(inst) == PDCRT_TOBJ_INSTANCIA)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.enviar_mensaje))
    {
        if(args < 2)
            pdcrt_error(ctx, "Runtime: enviarMensaje necesita al menos 2 argumentos");
        pdcrt_mover_a_cima(ctx, -(pdcrt_stp) (args <= 6 ? 0 : args - 6), 2);
        pdcrt_obj na6 = pdcrt_objeto_nulo(), na5 = pdcrt_objeto_nulo();
        if(args >= 6)
            na6 = pdcrt_sacar(ctx);
        if(args >= 5)
            na5 = pdcrt_sacar(ctx);
        return pdcrt_llamarnr(ctx, k.marco, k.kf, args - 2,
            a1, a2,
            a3, a4, a5, a6, pdcrt_xmm_desde_obj(na5), pdcrt_xmm_desde_obj(na6));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fallar_con_mensaje))
    {
        if(args != 1)
            pdcrt_error(ctx, "Runtime: fallarConMensaje necesita 1 argumento");
        // [texto]
        pdcrt_obj texto = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo(ctx, texto, PDCRT_TOBJ_TEXTO);

        puts("TRACEBACK:");
        size_t i = 0;
        for(pdcrt_marco *m = k.marco; m; m = m->k.marco)
        {
            printf("  (%zu): %s\n", i, m->debug_srcloc);
            i += 1;
        }
        pdcrt_error(ctx, texto.texto->contenido);
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.leer_caracter))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: leerCarácter no necesita argumentos");
        int c = getchar();
        if(c == EOF)
            c = -1;
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(c)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_argv))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerArgv no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(ctx->argv));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_programa))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerPrograma no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(ctx->nombre_del_programa));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_clase_objeto))
    {
        if(args != 0)
            pdcrt_error(ctx, u8"Runtime: obtenerClaseObjeto no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(ctx->clase_objeto));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_objeto))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseObjeto necesita 1 argumento");
        ctx->clase_objeto = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_arreglo))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseArreglo necesita 1 argumento");
        ctx->clase_arreglo = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_boole))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseBoole necesita 1 argumento");
        ctx->clase_boole = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_numero))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseNumero necesita 1 argumento");
        ctx->clase_numero = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_procedimiento))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseProcedimieto necesita 1 argumento");
        ctx->clase_procedimiento = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_tipo_nulo))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTipoNulo necesita 1 argumento");
        ctx->clase_tipo_nulo = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_texto))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTexto necesita 1 argumento");
        ctx->clase_texto = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fijar_clase_tabla))
    {
        if(args != 1)
            pdcrt_error(ctx, u8"Runtime: fijarClaseTabla necesita 1 argumento");
        ctx->clase_tabla = pdcrt_obj_desde_xmm(a1);
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }

    PDCRT_ASSERT(0 && "sin implementar");
}

pdcrt_tk pdcrt_recv_voidptr(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    PDCRT_PROBE0(recv_voidptr);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Voidptr: comoTexto no necesita argumentos");
        PDCRT_DEFINE_RAICES(1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Voidptr: %p", oyo.pval);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto_desde_cstr(ctx, PDCRT_GC(), buffer));
        PDCRT_CARGAR_RAIZ_K(0, k);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }

    PDCRT_ASSERT(0 && "sin implementar");
}

pdcrt_tk pdcrt_recv_valop(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    PDCRT_PROBE0(recv_valop);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Valop: comoTexto no necesita argumentos");
        PDCRT_DEFINE_RAICES(1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Valop: %p", oyo.valop);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto_desde_cstr(ctx, PDCRT_GC(), buffer));
        PDCRT_CARGAR_RAIZ_K(0, k);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }

    PDCRT_ASSERT(0 && "sin implementar");
}

pdcrt_tk pdcrt_recv_espacio_de_nombres(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);
    (void) argp;

    PDCRT_PROBE0(recv_espacio_de_nombres);

    pdcrt_obj valor = pdcrt_objeto_nulo();
    if(!pdcrt_tabla_en(ctx, oyo.tabla, omsj, &valor))
    {
        static const char stmsj[] = "Espacio de nombres no contiene el nombre '%.*s'";
        size_t dymsj_longitud = sizeof(stmsj) + omsj.texto->longitud + 1;
        char *dymsj = pdcrt_alojar_ctx(ctx, dymsj_longitud);
        int tam_real = snprintf(dymsj, dymsj_longitud, stmsj, (int) omsj.texto->longitud, omsj.texto->contenido);
        if(tam_real > 0)
            pdcrt_error(ctx, dymsj);
        else
            pdcrt_error(ctx, "Espacio de nombres no contiene el nombre; error al formatear mensaje de error");
    }

    pdcrt_debe_tener_tipo(ctx, valor, PDCRT_TOBJ_ARREGLO);
    if(valor.arreglo->longitud != 2)
        pdcrt_error(ctx, u8"Espacio de nombres inválido: no es tupla");

    pdcrt_debe_tener_tipo(ctx, valor.arreglo->valores[1], PDCRT_TOBJ_BOOLEANO);
    if(!/* esAutoejecutable */valor.arreglo->valores[1].bval)
    {
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(valor.arreglo->valores[0]));
    }
    else
    {
        return pdcrt_llamarnr(ctx, k.marco, k.kf, args,
            pdcrt_xmm_desde_obj(valor.arreglo->valores[0]),
            pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
            PDCRT_AA_IMM);
    }
}

static pdcrt_tk pdcrt_corrutina_generar(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
static pdcrt_tk pdcrt_recv_corrutina_avanzar_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res);

pdcrt_tk pdcrt_recv_corrutina(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_corrutina);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.como_texto))
    {
        if(args != 0)
            pdcrt_error(ctx, "Corrutina: comoTexto no necesita argumentos");
        PDCRT_DEFINE_RAICES(1);
#define PDCRT_MAX_LEN 32
        char *buffer = pdcrt_alojar_ctx(ctx, PDCRT_MAX_LEN);
        if(!buffer)
            pdcrt_enomem(ctx);
        snprintf(buffer, PDCRT_MAX_LEN, "Corrutina: %p", oyo.coro);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_obj txt = pdcrt_objeto_texto(pdcrt_crear_texto_desde_cstr(ctx, PDCRT_GC(), buffer));
        PDCRT_CARGAR_RAIZ_K(0, k);
        pdcrt_desalojar_ctx(ctx, buffer, PDCRT_MAX_LEN);
#undef PDCRT_MAX_LEN
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(txt));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.igual)
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_igual))
    {
        if(args != 1)
            pdcrt_error(ctx, "Corrutina: igualA / operador_= necesita 1 argumento");
        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CORRUTINA)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(false)));
        }
        else
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.coro == arg.coro)));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.distinto)
            || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.operador_distinto))
    {
        if(args != 1)
            pdcrt_error(ctx, "Corrutina: distintoDe / operador_no= necesita 1 argumento");

        pdcrt_obj arg = pdcrt_obj_desde_xmm(a1);
        if(pdcrt_tipo_de_obj(arg) != PDCRT_TOBJ_CORRUTINA)
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(true)));
        }
        else
        {
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.coro != arg.coro)));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.finalizada))
    {
        if(args != 0)
            pdcrt_error(ctx, "Corrutina: finalizada no necesita argumentos");
        PDCRT_SACAR_PRELUDIO();
        return pdcrt_continuar(ctx, k,
            pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(oyo.coro->estado == PDCRT_CORO_FINALIZADA)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.avanzar))
    {
        if(args != 0 && args != 1)
            pdcrt_error(ctx, "Corrutina: avanzar necesita 0 o 1 argumentos");

        pdcrt_obj kval;
        if(args == 1)
            kval = pdcrt_obj_desde_xmm(a1);
        else
            kval = pdcrt_objeto_nulo();

        if(oyo.coro->estado == PDCRT_CORO_INICIAL)
        {
            PDCRT_DEFINE_RAICES(3);

            __m128i punto = pdcrt_xmm_desde_obj(oyo.coro->punto_de_inicio);
            oyo.coro->estado = PDCRT_CORO_EJECUTANDOSE;
            oyo.coro->punto_de_continuacion = k;

            PDCRT_GUARDAR_RAIZ(0, oyo);
            PDCRT_GUARDAR_RAIZ(1, kval);
            PDCRT_GUARDAR_RAIZ_XMM(2, punto);
            pdcrt_marco *m = pdcrt_crear_marco(ctx, PDCRT_GC(), 1, 0, k, NULL);
            PDCRT_CARGAR_RAIZ(0, oyo);
            PDCRT_CARGAR_RAIZ(1, kval);
            PDCRT_CARGAR_RAIZ_XMM(2, punto);

            pdcrt_fijar_local(ctx, m, 0, oyo);

            PDCRT_REINICIAR_RAICES();
            PDCRT_GUARDAR_RAIZ(0, kval);
            PDCRT_GUARDAR_RAIZ_CABECERA(1, m);
            PDCRT_GUARDAR_RAIZ_XMM(2, punto);
            pdcrt_obj cls = pdcrt_crear_closure_obj_1(ctx, PDCRT_GC(), &pdcrt_corrutina_generar,
                pdcrt_xmm_desde_obj(oyo));
            PDCRT_CARGAR_RAIZ(0, kval);
            PDCRT_CARGAR_RAIZ_CABECERA(1, m);
            PDCRT_CARGAR_RAIZ_XMM(2, punto);

            return pdcrt_llamar2(ctx, m, &pdcrt_recv_corrutina_avanzar_k1,
                punto,
                pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
                pdcrt_xmm_desde_obj(cls), pdcrt_xmm_desde_obj(kval));
        }
        else if(oyo.coro->estado == PDCRT_CORO_SUSPENDIDA)
        {
            pdcrt_k sus = oyo.coro->punto_de_suspencion;
            oyo.coro->estado = PDCRT_CORO_EJECUTANDOSE;
            oyo.coro->punto_de_continuacion = k;
            PDCRT_SACAR_PRELUDIO();
            return pdcrt_continuar(ctx, sus, pdcrt_xmm_desde_obj(kval));
        }
        else
        {
            pdcrt_error(ctx, "No se puede avanzar una corrutina que se esta ejecutando o esta finalizada");
        }
    }

    PDCRT_ASSERT(0 && "sin implementar");
}

static pdcrt_tk pdcrt_recv_corrutina_avanzar_k1(pdcrt_ctx *ctx, pdcrt_marco *m, __m128i res)
{
    PDCRT_K(pdcrt_recv_corrutina_avanzar_k1);
    pdcrt_obj yo = pdcrt_obtener_local(ctx, m, 0);
    if(yo.coro->estado != PDCRT_CORO_EJECUTANDOSE)
        pdcrt_error(ctx, "No se puede devolver de una corrutina que no se esta ejecutando");
    yo.coro->estado = PDCRT_CORO_FINALIZADA;
    return pdcrt_devolver1(ctx, m, res);
}

static pdcrt_tk pdcrt_corrutina_generar(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    if(args != 1)
        pdcrt_error(ctx, "Corrutina: generador debe llamarse con un argumento");
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
#if !NDEBUG
    pdcrt_debe_tener_tipo(ctx, oyo, PDCRT_TOBJ_CLOSURE);
#endif
    PDCRT_ASSERT(oyo.closure->num_capturas == 1);
    pdcrt_obj obj_coro = oyo.closure->capturas[0];
    pdcrt_corrutina *coro = obj_coro.coro;
    if(coro->estado != PDCRT_CORO_EJECUTANDOSE)
        pdcrt_error(ctx, "Corrutina: no se puede generar un valor para una corrutina que no se esta ejecutando");
    pdcrt_k coro_k = coro->punto_de_continuacion;
    coro->estado = PDCRT_CORO_SUSPENDIDA;
    coro->punto_de_suspencion = k;
    return pdcrt_continuar(ctx, coro_k, a1);
}

pdcrt_tk pdcrt_recv_instancia(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo(ctx, omsj, PDCRT_TOBJ_TEXTO);

    PDCRT_PROBE0(recv_instancia);

    pdcrt_debe_tener_tipo(ctx, oyo.inst->metodos, PDCRT_TOBJ_TABLA);
    pdcrt_obj metodo_de_instancia = pdcrt_objeto_nulo();
    bool contiene = pdcrt_tabla_en(ctx, oyo.inst->metodos.tabla, omsj, &metodo_de_instancia);
    if(contiene)
    {
        if(args >= 6)
        {
            pdcrt_extender_pila(ctx, 1);
            pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a6));
            pdcrt_insertar(ctx, argp);
        }
        return pdcrt_llamarnr(ctx, k.marco, k.kf, args + 1,
            pdcrt_xmm_desde_obj(metodo_de_instancia),
            pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
            yo, a1, a2, a3, a4, a5);
    }
    else
    {
        pdcrt_obj no_enc = oyo.inst->metodo_no_encontrado;
        if(pdcrt_tipo_de_obj(no_enc) == PDCRT_TOBJ_BOOLEANO)
        {
            if(no_enc.bval)
            {
                contiene = pdcrt_tabla_en(ctx,
                                          oyo.inst->metodos.tabla,
                                          pdcrt_objeto_texto(ctx->textos_globales.mensaje_no_encontrado),
                                          &no_enc);
                if(contiene)
                {
                    goto LLAMAR;
                }
                else
                {
                    pdcrt_inspeccionar_texto(omsj.texto);
                    pdcrt_error(ctx, u8"Método no encontrado");
                }
            }
            else
            {
                pdcrt_inspeccionar_texto(omsj.texto);
                pdcrt_error(ctx, u8"Método no encontrado");
            }
        }
        else
        {
        LLAMAR:
            pdcrt_extender_pila(ctx, 2);
            if(args >= 7)
            {
                pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a6));
                pdcrt_insertar(ctx, argp);
            }
            if(args >= 6)
            {
                pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a5));
                pdcrt_insertar(ctx, argp);
            }
            return pdcrt_llamarnr(ctx, k.marco, k.kf, args + 2,
                pdcrt_xmm_desde_obj(no_enc),
                pdcrt_xmm_desde_obj(pdcrt_objeto_texto(ctx->textos_globales.llamar)),
                yo, msj, a1, a2, a3, a4);
        }
    }
}

pdcrt_tk pdcrt_recv_reubicado(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    (void) ctx;
    (void) args;
    (void) k;
    PDCRT_ASSERT(0 && "sin implementar");
}
