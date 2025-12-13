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


#ifdef __has_attribute
#  if __has_attribute(always_inline)
#    define PDCRT_INLINE __attribute__((always_inline)) inline
#  else
#    define PDCRT_INLINE inline
#  endif
#  if __has_attribute(noinline)
#    define PDCRT_NOINLINE __attribute__((noinline))
#  else
#    define PDCRT_NOINLINE
#  endif
#else
#  define PDCRT_INLINE inline
#  define PDCRT_NOINLINE
#endif

struct pdcrt_ctx;
typedef struct pdcrt_ctx pdcrt_ctx;

typedef struct pdcrt_aloj_exts
{
    uint32_t version;
    uint32_t tam_bytes;
    char resto[];
} pdcrt_aloj_exts;


typedef struct pdcrt_aloj
{
    void *(*alojar)(void *yo, size_t bytes);
    void *(*realojar)(void *yo, void *ptr, size_t tam_actual, size_t tam_nuevo);
    void (*desalojar)(void *yo, void *ptr, size_t tam_actual);
    pdcrt_aloj_exts *(*obtener_extensiones)(void *yo);
} pdcrt_aloj;

void *pdcrt_alojar(pdcrt_aloj *aloj, size_t bytes);
void *pdcrt_realojar(pdcrt_aloj *aloj, void *ptr, size_t tam_actual, size_t tam_nuevo);
void pdcrt_desalojar(pdcrt_aloj *aloj, void *ptr, size_t tam_actual);

pdcrt_aloj* pdcrt_alojador_malloc(void);

typedef struct pdcrt_aloj_basico_cfg_v1
{
    size_t tam_pagina;
    size_t num_inicial_de_paginas;
} pdcrt_aloj_basico_cfg_v1;

pdcrt_aloj* pdcrt_alojador_basico(pdcrt_aloj* base, pdcrt_aloj_basico_cfg_v1* cfg, size_t tam_cfg);
void pdcrt_desalojar_alojador_basico(pdcrt_aloj* basico);

pdcrt_aloj* pdcrt_alojador_con_estadisticas(pdcrt_aloj* base);
size_t pdcrt_alojador_con_estadisticas_obtener_usado(pdcrt_aloj* yo);
void pdcrt_desalojar_alojador_con_estadisticas(pdcrt_aloj* yo);


#ifdef PDCRT_INTERNO

#include <stdalign.h>

void *pdcrt_alojar_ctx(pdcrt_ctx *ctx, size_t bytes);
void *pdcrt_realojar_ctx(pdcrt_ctx *ctx, void *ptr, size_t tam_actual, size_t tam_nuevo);
void pdcrt_desalojar_ctx(pdcrt_ctx *ctx, void *ptr, size_t tam_actual);


struct pdcrt_marco;
typedef struct pdcrt_marco pdcrt_marco;

struct pdcrt_k;
typedef struct pdcrt_k pdcrt_k;

typedef pdcrt_k (*pdcrt_f)(pdcrt_ctx *, int args, pdcrt_k);
typedef pdcrt_k (*pdcrt_kf)(pdcrt_ctx *, pdcrt_marco *);

struct pdcrt_k
{
    pdcrt_kf kf;
    pdcrt_marco *marco;
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
                   gris, negro, raices_viejas;

    size_t tam_heap;
    unsigned int num_recolecciones;
} pdcrt_gc;


typedef struct pdcrt_texto
{
    pdcrt_cabecera_gc gc;
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
    pdcrt_obj funcion_hash;
    pdcrt_obj funcion_igualdad;
    size_t num_buckets;
    size_t buckets_ocupados;
    size_t limite_de_ocupacion;
    pdcrt_bucket *buckets;
};

struct pdcrt_bucket
{
    bool activo;
    pdcrt_bucket *siguiente_colision;
    pdcrt_obj llave, valor;
};

struct pdcrt_valop
{
    pdcrt_cabecera_gc gc;
    // Todos los objetos del GC deben ser de al menos un puntero de "ancho" en
    // bytes.
    void *padding;
    char datos[];
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
    X(obtener_clase_objeto, u8"obtenerClaseObjeto")                     \
    X(crearCorrutina, "crearCorrutina")                                 \
    X(avanzar, "avanzar")                                               \
    X(finalizada, "finalizada")                                         \
    X(recolectar_basura, "recolectarBasura")

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
    pdcrt_k continuacion_actual;

    pdcrt_marco *primer_marco_activo;
    pdcrt_marco *ultimo_marco_activo;

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

    uintptr_t inicio_del_stack;
    size_t tam_stack;

    const char* mensaje_de_error;
    jmp_buf manejador_de_errores;
    bool hay_un_manejador_de_errores;
    jmp_buf salir_del_trampolin;
    bool hay_una_salida_del_trampolin;

    struct
    {
        bool time;
    } capacidades;

    struct
    {
        bool gc;
    } log;
};

pdcrt_ctx *pdcrt_crear_contexto(pdcrt_aloj *aloj);
void pdcrt_fijar_argv(pdcrt_ctx *ctx, int argc, char **argv);
void pdcrt_cerrar_contexto(pdcrt_ctx *ctx);

void pdcrt_extender_pila(pdcrt_ctx *ctx, pdcrt_marco *m, size_t num_elem);

typedef ssize_t pdcrt_stp;

void pdcrt_empujar_entero(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_entero i);
void pdcrt_empujar_booleano(pdcrt_ctx *ctx, pdcrt_marco *m, bool v);
void pdcrt_empujar_float(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_float f);
void pdcrt_empujar_espacio_de_nombres(pdcrt_ctx *ctx, pdcrt_marco **m);
void pdcrt_empujar_texto(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str, size_t len);
void pdcrt_empujar_texto_cstr(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str);
void pdcrt_empujar_nulo(pdcrt_ctx *ctx, pdcrt_marco *m);
void pdcrt_empujar_arreglo_vacio(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad);
void pdcrt_empujar_caja_vacia(pdcrt_ctx *ctx, pdcrt_marco **m);
void pdcrt_empujar_tabla_vacia(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad);
pdcrt_obj pdcrt_obtener_objeto_runtime(pdcrt_ctx *ctx, pdcrt_marco *m);
void* pdcrt_empujar_valop(pdcrt_ctx *ctx, pdcrt_marco **m, size_t num_bytes);
void pdcrt_empujar_voidptr(pdcrt_ctx *ctx, pdcrt_marco *m, void* ptr);
void pdcrt_empujar_corrutina(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp f);
void pdcrt_empujar_instancia(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp metodos, pdcrt_stp metodo_no_encontrado, size_t num_atrs);

pdcrt_obj pdcrt_crear_espacio_de_nombres_cons(pdcrt_ctx *ctx, pdcrt_marco **m);
pdcrt_texto* pdcrt_crear_texto(pdcrt_ctx *ctx, pdcrt_marco **m, const char *str, size_t len);

pdcrt_entero pdcrt_obtener_entero(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok);
pdcrt_float pdcrt_obtener_float(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok);
bool pdcrt_obtener_booleano(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok);
size_t pdcrt_obtener_tam_texto(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok);
bool pdcrt_obtener_texto(pdcrt_ctx *ctx, pdcrt_stp i, char *buffer, size_t tam_buffer);
void* pdcrt_obtener_valop(pdcrt_ctx *ctx, pdcrt_stp i, bool *ok);

void pdcrt_envolver_en_caja(pdcrt_ctx *ctx, pdcrt_marco **m);

void pdcrt_negar(pdcrt_ctx *ctx);

void pdcrt_eliminar_elemento(pdcrt_ctx *ctx, pdcrt_stp pos);
void pdcrt_eliminar_elementos(pdcrt_ctx *ctx, pdcrt_stp inic, size_t cnt);

void pdcrt_duplicar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_stp i);
void pdcrt_extraer(pdcrt_ctx *ctx, pdcrt_stp i);
void pdcrt_insertar(pdcrt_ctx *ctx, pdcrt_stp pos);
void pdcrt_mover_a_cima(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_stp pos, size_t num_els);

void pdcrt_ejecutar(pdcrt_ctx *ctx, int args, pdcrt_f f);
bool pdcrt_ejecutar_protegido(pdcrt_ctx *ctx, int args, pdcrt_f f);


pdcrt_cabecera_gc *pdcrt_gc_cabecera_de(pdcrt_obj o);

void pdcrt_recolectar_basura_por_pila(pdcrt_ctx *ctx, pdcrt_marco **m);

void pdcrt_gc_mover_a_grupo(pdcrt_gc_grupo *desde, pdcrt_gc_grupo *hacia, pdcrt_cabecera_gc *h);

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

pdcrt_marco* pdcrt_crear_marco(pdcrt_ctx *ctx, size_t registros, size_t capturas, int args, pdcrt_k k);
void pdcrt_inicializar_marco(pdcrt_ctx *ctx,
                             pdcrt_marco *m,
                             size_t sz,
                             size_t registros,
                             size_t capturas,
                             int args,
                             pdcrt_k k);
pdcrt_arreglo* pdcrt_crear_arreglo_vacio(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad);
pdcrt_closure* pdcrt_crear_closure(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_f f, size_t capturas);
pdcrt_caja* pdcrt_crear_caja(pdcrt_ctx *ctx, pdcrt_marco **m);
pdcrt_tabla* pdcrt_crear_tabla(pdcrt_ctx *ctx, pdcrt_marco **m, size_t capacidad);
pdcrt_valop* pdcrt_crear_valop(pdcrt_ctx *ctx, pdcrt_marco **m, size_t num_bytes);
pdcrt_corrutina* pdcrt_crear_corrutina(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_stp f_idx);
pdcrt_instancia* pdcrt_crear_instancia(pdcrt_ctx *ctx, pdcrt_marco **m,
                                       pdcrt_stp metodos, pdcrt_stp metodo_no_encontrado, size_t num_atrs);


void pdcrt_empujar_interceptar(pdcrt_ctx *ctx, pdcrt_obj o);
void pdcrt_fijar_pila_interceptar(pdcrt_ctx *ctx, size_t i, pdcrt_obj v);

#define pdcrt_empujar_ll(ctx, v) (ctx)->pila[(ctx)->tam_pila++] = (v)
#define pdcrt_fijar_pila_ll(ctx, i, v) (ctx)->pila[(i)] = (v)

#ifdef PDCRT_EMP_INTR
#define pdcrt_empujar(ctx, v) pdcrt_empujar_interceptar(ctx, v)
#define pdcrt_fijar_pila(ctx, i, v) pdcrt_fijar_pila_interceptar(ctx, i, v)
#else
#define pdcrt_empujar(ctx, v) pdcrt_empujar_ll(ctx, v)
#define pdcrt_fijar_pila(ctx, i, v) pdcrt_fijar_pila_ll(ctx, i, v)
#endif
#define pdcrt_sacar(ctx) (ctx)->pila[--(ctx)->tam_pila]
#define pdcrt_cima(ctx) (ctx)->pila[(ctx)->tam_pila - 1]

#define pdcrt_obtener_local(ctx, m, idx) ((m)->registros[(idx)])
#define pdcrt_fijar_local(ctx, m, idx, v)                               \
    do                                                                  \
    {                                                                   \
        pdcrt_obj local = (v);                                          \
        pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(local);            \
        if(vh)                                                          \
            pdcrt_barrera_de_escritura_cabecera((ctx), PDCRT_CABECERA_GC(m), vh); \
        (m)->registros[(idx)] = local;                                  \
    }                                                                   \
    while(0)

pdcrt_obj pdcrt_caja_vacia(pdcrt_ctx *ctx, pdcrt_marco *m);

void pdcrt_arreglo_en(pdcrt_ctx *ctx, pdcrt_stp arr, pdcrt_entero i);
void pdcrt_arreglo_empujar_al_final(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_stp arr);

void pdcrt_caja_fijar(pdcrt_ctx *ctx, pdcrt_stp caja);
void pdcrt_caja_obtener(pdcrt_ctx *ctx, pdcrt_stp caja);

#define pdcrt_fijar_caja(ctx, o, v)                     \
    do                                                  \
    {                                                   \
        pdcrt_obj valor = (v);                          \
        pdcrt_barrera_de_escritura((ctx), (o), valor);  \
        (o).caja->valor = valor;                        \
    }                                                   \
    while(0)
#define pdcrt_obtener_caja(ctx, o) (o).caja->valor

typedef struct pdcrt_params_base
{
    size_t num_params;
    bool tiene_variadic : 1;
    size_t idc_variadic : sizeof(size_t) - 1;
} pdcrt_params_base;

typedef struct pdcrt_param_data
{
    size_t reg;
} pdcrt_param_data;

typedef struct pdcrt_params_data
{
    pdcrt_params_base base;
    pdcrt_param_data params[];
} pdcrt_params_data;

void pdcrt_params(pdcrt_ctx *ctx,
                  pdcrt_marco *m,
                  pdcrt_params_data *restrict p);

pdcrt_k pdcrt_saltar(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_kf kf);
pdcrt_k pdcrt_enviar_mensaje(pdcrt_ctx *ctx, pdcrt_marco *m,
                             const char* msj, size_t tam_msj,
                             const int* proto, size_t nproto,
                             pdcrt_kf kf);
pdcrt_k pdcrt_enviar_mensaje_obj(pdcrt_ctx *ctx, pdcrt_marco *m,
                                 const int* proto, size_t nproto,
                                 pdcrt_kf kf);
void pdcrt_prn(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj v);
void pdcrt_prnl(pdcrt_ctx *ctx, pdcrt_marco *m);
pdcrt_k pdcrt_devolver(pdcrt_ctx *ctx, pdcrt_marco *m, int rets);

pdcrt_k pdcrt_importar(pdcrt_ctx *ctx,
                       pdcrt_marco *m,
                       const char *nombre,
                       size_t tam_nombre,
                       pdcrt_kf kf);
pdcrt_k pdcrt_extraerv(pdcrt_ctx *ctx,
                       pdcrt_marco *m,
                       pdcrt_obj mod,
                       const char *nombre,
                       size_t tam_nombre,
                       pdcrt_kf kf);
pdcrt_k pdcrt_agregar_nombre(pdcrt_ctx *ctx,
                             pdcrt_marco *m,
                             pdcrt_obj mod,
                             pdcrt_obj valor,
                             const char *nombre,
                             size_t tam_nombre,
                             bool autoejec,
                             pdcrt_kf kf);
pdcrt_k pdcrt_exportar(pdcrt_ctx *ctx,
                       pdcrt_marco *m,
                       pdcrt_obj mod,
                       const char *modulo,
                       size_t tam_modulo);

pdcrt_obj pdcrt_obtener_clase_objeto(pdcrt_ctx *ctx, pdcrt_marco *m);

typedef struct pdcrt_captura
{
    size_t registro;
} pdcrt_captura;
void pdcrt_empujar_closure(pdcrt_ctx *ctx, pdcrt_marco **m, pdcrt_f f, size_t num_caps);
pdcrt_obj pdcrt_mk_closure(pdcrt_ctx *ctx,
                           pdcrt_marco **m,
                           pdcrt_f f,
                           const pdcrt_captura *caps,
                           size_t ncaps);

void pdcrt_assert(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj v);

_Noreturn void pdcrt_error(pdcrt_ctx *ctx, const char* msj);

bool pdcrt_son_identicos(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj x, pdcrt_obj y);

pdcrt_obj pdcrt_obtener_espacio_de_nombres_del_runtime(pdcrt_ctx *ctx, pdcrt_marco *m);

void pdcrt_arreglo_abrir_espacio(pdcrt_ctx *ctx,
                                 pdcrt_marco *m,
                                 pdcrt_arreglo *arr,
                                 size_t espacio);

bool pdcrt_stack_lleno(pdcrt_ctx *ctx);

void pdcrt_inspeccionar_pila(pdcrt_ctx *ctx);
void pdcrt_inspeccionar_texto(pdcrt_texto *txt);

pdcrt_obj pdcrt_convertir_a_espacio_de_nombres(pdcrt_ctx *ctx, pdcrt_marco *m, pdcrt_obj mod);

void pdcrt_preparar_registro_de_modulos(pdcrt_ctx *ctx, size_t num_mods);
void pdcrt_cargar_dependencia(pdcrt_ctx *ctx, pdcrt_f fmod, const char *nombre, size_t tam_nombre);

int pdcrt_main(int argc, char **argv, void (*cargar_deps)(pdcrt_ctx *ctx), pdcrt_f f);

pdcrt_k pdc_instalar_pdcrt_N95_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k);

pdcrt_tipo pdcrt_tipo_de_obj(pdcrt_obj o);

pdcrt_k pdcrt_recv_entero(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_float(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_booleano(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_marco(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_texto(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_nulo(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_arreglo(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_closure(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_caja(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_tabla(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_runtime(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_voidptr(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_valop(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_espacio_de_nombres(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_corrutina(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_instancia(pdcrt_ctx *ctx, int args, pdcrt_k k);
pdcrt_k pdcrt_recv_reubicado(pdcrt_ctx *ctx, int args, pdcrt_k k);

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

#define PDCRT_ALOJAR_MARCO(ctx, num_regs, num_capturas, args, k)     \
    alignas(alignof(pdcrt_cabecera_gc))                                 \
        char marco_en_pila[                                             \
            sizeof(pdcrt_marco) + sizeof(pdcrt_obj) * (num_regs)]; \
    pdcrt_marco *m = (pdcrt_marco *) marco_en_pila;                     \
    pdcrt_inicializar_marco(ctx, m, sizeof(marco_en_pila), num_regs, num_capturas, args, k)

// Esta macro debería aceptar cuanta pila necesitamos, en bytes. En su forma actual, aún podríamos causar
// stack-overflows.
#define PDCRT_VERIFICA_PILA(ctx, m, nombre)                \
    do                                                     \
    {                                                      \
        if(pdcrt_stack_lleno(ctx))                         \
        {                                                  \
            pdcrt_recolectar_basura_por_pila(ctx, &m);     \
            return (pdcrt_k) { .kf = nombre, .marco = m }; \
        }                                                  \
    }                                                      \
    while(0)

#define PDCRT_DECLARAR_ENTRYPOINT(mod, fmain)                       \
    pdcrt_k pdc_instalar_##mod(pdcrt_ctx *ctx, int args, pdcrt_k k) \
    {                                                               \
        return fmain(ctx, args, k);                                 \
    }

#define PDCRT_PRECARGAR(f, n, l) num_mods += 1;
#define PDCRT_CARGAR(f, n, l) pdcrt_cargar_dependencia(ctx, &pdc_instalar_##f, n, l);
#define PDCRT_PREDECLARAR(f, n, l) pdcrt_k pdc_instalar_##f(pdcrt_ctx *ctx, int args, pdcrt_k k);
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
