#ifndef PDCRT_H
#define PDCRT_H

#include <setjmp.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "pdcrt-plataforma.h"

#include "pdcrt_base.h"
#include "pdcrt_dtrace.h"
#include "pdcrt_vio.h"

#ifdef PDCRT_INTERNO

#include <stdalign.h>

#include <xmmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>

#define PDCRT_FORMATEAR_BYTES_TAM_BUFFER 80LU
void pdcrt_formatear_bytes(char *buffer, size_t bytes);

void *pdcrt_alojar_ctx(pdcrt_ctx *ctx, size_t bytes);
void *pdcrt_realojar_ctx(pdcrt_ctx *ctx, void *ptr, size_t tam_actual, size_t tam_nuevo);
void pdcrt_desalojar_ctx(pdcrt_ctx *ctx, void *ptr, size_t tam_actual);


struct pdcrt_marco;
typedef struct pdcrt_marco pdcrt_marco;

struct pdcrt_k;
typedef struct pdcrt_k pdcrt_k;

struct pdcrt_tk;
typedef struct pdcrt_tk pdcrt_tk;

#define PDCRT_F_IMM __m128i yo, __m128i msj, __m128i a1, __m128i a2, __m128i a3, __m128i a4, __m128i a5, __m128i a6
#define PDCRT_A_IMM yo, msj, a1, a2, a3, a4, a5, a6
#define PDCRT_AA_IMM a1, a2, a3, a4, a5, a6
#define PDCRT_N_IMM 8
#define PDCRT_NN_IMM 6

#ifdef PDCRT_DBG_NO_K
#define PDCRT_FUNC_DECL
#define PDCRT_K_DECL
#else
#define PDCRT_FUNC_DECL _Noreturn
#define PDCRT_K_DECL _Noreturn
#endif

typedef pdcrt_tk (*pdcrt_f)(pdcrt_ctx *, int args, pdcrt_k k, PDCRT_F_IMM);
typedef pdcrt_tk (*pdcrt_kf)(pdcrt_ctx *, pdcrt_marco *, __m128i res);

struct pdcrt_k
{
    pdcrt_kf kf;
    pdcrt_marco *marco;
};

struct pdcrt_tk
{
    pdcrt_k k;
    __m128i res;
};


typedef enum pdcrt_tipo_obj_gc
{
    PDCRT_TGC_MARCO,
    PDCRT_TGC_TEXTO,
    PDCRT_TGC_ARREGLO,
    PDCRT_TGC_CLOSURE,
    PDCRT_TGC_CAJA,
    PDCRT_TGC_TABLA,
    PDCRT_TGC_VALOP,
    PDCRT_TGC_CORO,
    PDCRT_TGC_INSTANCIA,
    PDCRT_TGC_REUBICADO,
} pdcrt_tipo_obj_gc;

typedef enum pdcrt_gc_tipo_grupo
{
    PDCRT_TGRP_BLANCO_JOVEN,
    PDCRT_TGRP_BLANCO_VIEJO,
    PDCRT_TGRP_BLANCO_EN_LA_PILA,
    PDCRT_TGRP_GRIS,
    PDCRT_TGRP_NEGRO,
    PDCRT_TGRP_RAICES_VIEJAS,
    PDCRT_TGRP_RECURSOS,
    PDCRT_TGRP_NINGUNO,
} pdcrt_gc_tipo_grupo;

typedef struct pdcrt_cabecera_gc
{
    struct pdcrt_cabecera_gc *siguiente, *anterior; // 8 + 8
    // {
    pdcrt_tipo_obj_gc tipo : 4;
    pdcrt_gc_tipo_grupo grupo : 4;
    bool en_la_pila : 1;
    // } = 1+1/8 -> 2 -> 4
    uint32_t num_bytes; // 4
    // total: 24
} pdcrt_cabecera_gc;

_Static_assert(sizeof(void*) == 8 ? sizeof(pdcrt_cabecera_gc) == 24 : sizeof(pdcrt_cabecera_gc) == 16,
               "sizeof(pdcrt_cabecera_gc) == 24 o 16");

#define PDCRT_CABECERA_GC(v) ((pdcrt_cabecera_gc *) (v))
#define PDCRT_CABECERA_GC_PTR(v) ((pdcrt_cabecera_gc **) (v))

typedef struct pdcrt_gc_grupo
{
    pdcrt_gc_tipo_grupo grupo;
    pdcrt_cabecera_gc *primero, *ultimo;
} pdcrt_gc_grupo;

typedef struct pdcrt_gc
{
    pdcrt_gc_grupo blanco_joven, blanco_viejo, blanco_en_la_pila,
                   gris, negro, raices_viejas, recursos;

    size_t tam_heap;
    unsigned int num_recolecciones;
} pdcrt_gc;


typedef struct pdcrt_texto
{
    pdcrt_cabecera_gc gc;
    pdcrt_entero hash;
    size_t longitud;
    char contenido[];
} pdcrt_texto;

struct pdcrt_arreglo;
typedef struct pdcrt_arreglo pdcrt_arreglo;

struct pdcrt_closure;
typedef struct pdcrt_closure pdcrt_closure;

struct pdcrt_caja;
typedef struct pdcrt_caja pdcrt_caja;

struct pdcrt_tabla;
typedef struct pdcrt_tabla pdcrt_tabla;

struct pdcrt_valop;
typedef struct pdcrt_valop pdcrt_valop;

struct pdcrt_corrutina;
typedef struct pdcrt_corrutina pdcrt_corrutina;

struct pdcrt_instancia;
typedef struct pdcrt_instancia pdcrt_instancia;

struct pdcrt_reubicado;
typedef struct pdcrt_reubicado pdcrt_reubicado;

typedef struct pdcrt_obj
{
    pdcrt_f recv;
    union
    {
        pdcrt_entero ival;
        pdcrt_float fval;
        bool bval;
        pdcrt_marco *marco;
        pdcrt_texto *texto;
        pdcrt_arreglo *arreglo;
        pdcrt_closure *closure;
        pdcrt_caja *caja;
        pdcrt_tabla *tabla;
        void *pval;
        pdcrt_valop *valop;
        pdcrt_corrutina *coro;
        pdcrt_instancia *inst;
        pdcrt_reubicado *reubicado;
        pdcrt_cabecera_gc *gc;
    };
} pdcrt_obj;

PDCRT_INLINE pdcrt_obj pdcrt_obj_desde_xmm(__m128i r)
{
    return (pdcrt_obj){ .recv = (void*) _mm_extract_epi64(r, 1), .pval = (void*) _mm_cvtsi128_si64(r) };
}

PDCRT_INLINE __m128i pdcrt_xmm_desde_obj(pdcrt_obj o)
{
    return _mm_set_epi64x((int64_t) (uint64_t) o.recv, (int64_t) (uint64_t) o.pval);
}

#define PDCRT_XMM_NULO() pdcrt_xmm_desde_obj(pdcrt_objeto_nulo())
#define PDCRT_XMM_TEXTO(texto) pdcrt_xmm_desde_obj(pdcrt_objeto_texto(texto))

struct pdcrt_arreglo
{
    pdcrt_cabecera_gc gc;
    size_t longitud;
    size_t capacidad;
    pdcrt_obj *valores;
};

struct pdcrt_closure
{
    pdcrt_cabecera_gc gc;
    pdcrt_f f;
    size_t num_capturas;
    pdcrt_arreglo *constantes_del_modulo;
    pdcrt_obj capturas[];
};

struct pdcrt_caja
{
    pdcrt_cabecera_gc gc;
    pdcrt_obj valor;
};

struct pdcrt_bucket;
typedef struct pdcrt_bucket pdcrt_bucket;

struct pdcrt_tabla
{
    pdcrt_cabecera_gc gc;

    bool _bit : 1;
    size_t mascara : sizeof(size_t) * 8 - 1;
    size_t buckets_ocupados;
    pdcrt_bucket *buckets, *colisiones;
    size_t num_colisiones, cap_colisiones;
};

struct pdcrt_bucket
{
    pdcrt_obj llave, valor;
    bool activo : 1;
    bool tiene_colision : 1;
    size_t idc_colision : sizeof(void*) * 8 - 2;
};

typedef void (*pdcrt_valop_liberar)(pdcrt_ctx *ctx, void *datos, size_t ndatos);

struct pdcrt_valop
{
    pdcrt_cabecera_gc gc;
    pdcrt_valop_liberar liberar;
    _Alignas(_Alignof(void*)) char datos[];
};

typedef enum pdcrt_corrutina_estado
{
    PDCRT_CORO_INICIAL,
    PDCRT_CORO_SUSPENDIDA,
    PDCRT_CORO_EJECUTANDOSE,
    PDCRT_CORO_FINALIZADA,
} pdcrt_corrutina_estado;

struct pdcrt_corrutina
{
    pdcrt_cabecera_gc gc;
    pdcrt_corrutina_estado estado;
    union
    {
        pdcrt_obj punto_de_inicio;
        pdcrt_k punto_de_suspencion;
        pdcrt_k punto_de_continuacion;
    };
};

struct pdcrt_instancia
{
    pdcrt_cabecera_gc gc;
    pdcrt_obj metodos;
    pdcrt_obj metodo_no_encontrado;
    size_t num_atributos;
    pdcrt_obj atributos[];
};

struct pdcrt_marco
{
    pdcrt_cabecera_gc gc;
    int args;
    pdcrt_k k;
    size_t num_registros;
    const char *debug_srcloc;
    pdcrt_arreglo *constantes_del_modulo;
    pdcrt_obj registros[];
};

struct pdcrt_reubicado
{
    pdcrt_cabecera_gc gc;
    pdcrt_cabecera_gc *nueva_direccion;
    char bytes_viejos[];
};

// Asegúrate de que pdcrt_reubicado es menor que todos los demás objetos del
// GC:
#define PDCRT_VERIFICA_TAM_REUBICADO(T)                             \
    _Static_assert(sizeof(T) >= sizeof(pdcrt_reubicado),            \
                   #T " debe ser más grande que pdcrt_reubicado")

PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_texto);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_arreglo);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_closure);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_caja);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_tabla);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_valop);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_corrutina);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_instancia);
PDCRT_VERIFICA_TAM_REUBICADO(pdcrt_marco);


typedef enum pdcrt_tipo
{
    PDCRT_TOBJ_ENTERO,
    PDCRT_TOBJ_FLOAT,
    PDCRT_TOBJ_BOOLEANO,
    PDCRT_TOBJ_MARCO,
    PDCRT_TOBJ_TEXTO,
    PDCRT_TOBJ_NULO,
    PDCRT_TOBJ_ARREGLO,
    PDCRT_TOBJ_CLOSURE,
    PDCRT_TOBJ_CAJA,
    PDCRT_TOBJ_TABLA,
    PDCRT_TOBJ_RUNTIME,
    PDCRT_TOBJ_VOIDPTR,
    PDCRT_TOBJ_VALOP,
    PDCRT_TOBJ_ESPACIO_DE_NOMBRES,
    PDCRT_TOBJ_CORRUTINA,
    PDCRT_TOBJ_INSTANCIA,
    PDCRT_TOBJ_REUBICADO,
} pdcrt_tipo;

#define PDCRT_TABLA_TEXTOS(X)                                           \
    X(operador_mas, "operador_+")                                       \
    X(operador_menos, "operador_-")                                     \
    X(operador_por, "operador_*")                                       \
    X(operador_entre, "operador_/")                                     \
    X(operador_igual, "operador_=")                                     \
    X(operador_distinto, "operador_no=")                                \
    X(sumar, "sumar")                                                   \
    X(restar, "restar")                                                 \
    X(multiplicar, "multiplicar")                                       \
    X(dividir, "dividir")                                               \
    X(igual, "igualA")                                                  \
    X(distinto, "distíntoDe")                                           \
    X(como_texto, "comoTexto")                                          \
    X(concatenar, "concatenar")                                         \
    X(operador_menor_que, "operador_<")                                 \
    X(menor_que, "menorQue")                                            \
    X(operador_mayor_que, "operador_>")                                 \
    X(mayor_que, "mayorQue")                                            \
    X(operador_menor_o_igual_a, "operador_=<")                          \
    X(menor_o_igual_a, "menorOIgualA")                                  \
    X(operador_mayor_o_igual_a, "operador_>=")                          \
    X(mayor_o_igual_a, "mayorOIgualA")                                  \
    X(operador_bitand, "operador_<*>")                                  \
    X(operador_bitor, "operador_<+>")                                   \
    X(operador_bitxor, "operador_<^>")                                  \
    X(operador_bitlshift, "operador_<<")                                \
    X(operador_bitrshift, "operador_>>")                                \
    X(invertir, "invertir")                                             \
    X(negar, "negar")                                                   \
    X(piso, "piso")                                                     \
    X(techo, "techo")                                                   \
    X(truncar, "truncar")                                               \
    X(como_numero_entero, "comoNumeroEntero")                           \
    X(como_numero_real, "comoNumeroReal")                               \
    X(byte_como_texto, "byteComoTexto")                                 \
    X(byte_en, "byteEn")                                                \
    X(longitud, "longitud")                                             \
    X(en, "en")                                                         \
    X(subtexto, "subTexto")                                             \
    X(parte_del_texto, "parteDelTexto")                                 \
    X(buscar, "buscar")                                                 \
    X(buscar_en_reversa, "buscarEnReversa")                             \
    X(fijarEn, "fijarEn")                                               \
    X(verdadero, "VERDADERO")                                           \
    X(falso, "FALSO")                                                   \
    X(unir, "unir")                                                     \
    X(llamar, "llamar")                                                 \
    X(formatear, "formatear")                                           \
    X(escoger, "escoger")                                               \
    X(llamarSegun, "llamarSegun")                                       \
    X(llamarSegun2, u8"llamarSegún")                                    \
    X(o, "o")                                                           \
    X(operador_o, "operador_||")                                        \
    X(y, "y")                                                           \
    X(operador_y, "operador_&&")                                        \
    X(crearTabla, "crearTabla")                                         \
    X(rehashear, "rehashear")                                           \
    X(capacidad, "capacidad")                                           \
    X(contiene, "contiene")                                             \
    X(eliminar, "eliminar")                                             \
    X(paraCadaPar, "paraCadaPar")                                       \
    X(agregar_al_final, "agregarAlFinal")                               \
    X(crear_instancia, "crearInstancia")                                \
    X(atributos_de_instancia, "atributosDeInstancia")                   \
    X(obtener_metodos, u8"obtenerMétodos")                              \
    X(obtener_atributo, "obtenerAtributo")                              \
    X(fijar_atributo, "fijarAtributo")                                  \
    X(es_instancia, "esInstancia")                                      \
    X(metodo_no_encontrado, "metodoNoEncontrado")                       \
    X(mensaje_no_encontrado, "mensajeNoEncontrado")                     \
    X(enviar_mensaje, "enviarMensaje")                                  \
    X(fallar_con_mensaje, "fallarConMensaje")                           \
    X(leer_caracter, u8"leerCarácter")                                  \
    X(obtener_argv, u8"obtenerArgv")                                    \
    X(obtener_programa, u8"obtenerPrograma")                            \
    X(fijar_clase_objeto, u8"fijarClaseObjeto")                         \
    X(fijar_clase_arreglo, u8"fijarClaseArreglo")                       \
    X(fijar_clase_boole, u8"fijarClaseBoole")                           \
    X(fijar_clase_numero, u8"fijarClaseNumero")                         \
    X(fijar_clase_procedimiento, u8"fijarClaseProcedimiento")           \
    X(fijar_clase_tipo_nulo, u8"fijarClaseTipoNulo")                    \
    X(fijar_clase_texto, u8"fijarClaseTexto")                           \
    X(fijar_clase_tabla, u8"fijarClaseTabla")                           \
    X(obtener_clase_objeto, u8"obtenerClaseObjeto")                     \
    X(obtener_metodo_de_instancia, u8"_obtenerMétodoDeInstancia")       \
    X(crearCorrutina, "crearCorrutina")                                 \
    X(avanzar, "avanzar")                                               \
    X(finalizada, "finalizada")                                         \
    X(recolectar_basura, "recolectarBasura")                            \
    X(vaciar, "vaciar")                                                 \
    X(texto_vacio, "")                                                  \
    X(abrir_archivo, "abrirArchivo")                                    \
    X(cerrar, "cerrar")                                                 \
    X(esta_abierto, "estaAbierto")                                      \
    X(esta_abierto2, u8"estáAbierto")                                   \
    X(leer_byte, u8"leerByte")                                          \
    X(obtener_siguiente_byte, u8"obtenerSiguienteByte")                 \
    X(escribir_byte, "escribirByte")                                    \
    X(escribir_texto, "escribirTexto")                                  \
    X(posicion_actual, "posicionActual")                                \
    X(posicion_actual2, u8"posiciónActual")                             \
    X(cambiar_posicion, "cambiarPosicion")                              \
    X(cambiar_posicion2, u8"cambiarPosición")                           \
    X(fin_del_archivo, "finDelArchivo")                                 \
    X(error, "error")                                                   \
    X(leer_todo, "__leerTodo")                                          \
    X(crear_directorio, "crearDirectorio")                              \
    X(borrar_directorio, "borrarDirectorio")                            \
    X(borrar_archivo, "borrarArchivo")                                  \
    X(obtener_pid, "obtenerPid")                                        \
    X(ejecutar, "ejecutar")                                             \
    X(ok, "ok")                                                         \
    X(otro, "otro")                                                     \
    X(obtener_variable_de_entorno, "obtenerVariableDeEntorno")          \
    X(redimensionar, "redimensionar")                                   \
    X(crear_arreglo_vacio, u8"crearArregloVacío")                       \
    X(tabla, u8"Tabla")                                                 \
    X(nulo_como_texto, "NULO")

typedef struct pdcrt_textos
{
#define PDCRT_X(nombre, _texto) pdcrt_texto *nombre;
    PDCRT_TABLA_TEXTOS(PDCRT_X)
#undef PDCRT_X
} pdcrt_textos;

struct pdcrt_ctx
{
    bool recolector_de_basura_activo;

    pdcrt_aloj *alojador;

    pdcrt_obj* pila;
    size_t tam_pila;
    size_t cap_pila;

    pdcrt_gc gc;
    pdcrt_tk continuacion_actual;

    unsigned int cnt;

    pdcrt_texto **textos;
    size_t tam_textos;
    size_t cap_textos;
    pdcrt_textos textos_globales;

    pdcrt_obj funcion_igualdad;
    pdcrt_obj funcion_hash;

    pdcrt_obj registro_de_espacios_de_nombres;
    pdcrt_obj registro_de_modulos;
    pdcrt_obj espacio_de_nombres_runtime;

    pdcrt_obj argv;
    pdcrt_obj nombre_del_programa;
    pdcrt_obj clase_objeto;
    pdcrt_obj clase_arreglo;
    pdcrt_obj clase_boole;
    pdcrt_obj clase_numero;
    pdcrt_obj clase_procedimiento;
    pdcrt_obj clase_tipo_nulo;
    pdcrt_obj clase_texto;
    pdcrt_obj clase_tabla;

    uintptr_t inicio_del_stack;
    size_t tam_stack;

    const char* mensaje_de_error;
    jmp_buf manejador_de_errores;
    bool hay_un_manejador_de_errores;
    jmp_buf salir_del_trampolin;
    bool hay_una_salida_del_trampolin;
    jmp_buf continuar;
    bool hay_un_continuar;

    pdcrt_vio vio;
};

pdcrt_ctx *pdcrt_crear_contexto(pdcrt_aloj *aloj, pdcrt_vio vio);
void pdcrt_fijar_argv(pdcrt_ctx *ctx, int argc, char **argv);
void pdcrt_cerrar_contexto(pdcrt_ctx *ctx);

void pdcrt_ejecutar(pdcrt_ctx *ctx, int args, pdcrt_f f);
bool pdcrt_ejecutar_protegido(pdcrt_ctx *ctx, int args, pdcrt_f f);


pdcrt_cabecera_gc *pdcrt_gc_cabecera_de(pdcrt_obj o);

typedef struct pdcrt_gc_raices
{
    struct pdcrt_gc_raices *superior;
    size_t num_raices;
    pdcrt_obj raices_locales[];
} pdcrt_gc_raices;

#define PDCRT_DEFINE_RAICES(n) \
    _Alignas(pdcrt_gc_raices) char raices_locales_buffer[sizeof(pdcrt_gc_raices) + sizeof(pdcrt_obj) * (n)] = {0}; \
    pdcrt_gc_raices *raices_locales = (pdcrt_gc_raices *) raices_locales_buffer; \
    raices_locales->superior = NULL; \
    raices_locales->num_raices = (n);
#define PDCRT_RAICES_SUPERIORES(sup) raices_locales->superior = (sup)
#define PDCRT_REINICIAR_RAICES() memset(raices_locales->raices_locales, 0, sizeof(pdcrt_obj) * raices_locales->num_raices)

#define PDCRT_GUARDAR_RAIZ(n, o) raices_locales->raices_locales[n] = (o)
#define PDCRT_CARGAR_RAIZ(n, o) (o) = raices_locales->raices_locales[n]
#define PDCRT_GUARDAR_RAIZ_CABECERA(n, c) raices_locales->raices_locales[n] = (pdcrt_obj) { .recv = NULL, .gc = PDCRT_CABECERA_GC(c) };
#define PDCRT_CARGAR_RAIZ_CABECERA(n, c) (c) = (void *) raices_locales->raices_locales[n].gc
#define PDCRT_GUARDAR_RAIZ_XMM(n, xmm) raices_locales->raices_locales[n] = pdcrt_obj_desde_xmm(xmm)
#define PDCRT_CARGAR_RAIZ_XMM(n, xmm) (xmm) = pdcrt_xmm_desde_obj(raices_locales->raices_locales[n])
#define PDCRT_GUARDAR_RAIZ_K(n, k) PDCRT_GUARDAR_RAIZ_CABECERA(n, (k).marco)
#define PDCRT_CARGAR_RAIZ_K(n, k) PDCRT_CARGAR_RAIZ_CABECERA(n, (k).marco)

// Marcador de pdcrt_gc_raices. Debe usarse en todos los puntos en los
// que se realize una posible recolección de basura.
#define PDCRT_GC() raices_locales

void pdcrt_recolectar_basura_por_pila(pdcrt_ctx *ctx, pdcrt_gc_raices *m);

void pdcrt_gc_mover_a_grupo(pdcrt_gc_grupo *desde, pdcrt_gc_grupo *hacia, pdcrt_cabecera_gc *h);

void pdcrt_gc_eliminar_de_grupo(pdcrt_gc_grupo *grupo, pdcrt_cabecera_gc *h);
void pdcrt_gc_agregar_a_grupo(pdcrt_gc_grupo *grupo, pdcrt_cabecera_gc *h);

typedef enum pdcrt_tipo_recoleccion
{
    PDCRT_RECOLECCION_SIN_PILA,
    PDCRT_RECOLECCION_SIN_MEMORIA,
} pdcrt_tipo_recoleccion;

typedef struct pdcrt_recoleccion
{
    pdcrt_tipo_recoleccion tipo;
    bool mayor;
    union
    {
        struct
        {
            size_t memoria_requerida;
        } sin_memoria;
    };
} pdcrt_recoleccion;

pdcrt_recoleccion pdcrt_gc_recoleccion_por_pila(pdcrt_ctx *ctx, bool mayor);
pdcrt_recoleccion pdcrt_gc_recoleccion_por_memoria(pdcrt_ctx *ctx, size_t memoria_requerida, bool mayor);

PDCRT_INLINE bool pdcrt_gc_recoleccion_debe_mover_pila(pdcrt_recoleccion r)
{
    return r.tipo == PDCRT_RECOLECCION_SIN_PILA;
}

PDCRT_INLINE bool pdcrt_gc_recoleccion_es_mayor(pdcrt_recoleccion r)
{
    return r.mayor;
}

PDCRT_INLINE bool pdcrt_gc_recoleccion_es_menor(pdcrt_recoleccion r)
{
    return !r.mayor;
}

void pdcrt_recolectar_basura_simple(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_recoleccion params);
void pdcrt_intenta_invocar_al_recolector(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_recoleccion params);
void pdcrt_activar_recolector_de_basura(pdcrt_ctx *ctx);
void pdcrt_desactivar_recolector_de_basura(pdcrt_ctx *ctx);
void pdcrt_inicializar_obj(pdcrt_ctx *ctx,
                           pdcrt_cabecera_gc *h,
                           pdcrt_tipo_obj_gc tipo,
                           size_t sz);
void pdcrt_inicializar_rsc(pdcrt_ctx *ctx,
                           pdcrt_cabecera_gc *h,
                           pdcrt_tipo_obj_gc tipo,
                           size_t sz);
void *pdcrt_alojar_obj(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_tipo_obj_gc tipo, size_t sz);
void *pdcrt_alojar_rsc(pdcrt_ctx *ctx, pdcrt_gc_raices *m, pdcrt_tipo_obj_gc tipo, size_t sz);

typedef struct pdcrt_objeto_generico_gc
{
    pdcrt_cabecera_gc gc;
    char bytes[];
} pdcrt_objeto_generico_gc;

size_t pdcrt_gc_liberar_objeto(pdcrt_ctx *ctx, pdcrt_cabecera_gc *h);

// NOTA: No necesitamos la barrera de escritura para ninguna "raiz" del GC
inline void pdcrt_barrera_de_escritura_cabecera(pdcrt_ctx *ctx, pdcrt_cabecera_gc *ch, pdcrt_cabecera_gc *vh)
{
    if(ch->grupo == PDCRT_TGRP_BLANCO_VIEJO && (vh->grupo == PDCRT_TGRP_BLANCO_JOVEN || vh->grupo == PDCRT_TGRP_BLANCO_EN_LA_PILA))
    {
        pdcrt_gc_mover_a_grupo(&ctx->gc.blanco_viejo, &ctx->gc.raices_viejas, ch);
    }
}

inline void pdcrt_barrera_de_escritura(pdcrt_ctx *ctx, pdcrt_obj contenedor, pdcrt_obj valor)
{
    pdcrt_cabecera_gc *ch = pdcrt_gc_cabecera_de(contenedor);
    pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(valor);
    if(!vh || !ch)
        return;
    pdcrt_barrera_de_escritura_cabecera(ctx, ch, vh);
}

size_t pdcrt_tabla_num_buckets_hasheables(size_t mascara);
void pdcrt_inicializar_buckets(pdcrt_bucket *arr, size_t len);
void pdcrt_tabla_inicializar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, size_t capacidad);
size_t pdcrt_tabla_desalojar(pdcrt_ctx *ctx, pdcrt_tabla *tbl);
void pdcrt_tabla_fijar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, pdcrt_obj valor, bool rehashear);
void pdcrt_tabla_rehashear(pdcrt_ctx *ctx, pdcrt_tabla *tbl, size_t nueva_cap);
void pdcrt_tabla_vaciar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, bool rehashear);
bool pdcrt_tabla_en(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, pdcrt_obj *valor);
void pdcrt_tabla_eliminar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, bool rehashear);

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

PDCRT_INLINE bool pdcrt_es_menor_que(enum pdcrt_comparacion op)
{
    return op & 1;
}

PDCRT_INLINE bool pdcrt_es_igual_a(enum pdcrt_comparacion op)
{
    return op & 2;
}

PDCRT_INLINE bool pdcrt_es_mayor_que(enum pdcrt_comparacion op)
{
    return op & 4;
}

#define PDCRT_K(func)                                   \
    if(pdcrt_stack_lleno(ctx))                          \
    {                                                   \
        PDCRT_DEFINE_RAICES(1);                         \
        PDCRT_GUARDAR_RAIZ_CABECERA(0, m);              \
        pdcrt_recolectar_basura_por_pila(ctx, PDCRT_GC()); \
        PDCRT_CARGAR_RAIZ_CABECERA(0, m);               \
        return pdcrt_trampolin(ctx, (pdcrt_tk) { .k = (pdcrt_k) { .kf = &func, .marco = m }, .res = res }); \
    }

bool pdcrt_comparar_entero_y_float(pdcrt_entero e, pdcrt_float f, enum pdcrt_comparacion op);
bool pdcrt_comparar_enteros(pdcrt_entero a, pdcrt_entero b, enum pdcrt_comparacion op);
bool pdcrt_comparar_floats(pdcrt_float a, pdcrt_float b, enum pdcrt_comparacion op);

PDCRT_INLINE bool pdcrt_comparar_textos(pdcrt_texto *a, pdcrt_texto *b)
{
    return a == b;
}

#define pdcrt_crear_texto_desde_cstr(ctx, m, cstr) \
    pdcrt_crear_texto(ctx, m, cstr, strlen(cstr))

void pdcrt_precalcular_hash(pdcrt_ctx *ctx, pdcrt_obj o);

void pdcrt_traceback(pdcrt_ctx *ctx, pdcrt_marco *m);
_Noreturn void pdcrt_error(pdcrt_ctx *ctx, const char* msj);
_Noreturn void pdcrt_enomem(pdcrt_ctx *ctx);
_Noreturn void pdcrt_errortb(pdcrt_ctx *ctx, pdcrt_marco *m, const char* msj);
void pdcrt_debe_tener_tipo_lento(pdcrt_ctx *ctx, pdcrt_obj obj, pdcrt_tipo t);
void pdcrt_debe_tener_tipo_rapido(pdcrt_ctx *ctx, pdcrt_obj obj, pdcrt_f t);

bool pdcrt_son_identicos(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj x, pdcrt_obj y);

pdcrt_obj pdcrt_obtener_espacio_de_nombres_del_runtime(pdcrt_ctx *ctx, pdcrt_marco *m);

void pdcrt_arreglo_abrir_espacio(pdcrt_ctx *ctx,
                                 pdcrt_marco *m,
                                 pdcrt_arreglo *arr,
                                 size_t espacio);

bool pdcrt_stack_lleno(pdcrt_ctx *ctx);
PDCRT_INLINE _Noreturn pdcrt_tk pdcrt_trampolin(pdcrt_ctx *ctx, pdcrt_tk tk)
{
    if(!ctx->hay_un_continuar)
        pdcrt_error(ctx, u8"No se inicializó el trampolín");
    PDCRT_PROBE0(trampolin_rapido);
    ctx->continuacion_actual = tk;
    longjmp(ctx->continuar, 1);
}

PDCRT_INLINE pdcrt_tk pdcrt_continuar(pdcrt_ctx *ctx, pdcrt_k k, __m128i res)
{
#ifdef PDCRT_DBG_NO_K
    // No usamos el trampolín dado que NO_K hace que la pila nunca esté muy llena
    return (pdcrt_tk) { .k = k, .res = res };
#else
    return k.kf(ctx, k.marco, res);
#endif
}

bool pdcrt_es_primitivo(pdcrt_ctx *ctx, pdcrt_obj o);
pdcrt_entero pdcrt_hash(pdcrt_ctx *ctx, pdcrt_obj o);
bool pdcrt_igualdad(pdcrt_ctx *ctx, pdcrt_obj a, pdcrt_obj b);
pdcrt_tk pdcrt_funcion_igualdad(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_funcion_hash(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);

void pdcrt_inspeccionar_pila(pdcrt_ctx *ctx);
void pdcrt_inspeccionar_texto(pdcrt_texto *txt);

pdcrt_obj pdcrt_convertir_a_espacio_de_nombres(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj mod);

void pdcrt_preparar_registro_de_modulos(pdcrt_ctx *ctx, size_t num_mods);
void pdcrt_cargar_dependencia(pdcrt_ctx *ctx, pdcrt_f fmod, const char *nombre, size_t tam_nombre);

int pdcrt_main(int argc, char **argv, void (*cargar_deps)(pdcrt_ctx *ctx), pdcrt_f f);

pdcrt_tk pdc_instalar_pdcrt_N95_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);

pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o);

pdcrt_tk pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_arreglo(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_closure(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_caja(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_tabla(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_voidptr(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_valop(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_espacio_de_nombres(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_corrutina(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_instancia(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
pdcrt_tk pdcrt_recv_reubicado(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);

#define pdcrt_objeto_entero(i) ((pdcrt_obj) { .recv = &pdcrt_recv_entero, .ival = (i) })
#define pdcrt_objeto_float(f) ((pdcrt_obj) { .recv = &pdcrt_recv_float, .fval = (f) })
#define pdcrt_objeto_booleano(b) ((pdcrt_obj) { .recv = &pdcrt_recv_booleano, .bval = (b) })
#define pdcrt_objeto_marco(m) ((pdcrt_obj) { .recv = &pdcrt_recv_marco, .marco = (m) })
#define pdcrt_objeto_texto(txt) ((pdcrt_obj) { .recv = &pdcrt_recv_texto, .texto = (txt) })
#define pdcrt_objeto_nulo() ((pdcrt_obj) { .recv = &pdcrt_recv_nulo })
#define pdcrt_objeto_arreglo(arr) ((pdcrt_obj) { .recv = &pdcrt_recv_arreglo, .arreglo = (arr) })
#define pdcrt_objeto_closure(cls) ((pdcrt_obj) { .recv = &pdcrt_recv_closure, .closure = (cls) })
#define pdcrt_objeto_caja(cj) ((pdcrt_obj) { .recv = &pdcrt_recv_caja, .caja = (cj) })
#define pdcrt_objeto_tabla(tbl) ((pdcrt_obj) { .recv = &pdcrt_recv_tabla, .tabla = (tbl) })
#define pdcrt_objeto_runtime() ((pdcrt_obj) { .recv = &pdcrt_recv_runtime })
#define pdcrt_objeto_voidptr(ptr) ((pdcrt_obj) { .recv = &pdcrt_recv_voidptr, .pval = (ptr) })
#define pdcrt_objeto_valop(vo) ((pdcrt_obj) { .recv = &pdcrt_recv_valop, .valop = (vo) })
#define pdcrt_objeto_espacio_de_nombres(tbl) ((pdcrt_obj) { .recv = &pdcrt_recv_espacio_de_nombres, .tabla = (tbl) })
#define pdcrt_objeto_corrutina(corrutina) ((pdcrt_obj) { .recv = &pdcrt_recv_corrutina, .coro = (corrutina) })
#define pdcrt_objeto_instancia(instancia) ((pdcrt_obj) { .recv = &pdcrt_recv_instancia, .inst = (instancia) })
#define pdcrt_objeto_reubicado(reub) ((pdcrt_obj) { .recv = &pdcrt_recv_reubicado, .reubicado = (reub) })


#define PDCRT_CALC_ARGS() (ctx->tam_pila - (args < PDCRT_NN_IMM ? 0 : args - PDCRT_NN_IMM));
#define PDCRT_SACAR_PRELUDIO() do { if(args >= PDCRT_NN_IMM) pdcrt_eliminar_elementos(ctx, argp, args - PDCRT_NN_IMM); } while(0)


#define PDCRT_EMPUJAR_IMM(ctx) \
    pdcrt_extender_pila(ctx, PDCRT_N_IMM + 1); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(yo)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(msj)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a1)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a2)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a3)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a4)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a5)); \
    pdcrt_empujar(ctx, pdcrt_obj_desde_xmm(a6)); \
    pdcrt_empujar(ctx, pdcrt_objeto_marco(k.marco));

#define PDCRT_SACAR_IMM(ctx) (ctx)->tam_pila -= PDCRT_N_IMM + 1;

#ifdef PDCRT_DBG_NO_K
#define PDCRT_ALOJAR_MARCO(ctx, num_regs, args, k, srcloc)     \
    pdcrt_debe_tener_tipo(ctx, pdcrt_obj_desde_xmm(yo), PDCRT_TOBJ_CLOSURE); \
    PDCRT_EMPUJAR_IMM(ctx) \
    pdcrt_marco *m = pdcrt_crear_marco(ctx, NULL, num_regs, args, k, pdcrt_obj_desde_xmm(yo).closure); \
    m->debug_srcloc = srcloc; \
    PDCRT_SACAR_IMM(ctx)
#else
#define PDCRT_ALOJAR_MARCO(ctx, num_regs, args, k, srcloc)     \
    pdcrt_debe_tener_tipo_rapido(ctx, pdcrt_obj_desde_xmm(yo), &pdcrt_recv_closure); \
    alignas(alignof(pdcrt_cabecera_gc))                                 \
        char marco_en_pila[                                             \
            sizeof(pdcrt_marco) + sizeof(pdcrt_obj) * (num_regs)]; \
    pdcrt_marco *m = (pdcrt_marco *) marco_en_pila;                     \
    pdcrt_inicializar_marco(ctx, m, sizeof(marco_en_pila), num_regs, args, k, pdcrt_obj_desde_xmm(yo).closure); \
    m->debug_srcloc = srcloc;
#endif

#define PDCRT_OBTENER_CONSTANTE(ctx, m, n) ((m)->constantes_del_modulo->valores[n])
#define PDCRT_FIJAR_CONSTANTES(ctx, m, arr) \
    do { \
    pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(m), PDCRT_CABECERA_GC(arr)); \
    (m)->constantes_del_modulo = (arr); \
    } while(0)

// Esta macro debería aceptar cuanta pila necesitamos, en bytes. En su forma actual, aún podríamos causar
// stack-overflows.
#define PDCRT_VERIFICA_PILA(ctx, m, resv, nombre)          \
    do                                                     \
    {                                                      \
        if(pdcrt_stack_lleno(ctx))                         \
        {                                                  \
            PDCRT_DEFINE_RAICES(2);                        \
            PDCRT_GUARDAR_RAIZ_CABECERA(0, m);             \
            PDCRT_GUARDAR_RAIZ_XMM(1, resv);               \
            pdcrt_recolectar_basura_por_pila(ctx, PDCRT_GC()); \
            PDCRT_CARGAR_RAIZ_CABECERA(0, m);              \
            PDCRT_CARGAR_RAIZ_XMM(1, resv);              \
            return pdcrt_trampolin(ctx, (pdcrt_tk) { .k = { .kf = nombre, .marco = m }, .res = resv }); \
        }                                                  \
    }                                                      \
    while(0)

#define PDCRT_DECLARAR_ENTRYPOINT(mod, fmain)                       \
    pdcrt_tk pdc_instalar_##mod(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM) \
    {                                                               \
        return fmain(ctx, args, k, PDCRT_A_IMM);                    \
    }

#define PDCRT_PRECARGAR(f, n, l) num_mods += 1;
#define PDCRT_CARGAR(f, n, l) pdcrt_cargar_dependencia(ctx, &pdc_instalar_##f, n, l);
#define PDCRT_PREDECLARAR(f, n, l) pdcrt_tk pdc_instalar_##f(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
#define PDCRT_DECLARAR_DEPS(X)                             \
    X(PDCRT_PREDECLARAR)                                   \
    void pdcrt_cargar_dependencias(pdcrt_ctx *ctx)         \
    {                                                      \
        size_t num_mods = 0;                               \
        X(PDCRT_PRECARGAR)                                 \
        pdcrt_preparar_registro_de_modulos(ctx, num_mods); \
        X(PDCRT_CARGAR)                                    \
    }

#define PDCRT_DECLARAR_MAIN(mod)                            \
    int main(int argc, char **argv)                         \
    {                                                       \
        return pdcrt_main(argc, argv, &pdcrt_cargar_dependencias, &pdc_instalar_##mod); \
    }

#endif  /* PDCRT_INTERNO */

#endif /* PDCRT_H */
