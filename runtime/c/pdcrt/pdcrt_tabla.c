//
// Created by alinarezrangel on 15/3/26.
//

#define PDCRT_INTERNO
#include "pdcrt.h"
#include "pdcrt_base.h"
#include "pdcrt_ops.h"

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
#define PDCRT_TABLA_OCUPACION_MAXIMA 3 // Máximo 300% de ocupación
#define PDCRT_TABLA_OCUPACION_MINIMA 4 // Mínimo 1/4 de ocupación

_Static_assert(PDCRT_TABLA_TAM_MINIMO > 1, "PDCRT_TABLA_TAM_MINIMO debe ser mayor a 1");
_Static_assert(PDCRT_TABLA_OCUPACION_MAXIMA >= 1, "PDCRT_TABLA_OCUPACION_MAXIMA debe ser mayor a 100%");
_Static_assert(PDCRT_TABLA_OCUPACION_MINIMA > 1, "PDCRT_TABLA_OCUPACION_MINIMA debe ser mayor a 1");

#define PDCRT_TABLA_NUM_MAX_BUCKETS (1LLU << ((sizeof(size_t) * 8LLU) - 1))

size_t pdcrt_tabla_num_buckets_hasheables(size_t mascara)
{
    // Aunque parezca un error, la mascara solo tiene 63/31 bits por lo que es seguro.
    return mascara + 1;
}

void pdcrt_inicializar_buckets(pdcrt_bucket *arr, size_t len)
{
    for(size_t i = 0; i < len; i++)
    {
        arr[i] = (pdcrt_bucket) {
            .activo = false,
            .idc_colision = 0,
            .tiene_colision = false,
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
    tbl->cap_colisiones = pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * (PDCRT_TABLA_OCUPACION_MAXIMA - 1) + 1;
    tbl->colisiones = pdcrt_alojar_ctx(ctx, tbl->cap_colisiones * sizeof(pdcrt_bucket));
    if(!tbl->colisiones)
        pdcrt_enomem(ctx);

    tbl->buckets_ocupados = 0;
}

size_t pdcrt_tabla_desalojar(pdcrt_ctx *ctx, pdcrt_tabla *tbl)
{
    size_t total = (pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * sizeof(pdcrt_bucket)) + (tbl->cap_colisiones * sizeof(pdcrt_bucket));
    pdcrt_desalojar_ctx(ctx, tbl->buckets, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) * sizeof(pdcrt_bucket));
    pdcrt_desalojar_ctx(ctx, tbl->colisiones, tbl->cap_colisiones * sizeof(pdcrt_bucket));
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

    for(size_t i = 0; i < hasheables_viejos; i++)
    {
        pdcrt_bucket *b = &viejos_buckets[i];
        if(b->activo)
            pdcrt_tabla_fijar(ctx, tbl, b->llave, b->valor, false);
    }

    pdcrt_desalojar_ctx(ctx, viejos_buckets, hasheables_viejos * sizeof(pdcrt_bucket));

    tbl->buckets_ocupados = 0;
    for(size_t i = 0; i < pdcrt_tabla_num_buckets_hasheables(tbl->mascara); i++)
    {
        if(tbl->buckets[i].activo)
        {
            tbl->buckets_ocupados += 1;
            tbl->buckets[i].tiene_colision = false;
        }
    }

    size_t num_cols = tbl->num_colisiones;
    tbl->num_colisiones = 0;
    for(size_t i = 0; i < num_cols; i++)
    {
        pdcrt_bucket b = tbl->colisiones[i];
        if(b.activo)
            pdcrt_tabla_fijar(ctx, tbl, b.llave, b.valor, false);
    }

    PDCRT_ASSERT(tbl->num_colisiones <= (PDCRT_TABLA_OCUPACION_MAXIMA - 1) * nueva_cap);

    if(tbl->cap_colisiones > (PDCRT_TABLA_OCUPACION_MAXIMA - 1) * nueva_cap + 1)
    {
        pdcrt_bucket *nuevas_cols = pdcrt_realojar_ctx(ctx,
                                                       tbl->colisiones,
                                                       tbl->cap_colisiones * sizeof(pdcrt_bucket),
                                                       (2 * nueva_cap + 1) * sizeof(pdcrt_bucket));
        if(!nuevas_cols)
            return;
        tbl->colisiones = nuevas_cols;
        tbl->cap_colisiones = (PDCRT_TABLA_OCUPACION_MAXIMA - 1) * nueva_cap + 1;
    }

    PDCRT_PROBE0(tabla_rehashear_exit);

    PDCRT_ASSERT(tbl->buckets_ocupados == buckets_ocupados);
}

void pdcrt_tabla_fijar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, pdcrt_obj valor, bool rehashear)
{
    if(!pdcrt_es_primitivo(ctx, llave))
        pdcrt_error(ctx, u8"Se trató de agregar un valor no primitivo a una tabla primitiva");

    pdcrt_cabecera_gc *lh = pdcrt_gc_cabecera_de(llave);
    pdcrt_cabecera_gc *vh = pdcrt_gc_cabecera_de(valor);
    pdcrt_entero hash = pdcrt_hash(ctx, llave);
    pdcrt_bucket *b = &tbl->buckets[hash & tbl->mascara];
    if(b->activo)
    {
        PDCRT_PROBE0(tabla_fijar_colision);
        while(true)
        {
            PDCRT_ASSERT(b->activo);

            if(pdcrt_igualdad(ctx, b->llave, llave))
            {
                if(vh)
                    pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), vh);
                b->valor = valor;
                break;
            }

            if(!b->tiene_colision)
            {
                b->tiene_colision = true;
                b->idc_colision = tbl->num_colisiones;

                if(tbl->num_colisiones >= tbl->cap_colisiones)
                {
                    size_t nueva_cap = tbl->cap_colisiones * 2;
                    tbl->colisiones = pdcrt_realojar_ctx(ctx,
                                                         tbl->colisiones,
                                                         tbl->cap_colisiones * sizeof(pdcrt_bucket),
                                                         nueva_cap * sizeof(pdcrt_bucket));
                    if(!tbl->colisiones)
                        pdcrt_enomem(ctx);
                    tbl->cap_colisiones = nueva_cap;
                    PDCRT_PROBE1(tabla_mas_colisiones, nueva_cap);
                    PDCRT_ASSERT(tbl->num_colisiones < tbl->cap_colisiones);
                }

                if(lh)
                    pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), lh);
                if(vh)
                    pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), vh);
                tbl->colisiones[tbl->num_colisiones] = (pdcrt_bucket) {
                    .activo = true,
                    .idc_colision = 0,
                    .tiene_colision = false,
                    .llave = llave,
                    .valor = valor,
                };

                tbl->num_colisiones += 1;
                tbl->buckets_ocupados += 1;
                PDCRT_ASSERT(tbl->buckets_ocupados <= pdcrt_tabla_num_buckets_hasheables(tbl->mascara) + tbl->num_colisiones);
                break;
            }
            else
            {
                b = &tbl->colisiones[b->idc_colision];
            }
        }
    }
    else
    {
        if(lh)
            pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), lh);
        if(vh)
            pdcrt_barrera_de_escritura_cabecera(ctx, PDCRT_CABECERA_GC(tbl), vh);
        *b = (pdcrt_bucket) {
            .activo = true,
            .idc_colision = 0,
            .tiene_colision = false,
            .llave = llave,
            .valor = valor,
        };

        tbl->buckets_ocupados += 1;
        PDCRT_ASSERT(tbl->buckets_ocupados <= pdcrt_tabla_num_buckets_hasheables(tbl->mascara) + tbl->num_colisiones);
    }

    if(rehashear && tbl->buckets_ocupados / pdcrt_tabla_num_buckets_hasheables(tbl->mascara) >= PDCRT_TABLA_OCUPACION_MAXIMA)
    {
        pdcrt_tabla_rehashear(ctx, tbl, tbl->buckets_ocupados);
    }
    else if(rehashear && tbl->buckets_ocupados <= pdcrt_tabla_num_buckets_hasheables(tbl->mascara) / PDCRT_TABLA_OCUPACION_MINIMA)
    {
        pdcrt_tabla_rehashear(ctx, tbl, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) / PDCRT_TABLA_OCUPACION_MINIMA);
    }
}

bool pdcrt_tabla_en(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, pdcrt_obj *valor)
{
    if(!pdcrt_es_primitivo(ctx, llave))
        return false;

    pdcrt_entero hash = pdcrt_hash(ctx, llave);
    pdcrt_bucket *b = &tbl->buckets[hash & tbl->mascara];
    if(b->activo)
    {
        bool iguales = false;
        while(!((iguales = pdcrt_igualdad(ctx, b->llave, llave))) && b->tiene_colision)
            b = &tbl->colisiones[b->idc_colision];

        if(iguales)
        {
            *valor = b->valor;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

void pdcrt_tabla_eliminar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, pdcrt_obj llave, bool rehashear)
{
    if(!pdcrt_es_primitivo(ctx, llave))
        return;

    pdcrt_entero hash = pdcrt_hash(ctx, llave);
    pdcrt_bucket *b = &tbl->buckets[hash & tbl->mascara];
    if(b->activo)
    {
        if(pdcrt_igualdad(ctx, b->llave, llave))
        {
            b->activo = false;
            tbl->buckets_ocupados -= 1;
        }
        else
        {
            pdcrt_bucket *anterior = NULL, *actual = b;
            while(actual->tiene_colision)
            {
                anterior = actual;
                actual = &tbl->colisiones[anterior->idc_colision];

                if(pdcrt_igualdad(ctx, actual->llave, llave))
                {
                    size_t idc_actual = anterior->idc_colision;

                    actual->activo = false;
                    anterior->tiene_colision = actual->tiene_colision;
                    anterior->idc_colision = actual->idc_colision;

                    for(size_t i = 0; i < tbl->num_colisiones; i++)
                    {
                        pdcrt_bucket col = tbl->colisiones[i];
                        if(col.tiene_colision && col.idc_colision > idc_actual)
                        {
                            col.idc_colision -= 1;
                        }
                        if(i > idc_actual)
                        {
                            tbl->colisiones[i - 1] = col;
                            tbl->colisiones[i].activo = false;
                        }
                    }

                    tbl->num_colisiones -= 1;
                    tbl->buckets_ocupados -= 1;
                    return;
                }
            }
        }
    }

    if(rehashear && tbl->buckets_ocupados / pdcrt_tabla_num_buckets_hasheables(tbl->mascara) >= PDCRT_TABLA_OCUPACION_MAXIMA)
    {
        pdcrt_tabla_rehashear(ctx, tbl, tbl->buckets_ocupados);
    }
    else if(rehashear && tbl->buckets_ocupados <= pdcrt_tabla_num_buckets_hasheables(tbl->mascara) / PDCRT_TABLA_OCUPACION_MINIMA)
    {
        pdcrt_tabla_rehashear(ctx, tbl, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) / PDCRT_TABLA_OCUPACION_MINIMA);
    }
}

void pdcrt_tabla_vaciar(pdcrt_ctx *ctx, pdcrt_tabla *tbl, bool rehashear)
{
    for(size_t i = 0; i < pdcrt_tabla_num_buckets_hasheables(tbl->mascara); i++)
    {
        tbl->buckets[i].activo = false;
    }
    for(size_t i = 0; i < tbl->num_colisiones; i++)
    {
        tbl->colisiones[i].activo = false;
    }
    tbl->num_colisiones = 0;
    tbl->buckets_ocupados = 0;

    if(rehashear)
    {
        pdcrt_tabla_rehashear(ctx, tbl, pdcrt_tabla_num_buckets_hasheables(tbl->mascara) / PDCRT_TABLA_OCUPACION_MINIMA);
    }
}
