//
// Created by alinarezrangel on 30/10/25.
//

#ifndef PDCRT_PDFFI_H
#define PDCRT_PDFFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#include "pdcrt-plataforma.h"

struct pdffi_ctx;
typedef struct pdffi_ctx pdffi_ctx;

typedef int (*pdffi_f)(pdffi_ctx *ctx);
typedef long pdffi_stp;

void pdffi_asegurar_pila(pdffi_ctx *ctx, size_t elementos);
void pdffi_extender_pila(pdffi_ctx *ctx, size_t elementos);

pdcrt_entero pdffi_obtener_entero(pdffi_ctx *ctx, pdffi_stp p, bool *ok);
pdcrt_float pdffi_obtener_float(pdffi_ctx *ctx, pdffi_stp p, bool *ok);
bool pdffi_obtener_boole(pdffi_ctx *ctx, pdffi_stp p, bool *ok);
bool pdffi_es_nulo(pdffi_ctx *ctx, pdffi_stp p);
void *pdffi_obtener_voidptr(pdffi_ctx *ctx, pdffi_stp p, bool *ok);
void *pdffi_obtener_valop(pdffi_ctx *ctx, pdffi_stp p, bool *ok, size_t *tam);

void pdffi_obtener_tam_texto(pdffi_ctx *ctx, pdffi_stp p, bool *ok, size_t *tam);
void pdffi_copiar_texto(pdffi_ctx *ctx, pdffi_stp p, bool *ok,
                        char *txt, size_t *tam);

typedef pdcrt_entero pdffi_texto_ref;
pdffi_texto_ref pdffi_obtener_texto(pdffi_ctx *ctx, pdffi_stp p, bool *ok,
                                    const char **salida, size_t *tam);
void pdffi_liberar_texto(pdffi_ctx *ctx, pdffi_texto_ref ref);

bool pdffi_es_arreglo(pdffi_ctx *ctx, pdffi_stp arr);
void pdffi_arreglo_en(pdffi_ctx *ctx, pdffi_stp arr, pdffi_stp i);
void pdffi_arreglo_en_i(pdffi_ctx *ctx, pdffi_stp arr, pdcrt_entero i);
void pdffi_arreglo_fijar_en(pdffi_ctx *ctx, pdffi_stp arr, pdffi_stp i, pdffi_stp v);
void pdffi_arreglo_fijar_en_i(pdffi_ctx *ctx, pdffi_stp arr, pdcrt_entero i, pdffi_stp v);
pdcrt_entero pdffi_arreglo_longitud(pdffi_ctx *ctx, pdffi_stp arr);
void pdffi_arreglo_agregar_al_final(pdffi_ctx *ctx, pdffi_stp arr, pdffi_stp v);

bool pdffi_es_diccionario(pdffi_ctx *ctx, pdffi_stp dic);
void pdffi_diccionario_en(pdffi_ctx *ctx, pdffi_stp dic, pdffi_stp llave);
bool pdffi_diccionario_intenta_en(pdffi_ctx *ctx, pdffi_stp dic, pdffi_stp llave);
void pdffi_diccionario_fijar_en(pdffi_ctx *ctx, pdffi_stp dic, pdffi_stp llave, pdffi_stp valor);
void pdffi_diccionario_eliminar(pdffi_ctx *ctx, pdffi_stp dic, pdffi_stp llave);
bool pdffi_diccionario_contiene(pdffi_ctx *ctx, pdffi_stp dic, pdffi_stp llave);
void pdffi_diccionario_vaciar(pdffi_ctx *ctx, pdffi_stp dic);
pdcrt_entero pdffi_diccionario_longitud(pdffi_ctx *ctx, pdffi_stp dic);
void pdffi_diccionario_para_cada_par(pdffi_ctx *ctx, pdffi_stp dic);
bool pdffi_diccionario_iter_avanzar(pdffi_ctx *ctx, pdffi_stp iter);

void pdffi_empujar_entero(pdffi_ctx *ctx, pdcrt_entero i);
void pdffi_empujar_float(pdffi_ctx *ctx, pdcrt_float f);
void pdffi_empujar_boole(pdffi_ctx *ctx, bool v);
void pdffi_empujar_nulo(pdffi_ctx *ctx);
void pdffi_empujar_voidptr(pdffi_ctx *ctx, void *ptr);
void *pdffi_empujar_valop(pdffi_ctx *ctx, size_t tam, size_t nvals);
void pdffi_empujar_texto(pdffi_ctx *ctx, const char *str, size_t tam_str);
void pdffi_empujar_texto_c(pdffi_ctx *ctx, const char *str);
void pdffi_empujar_arreglo_vacio(pdffi_ctx *ctx, size_t tam, size_t cap);
void pdffi_empujar_diccionario_vacio(pdffi_ctx *ctx, size_t cap);
void pdffi_empujar_closure(pdffi_ctx *ctx, pdffi_f f, size_t n_capturas);

void pdffi_llamar(pdffi_ctx *ctx, pdffi_stp func, int nargs, int nrets);
void pdffi_llamarv(pdffi_ctx *ctx, pdffi_stp func, int nargs, int *nrets);
typedef int (*pdffi_k)(pdffi_ctx *ctx, void *k_env);
int pdffi_llamark(pdffi_ctx *ctx, pdffi_stp func, int nargs, int nrets, void *k_env, pdffi_k k);
typedef int (*pdffi_vk)(pdffi_ctx *ctx, void *k_env, int nrets);
int pdffi_llamarvk(pdffi_ctx *ctx, pdffi_stp func, int nargs, void *k_env, pdffi_vk k);
int pdffi_llamart(pdffi_ctx *ctx, pdffi_stp func, int nargs);

bool pdffi_ejecutar_protegido(pdffi_ctx *ctx, pdffi_f f);
int pdffi_error(pdffi_ctx *ctx, const char *msj);
int pdffi_errorf(pdffi_ctx *ctx, const char *msj, ...);
int pdffi_verrorf(pdffi_ctx *ctx, const char *msj, va_list v);

bool pdffi_obtener_builtin(pdffi_ctx *ctx, const char *nombre, size_t tam_nombre);
bool pdffi_obtener_de_espacio_de_nombre(pdffi_ctx *ctx, pdffi_stp ns,
                                        const char *nombre, size_t tam_nombre);
void pdffi_utilizar(pdffi_ctx *ctx, const char *nombre, size_t tam_nombre);
int pdffi_utilizark(pdffi_ctx *ctx, const char *nombre, size_t tam_nombre, pdffi_k k);

typedef struct pdffi_exportacion
{
    const char *nombre;
    size_t tam_nombre;
    bool es_procedimiento;
} pdffi_exportacion;

int pdffi_exportar(pdffi_ctx *ctx, pdffi_exportacion *proto, size_t nproto);

void pdffi_enviar_mensaje(pdffi_ctx *ctx, pdffi_stp obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs, int nrets);
void pdffi_enviar_mensajev(pdffi_ctx *ctx, pdffi_stp obj,
                           const char *nombre, size_t tam_nombre,
                           int nargs, int *nrets);
int pdffi_enviar_mensajek(pdffi_ctx *ctx, pdffi_stp obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs, int nrets,
                          void *k_env, pdffi_k k);
int pdffi_enviar_mensajevk(pdffi_ctx *ctx, pdffi_stp obj,
                           const char *nombre, size_t tam_nombre,
                           int nargs,
                           void *k_env, pdffi_vk k);
int pdffi_enviar_mensajet(pdffi_ctx *ctx, pdffi_stp obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs);

#endif //PDCRT_PDFFI_H
