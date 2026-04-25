//
// Created by alinarezrangel on 24/4/26.
//

#ifndef PDCRT_DTRACE_H
#define PDCRT_DTRACE_H

#include "pdcrt_base.h"
#include "pdcrt-plataforma.h"

#ifdef PDCRT_DTRACE

#include "sys/sdt.h"

#define PDCRT_PROBE0(nombre) DTRACE_PROBE(pdcrt, nombre)
#define PDCRT_PROBE1(nombre, arg1) DTRACE_PROBE1(pdcrt, nombre, arg1)
#define PDCRT_PROBE2(nombre, arg1, arg2) DTRACE_PROBE2(pdcrt, nombre, arg1, arg2)
#define PDCRT_PROBE3(nombre, arg1, arg2, arg3) DTRACE_PROBE3(pdcrt, nombre, arg1, arg2, arg3)
#define PDCRT_PROBE4(nombre, arg1, arg2, arg3, arg4) DTRACE_PROBE4(pdcrt, nombre, arg1, arg2, arg3, arg4)
#define PDCRT_PROBE5(nombre, arg1, arg2, arg3, arg4, arg5) DTRACE_PROBE5(pdcrt, nombre, arg1, arg2, arg3, arg4, arg5)

#define PDCRT_DTRACE_COMPILADO 1

#else

#define PDCRT_PROBE0(nombre)
#define PDCRT_PROBE1(nombre, arg1)
#define PDCRT_PROBE2(nombre, arg1, arg2)
#define PDCRT_PROBE3(nombre, arg1, arg2, arg3)
#define PDCRT_PROBE4(nombre, arg1, arg2, arg3, arg4)
#define PDCRT_PROBE5(nombre, arg1, arg2, arg3, arg4, arg5)

#define PDCRT_DTRACE_COMPILADO 0

#endif

#endif //PDCRT_DTRACE_H
