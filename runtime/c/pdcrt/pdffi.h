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

typedef int (*pdffi_f)(pdffi_ctx *ctx, int nargs);
typedef int (*pdffi_k)(pdffi_ctx *ctx, void *k_env);
typedef int (*pdffi_vk)(pdffi_ctx *ctx, void *k_env, int nrets);

typedef pdcrt_entero pdffi_reg;

void pdffi_extender_pila(pdffi_ctx *ctx, size_t elementos);
size_t pdffi_tam_pila(pdffi_ctx *ctx);

pdcrt_entero pdffi_obtener_entero(pdffi_ctx *ctx, pdffi_reg p, bool *ok);
pdcrt_float pdffi_obtener_float(pdffi_ctx *ctx, pdffi_reg p, bool *ok);
bool pdffi_obtener_boole(pdffi_ctx *ctx, pdffi_reg p, bool *ok);
bool pdffi_es_nulo(pdffi_ctx *ctx, pdffi_reg p);
void *pdffi_obtener_voidptr(pdffi_ctx *ctx, pdffi_reg p, bool *ok);

size_t pdffi_obtener_tam_texto(pdffi_ctx *ctx, pdffi_reg p, bool *ok);
void pdffi_copiar_texto(pdffi_ctx *ctx, pdffi_reg p, bool *ok,
                        char *txt, size_t tam);

bool pdffi_es_arreglo(pdffi_ctx *ctx, pdffi_reg arr);
void pdffi_arreglo_en(pdffi_ctx *ctx, pdffi_reg arr, pdffi_reg i);
void pdffi_arreglo_en_i(pdffi_ctx *ctx, pdffi_reg arr, pdcrt_entero i);
void pdffi_arreglo_fijar_en(pdffi_ctx *ctx, pdffi_reg arr, pdffi_reg i, pdffi_reg v);
void pdffi_arreglo_fijar_en_i(pdffi_ctx *ctx, pdffi_reg arr, pdcrt_entero i, pdffi_reg v);
pdcrt_entero pdffi_arreglo_longitud(pdffi_ctx *ctx, pdffi_reg arr);
void pdffi_arreglo_agregar_al_final(pdffi_ctx *ctx, pdffi_reg arr, pdffi_reg v);

bool pdffi_es_diccionario(pdffi_ctx *ctx, pdffi_reg dic);
void pdffi_diccionario_en(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave);
bool pdffi_diccionario_intenta_en(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave);
void pdffi_diccionario_fijar_en(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave, pdffi_reg valor);
void pdffi_diccionario_eliminar(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave);
bool pdffi_diccionario_contiene(pdffi_ctx *ctx, pdffi_reg dic, pdffi_reg llave);
void pdffi_diccionario_vaciar(pdffi_ctx *ctx, pdffi_reg dic);
pdcrt_entero pdffi_diccionario_longitud(pdffi_ctx *ctx, pdffi_reg dic);

void pdffi_empujar_entero(pdffi_ctx *ctx, pdcrt_entero i);
void pdffi_empujar_float(pdffi_ctx *ctx, pdcrt_float f);
void pdffi_empujar_booleano(pdffi_ctx *ctx, bool v);
void pdffi_empujar_nulo(pdffi_ctx *ctx);
void pdffi_empujar_voidptr(pdffi_ctx *ctx, void *ptr);
void pdffi_empujar_texto(pdffi_ctx *ctx, const char *str, size_t tam_str);
void pdffi_empujar_texto_c(pdffi_ctx *ctx, const char *str);
void pdffi_empujar_arreglo_vacio(pdffi_ctx *ctx, size_t cap);
void pdffi_empujar_diccionario_vacio(pdffi_ctx *ctx, size_t cap);
void pdffi_empujar_closure(pdffi_ctx *ctx, pdffi_f f, size_t n_capturas);

void pdffi_llamar(pdffi_ctx *ctx, pdffi_reg func, int nargs, int nrets);
void pdffi_llamarv(pdffi_ctx *ctx, pdffi_reg func, int nargs, int *nrets);

int pdffi_llamark(pdffi_ctx *ctx, pdffi_reg func, int nargs, int nrets, void *k_env, pdffi_k k);
int pdffi_llamarvk(pdffi_ctx *ctx, pdffi_reg func, int nargs, void *k_env, pdffi_vk k);

int pdffi_llamart(pdffi_ctx *ctx, pdffi_reg func, int nargs);

_Noreturn int pdffi_error(pdffi_ctx *ctx, const char *msj);

void pdffi_enviar_mensaje(pdffi_ctx *ctx, pdffi_reg obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs, int nrets);
void pdffi_enviar_mensajev(pdffi_ctx *ctx, pdffi_reg obj,
                           const char *nombre, size_t tam_nombre,
                           int nargs, int *nrets);

int pdffi_enviar_mensajek(pdffi_ctx *ctx, pdffi_reg obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs, int nrets,
                          void *k_env, pdffi_k k);
int pdffi_enviar_mensajevk(pdffi_ctx *ctx, pdffi_reg obj,
                           const char *nombre, size_t tam_nombre,
                           int nargs,
                           void *k_env, pdffi_vk k);

int pdffi_enviar_mensajet(pdffi_ctx *ctx, pdffi_reg obj,
                          const char *nombre, size_t tam_nombre,
                          int nargs);

#endif //PDCRT_PDFFI_H
