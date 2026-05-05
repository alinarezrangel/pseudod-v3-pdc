//
// Created by alinarezrangel on 2/5/26.
//

#ifndef PDCRT_ARCHIVO_H
#define PDCRT_ARCHIVO_H

#define PDCRT_INTERNO
#include "pdcrt/pdcrt.h"
#include "pdcrt/pdcrt_vio.h"

typedef struct pdcrt_rsc_archivo
{
    pdcrt_archivo *archivo;
    bool eof;
} pdcrt_rsc_archivo;

#define PDCRT_RSC_ARCHIVO_EOF (-1)
#define PDCRT_RSC_ARCHIVO_NOBUF (-2)

pdcrt_tk pdcrt_recv_archivo(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM);
void pdcrt_liberar_rsc_archivo(pdcrt_ctx *ctx, void *datos, size_t ndatos);

#define pdcrt_objeto_archivo(v) ((pdcrt_obj) { .recv = &pdcrt_recv_archivo, .valop = (v) })

#endif //PDCRT_ARCHIVO_H
