//
// Created by alinarezrangel on 15/3/26.
//

#define PDCRT_INTERNO
#include "pdcrt.h"
#include "pdcrt_base.h"
#include "pdcrt_ops.h"

/* Esta implementación de tablas hash está fuertemente inspirada por las tablas hash de Lua 5.4. */

pdcrt_tk pdcrt_funcion_igualdad(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    if(args != 2)
        pdcrt_error(ctx, u8"FunciónIgualdad: se esperaban 2 argumentos");

    pdcrt_obj a = pdcrt_obj_desde_xmm(a1);
    pdcrt_obj b = pdcrt_obj_desde_xmm(a2);

    if(pdcrt_es_primitivo(ctx, a) || pdcrt_es_primitivo(ctx, b))
    {
        bool ok = pdcrt_igualdad(ctx, a, b);
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(ok)));
    }
    else
    {
        return pdcrt_llamar1(ctx, k.marco, k.kf,
                             a1, PDCRT_XMM_TEXTO(ctx->textos_globales.igual), a2);
    }
}

pdcrt_tk pdcrt_funcion_hash(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    if(args != 1)
        pdcrt_error(ctx, u8"FunciónHash: se esperaba 1 argumento");
    pdcrt_entero hash = pdcrt_hash(ctx, pdcrt_obj_desde_xmm(a1));
    return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(hash)));
}


#define PDCRT_TABLA_TAM_MINIMO 8 // Debe ser una potencia de 2 mayor a 1

_Static_assert(PDCRT_TABLA_TAM_MINIMO > 1, "PDCRT_TABLA_TAM_MINIMO debe ser mayor a 1");

#define PDCRT_TABLA_NUM_MAX_BUCKETS (1LLU << ((sizeof(size_t) * 8LLU) - 1))

extern size_t pdcrt_tabla_num_buckets_hasheables(size_t mascara);

void pdcrt_inicializar_buckets(pdcrt_bucket *arr, size_t len)
{
    for(size_t i = 0; i < len; i++)
    {
        arr[i] = (pdcrt_bucket) {
            .activo = false,
            .off_colision = 0,
            .llave = pdcrt_objeto_nulo(),
            .valor = pdcrt_objeto_nulo(),
        };
    }
}

void pdcrt_tabla_inicializar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, size_t capacidad)
{
    if(capacidad > PDCRT_TABLA_NUM_MAX_BUCKETS)
        capacidad = PDCRT_TABLA_NUM_MAX_BUCKETS;
    tbl->mascara = pdcrt_redondear_a_p2(capacidad) - 1;
    if(tbl->mascara == 0 || capacidad < PDCRT_TABLA_TAM_MINIMO)
        tbl->mascara = PDCRT_TABLA_TAM_MINIMO - 1;

    tbl->buckets = pdcrt_alojar_ctx(ctx, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * sizeof(pdcrt_bucket));
    if(!tbl->buckets)
        pdcrt_enomem(ctx);
    pdcrt_inicializar_buckets(tbl->buckets, pdcrt_tabla_num_buckets_hasheables(tbl->mascara));

    tbl->num_colisiones = 0;
    tbl->buckets_ocupados = 0;
}

size_t pdcrt_tabla_desalojar(pdcrt_ctx *ctx, pdcrt_tabla *tbl)
{
    size_t total = pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * sizeof(pdcrt_bucket);
    pdcrt_desalojar_ctx(ctx, tbl->buckets, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * sizeof(pdcrt_bucket));
    return total;
}

void pdcrt_tabla_rehashear(pdcrt_ctx *ctx, pdcrt_tabla *tbl, size_t nueva_cap)
{
    if(nueva_cap <= tbl->buckets_ocupados)
        // No redimensiones, resultaría en peor rendimiento
        return;

    PDCRT_PROBE1(tabla_rehashear_entry, nueva_cap);

    if(nueva_cap > PDCRT_TABLA_NUM_MAX_BUCKETS)
        nueva_cap = PDCRT_TABLA_NUM_MAX_BUCKETS;
    nueva_cap = pdcrt_redondear_a_p2(nueva_cap);
    if(nueva_cap < PDCRT_TABLA_TAM_MINIMO)
        nueva_cap = PDCRT_TABLA_TAM_MINIMO;

    size_t buckets_ocupados = tbl->buckets_ocupados;

    pdcrt_bucket *nuevos_buckets = pdcrt_alojar_ctx(ctx, pdcrt_tabla_num_buckets_hasheables(nueva_cap) * sizeof(pdcrt_bucket));
    if(!nuevos_buckets)
        pdcrt_enomem(ctx);
    pdcrt_inicializar_buckets(nuevos_buckets, nueva_cap);

    size_t hasheables_viejos = pdcrt_tabla_num_buckets_hasheables(tbl->mascara);
    pdcrt_bucket *viejos_buckets = tbl->buckets;
    tbl->buckets = nuevos_buckets;
    tbl->mascara = nueva_cap - 1;
    tbl->buckets_ocupados = 0;
    tbl->num_colisiones = 0;

    for(size_t i = 0; i < hasheables_viejos; i++)
    {
        pdcrt_bucket *b = &viejos_buckets[i];
        if(b->activo)
            pdcrt_tabla_fijar(ctx, tbl, b->llave, b->valor, false);
    }

    pdcrt_desalojar_ctx(ctx, viejos_buckets, hasheables_viejos * sizeof(pdcrt_bucket));

    PDCRT_PROBE0(tabla_rehashear_exit);

    PDCRT_ASSERT(tbl->buckets_ocupados == buckets_ocupados);
}

static pdcrt_bucket *pdcrt_tabla_reclamar_bucket(pdcrt_tabla *tbl)
{
    for(size_t i = 0; i < pdcrt_tabla_num_buckets_hasheables(tbl->mascara); i++)
    {
        if(!tbl->buckets[i].activo)
            return &tbl->buckets[i];
    }
    return NULL;
}

void pdcrt_tabla_fijar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, pdcrt_obj valor, bool rehashear)
{
    PDCRT_PROBE0(tabla_fijar);

    bool rehasheado = false;
reintentar:;
    pdcrt_entero hash = pdcrt_hash(ctx, llave);
    pdcrt_bucket *b1 = &tbl->buckets[hash & tbl->mascara];
    if(b1->activo)
    {
        for(pdcrt_bucket *at = b1;; at += at->off_colision)
        {
            if(pdcrt_igualdad(ctx, llave, at->llave))
            {
                at->valor = valor;
                pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(valor);
                if(vh)
                    pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), vh);
                return;
            }

            if(!at->off_colision)
                break;
        }

        // Colisión
        pdcrt_bucket *b2 = &tbl->buckets[pdcrt_hash(ctx, b1->llave) & tbl->mascara];
        pdcrt_bucket *col = pdcrt_tabla_reclamar_bucket(tbl);
        if(!col)
        {
            PDCRT_ASSERT(!rehasheado);
            pdcrt_tabla_rehashear(ctx, tbl, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * 2);
            rehasheado = true;
            goto reintentar;
        }

        tbl->buckets_ocupados += 1;
        tbl->num_colisiones += 1;

        if(b1 == b2)
        {
            // Agrega llave/valor como colisión de b1
            col->activo = true;
            col->llave = llave;
            col->valor = valor;
            col->off_colision = b1->off_colision ? (b1 + b1->off_colision) - col : 0;
            b1->off_colision = col - b1;
        }
        else
        {
            // Reubica b1, inserta llave/valor en *b1
            col->llave = b1->llave;
            col->valor = b1->valor;
            col->activo = true;
            col->off_colision = b1->off_colision ? (b1 + b1->off_colision) - col : 0;
            pdcrt_bucket *at;
            for(at = b2;
                at + at->off_colision != b1;
                at += at->off_colision)
            {}
            at->off_colision = col - at;

            b1->llave = llave;
            b1->valor = valor;
            b1->off_colision = 0;
        }
    }
    else
    {
        tbl->buckets_ocupados += 1;
        b1->activo = true;
        b1->llave = llave;
        b1->valor = valor;
        b1->off_colision = 0;
    }

    pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(llave);
    if(vh)
        pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), vh);
    vh = pdcrt_gc_cabecera_de(valor);
    if(vh)
        pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), vh);
}

bool pdcrt_tabla_en(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, pdcrt_obj *valor)
{
    if(!pdcrt_es_primitivo(ctx, llave))
        return false;

    PDCRT_PROBE0(tabla_en);

    pdcrt_entero hash = pdcrt_hash(ctx, llave);
    pdcrt_bucket *b1 = &tbl->buckets[hash & tbl->mascara];
    if(b1->activo)
    {
        if(pdcrt_igualdad(ctx, llave, b1->llave))
        {
            *valor = b1->valor;
            return true;
        }

        pdcrt_bucket *at = b1;
        while(true)
        {
            if(pdcrt_igualdad(ctx, llave, at->llave))
            {
                *valor = at->valor;
                return true;
            }

            if(!at->off_colision)
                break;

            at += at->off_colision;
        }
    }
    return false;
}

void pdcrt_tabla_eliminar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, bool rehashear)
{
    if(!pdcrt_es_primitivo(ctx, llave))
        return;

    PDCRT_PROBE0(tabla_eliminar);

    pdcrt_entero hash = pdcrt_hash(ctx, llave);
    pdcrt_bucket *b1 = &tbl->buckets[hash & tbl->mascara];
    if(b1->activo)
    {
        for(pdcrt_bucket *at = b1, *prev = NULL; at != prev; at += at->off_colision, prev = at)
        {
            if(pdcrt_igualdad(ctx, llave, at->llave))
            {
                tbl->buckets_ocupados -= 1;

                if(!prev)
                {
                    if(!at->off_colision)
                    {
                        at->activo = false;
                        at->off_colision = 0;
                    }
                    else
                    {
                        pdcrt_bucket *ultimo, *penultimo;
                        for(ultimo = at, penultimo = NULL;
                            ultimo->off_colision;
                            penultimo = ultimo, ultimo += ultimo->off_colision)
                        {}
                        if(penultimo)
                            penultimo->off_colision = 0;
                        ultimo->activo = false;
                        ultimo->off_colision = 0;
                        at->llave = ultimo->llave;
                        at->valor = ultimo->valor;

                        tbl->num_colisiones -= 1;
                    }
                }
                else
                {
                    prev->off_colision = at->off_colision ? prev - (at + at->off_colision) : 0;
                    at->activo = false;
                    at->off_colision = 0;
                    tbl->num_colisiones -= 1;
                }
            }
        }
    }
}

void pdcrt_tabla_vaciar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, bool rehashear)
{
    (void) rehashear;
    PDCRT_PROBE0(tabla_vaciar);

    for(size_t i = 0; i < pdcrt_tabla_num_buckets_hasheables(tbl->mascara); i++)
    {
        tbl->buckets[i].activo = false;
        tbl->buckets[i].off_colision = 0;
    }
    tbl->num_colisiones = 0;
    tbl->buckets_ocupados = 0;
}
