//
// Created by alinarezrangel on 11/1/26.
//

#ifndef PDCRT_PDCRT_HASH_H
#define PDCRT_PDCRT_HASH_H

#include <stddef.h>

#include "pdcrt-plataforma.h"

pdcrt_entero pdcrt_hash_entero(pdcrt_entero e);
pdcrt_entero pdcrt_hash_float(pdcrt_float f);
pdcrt_entero pdcrt_hash_bytes(const char *bytes, size_t nbytes);

#endif //PDCRT_PDCRT_HASH_H
