//
// Created by alinarezrangel on 11/1/26.
//

#include "pdcrt_hash.h"


#if PDCRT_PTR_BITS == 64
#define P 2305843009213693951LLU
#else
#define P 2147483647LLU
#endif

union caster
{
    pdcrt_entero e;
    pdcrt_uentero u;
};

#define SCAST(ex) (((union caster) {.u = (ex)}).e)
#define UCAST(ex) (((union caster) {.e = (ex)}).u)

union fcaster
{
    pdcrt_float f;
    pdcrt_efloat u;
};

#define FCAST(ex) (((union fcaster) {.u = (ex)}).f)
#define ECAST(ex) (((union fcaster) {.f = (ex)}).u)


pdcrt_entero pdcrt_hash_entero(pdcrt_entero e)
{
    return SCAST(((uint64_t) e) * P);
}

pdcrt_entero pdcrt_hash_float(pdcrt_float f)
{
    return SCAST(ECAST(f) * P);
}

#define rotatel(ue) ((ue) << 1 | (ue) >> (sizeof(ue) * 8 - 1))

pdcrt_entero pdcrt_hash_bytes(const char *bytes, size_t nbytes)
{
    pdcrt_uentero hash = 5;
    for(size_t i = 0; i < nbytes; i++)
        hash = rotatel(hash) + bytes[i] * P;
    return SCAST(hash);
}
