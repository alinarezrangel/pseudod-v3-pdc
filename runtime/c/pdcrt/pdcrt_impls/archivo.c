//
// Created by alinarezrangel on 2/5/26.
//

#include "archivo.h"

#include "pdcrt/pdcrt_ops.h"


pdcrt_tk pdcrt_recv_archivo(pdcrt_ctx *ctx, int args, pdcrt_k k, PDCRT_F_IMM)
{
    // [yo, msj, ...#args]
    size_t argp = PDCRT_CALC_ARGS();
    pdcrt_obj oyo = pdcrt_obj_desde_xmm(yo);
    pdcrt_obj omsj = pdcrt_obj_desde_xmm(msj);
    pdcrt_debe_tener_tipo_rapido(ctx, omsj, &pdcrt_recv_texto);

    PDCRT_PROBE0(recv_archivo);

    if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.cerrar))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, "Archivo: cerrar no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo)
        {
            (void) (*ctx->vio.vtable->op_cerrar_archivo)(ctx->vio.ctx, arch->archivo);
            arch->archivo = NULL;
            arch->eof = true;
            oyo.valop->liberar = NULL;
        }
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.esta_abierto)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.esta_abierto2))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, "Archivo: estáAbierto no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(arch->archivo != NULL)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.leer_byte))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, "Archivo: leerByte no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_error(ctx, u8"El archivo no está abierto");
        unsigned char bytes[1];
        pdcrt_vio_buffer buffer = {
            .ptr = (char *) bytes,
            .cap = 1,
            .tam = 0,
        };
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_leer)(ctx->vio.ctx, arch->archivo, &buffer, 1);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_error(ctx, "No se pudo leer el archivo");
        if(buffer.tam == 0)
        {
            // EOF
            arch->eof = true;
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(-1)));
        }
        else
        {
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(bytes[0])));
        }
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.obtener_siguiente_byte))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, "Archivo: obtenerSiguienteByte no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        size_t pos = 0;
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_posicionar)(ctx->vio.ctx, arch->archivo, PDCRT_ARCHIVO_ANCLA_ACTUAL, 0, &pos);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_errortb(ctx, k.marco, "Archivo#obtenerSiguienteByte: No se pudo posicionar en el archivo");
        unsigned char bytes[1];
        pdcrt_vio_buffer buffer = {
            .ptr = (char *) bytes,
            .cap = 1,
            .tam = 0,
        };
        ioerr = (*ctx->vio.vtable->op_leer)(ctx->vio.ctx, arch->archivo, &buffer, 1);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_errortb(ctx, k.marco, "No se pudo leer el archivo");
        ioerr = (*ctx->vio.vtable->op_posicionar)(ctx->vio.ctx, arch->archivo, PDCRT_ARCHIVO_ANCLA_INICIO, pos, NULL);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_errortb(ctx, k.marco, "Archivo#obtenerSiguienteByte: No se pudo posicionar en el archivo");
        if(buffer.tam == 0)
            // EOF
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(-1)));
        else
            return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(bytes[0])));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.escribir_byte))
    {
        if(args != 1)
            pdcrt_errortb(ctx, k.marco, "Archivo: escribirByte necesita 1 argumento");
        bool ok = false;
        pdcrt_entero byte = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_errortb(ctx, k.marco, "Archivo: escribirByte necesita un entero como argumento");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        unsigned char bytes[1] = {byte};
        pdcrt_vio_cadena str = {
            .ptr = (char *) bytes,
            .tam = 1,
        };
        size_t escrito = 0;
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_escribir)(ctx->vio.ctx, arch->archivo, &str, 1, &escrito);
        if(ioerr != PDCRT_IO_OK || escrito == 0)
            pdcrt_errortb(ctx, k.marco, "No se pudo escribir el archivo");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.escribir_texto))
    {
        if(args != 1)
            pdcrt_errortb(ctx, k.marco, "Archivo: escribirTexto necesita 1 argumento");
        pdcrt_obj txt = pdcrt_obj_desde_xmm(a1);
        pdcrt_debe_tener_tipo_rapido(ctx, txt, &pdcrt_recv_texto);
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        pdcrt_vio_cadena str = {
            .ptr = txt.texto->contenido,
            .tam = txt.texto->longitud,
        };
        size_t escrito = 0;
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_escribir)(ctx->vio.ctx, arch->archivo, &str, 1, &escrito);
        if(ioerr != PDCRT_IO_OK || escrito != txt.texto->longitud)
            pdcrt_errortb(ctx, k.marco, "No se pudo escribir el archivo");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.posicion_actual)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.posicion_actual2))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, u8"Archivo: posiciónActual no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        size_t pos = 0;
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_posicionar)(ctx->vio.ctx, arch->archivo, PDCRT_ARCHIVO_ANCLA_ACTUAL, 0, &pos);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_errortb(ctx, k.marco, "No se pudo posicionar el archivo");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_entero(pos)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.cambiar_posicion)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.cambiar_posicion2))
    {
        if(args != 1)
            pdcrt_errortb(ctx, k.marco, u8"Archivo: cambiarPosición necesita 1 argumento");
        bool ok = false;
        pdcrt_entero pos = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_errortb(ctx, k.marco, u8"cambiarPosición necesita un entero");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_posicionar)(ctx->vio.ctx, arch->archivo, PDCRT_ARCHIVO_ANCLA_INICIO, pos, NULL);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_errortb(ctx, k.marco, "No se pudo posicionar el archivo");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.cambiar_posicion)
        || pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.cambiar_posicion2))
    {
        if(args != 1)
            pdcrt_errortb(ctx, k.marco, u8"Archivo: cambiarPosición necesita 1 argumento");
        bool ok = false;
        pdcrt_entero pos = pdcrt_obtener_entero_obj(ctx, pdcrt_obj_desde_xmm(a1), &ok);
        if(!ok)
            pdcrt_errortb(ctx, k.marco, u8"cambiarPosición necesita un entero");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        pdcrt_io_error ioerr = (*ctx->vio.vtable->op_posicionar)(ctx->vio.ctx, arch->archivo, PDCRT_ARCHIVO_ANCLA_INICIO, pos, NULL);
        if(ioerr != PDCRT_IO_OK)
            pdcrt_errortb(ctx, k.marco, "No se pudo posicionar el archivo");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_nulo()));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.fin_del_archivo))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, u8"Archivo: cambiarPosición no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(arch->eof)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.error))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, u8"Archivo: error no necesita argumentos");
        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_booleano(false)));
    }
    else if(pdcrt_comparar_textos(omsj.texto, ctx->textos_globales.leer_todo))
    {
        if(args != 0)
            pdcrt_errortb(ctx, k.marco, u8"Archivo: __leerTodo no necesita argumentos");
        pdcrt_rsc_archivo *arch = (void *) oyo.valop->datos;
        if(arch->archivo == NULL)
            pdcrt_errortb(ctx, k.marco, u8"El archivo no está abierto");

        size_t tbuffer_cap = 4 * 1024, tbuffer_len = 0;
        char *tbuffer = pdcrt_alojar_ctx(ctx, tbuffer_cap);
        if(!tbuffer)
            pdcrt_enomem(ctx);

        size_t buffer_cap = 4 * 1024;
        char *subbuffer = pdcrt_alojar_ctx(ctx, buffer_cap);
        if(!subbuffer)
            pdcrt_enomem(ctx);

        while(1)
        {
            pdcrt_vio_buffer buffer = {
                .cap = buffer_cap,
                .tam = 0,
                .ptr = subbuffer,
            };
            pdcrt_io_error ioerr = (*ctx->vio.vtable->op_leer)(ctx->vio.ctx, arch->archivo, &buffer, 1);
            if(ioerr != PDCRT_IO_OK)
                pdcrt_errortb(ctx, k.marco, "Archivo#__leerTodo: no se pudo leer el archivo");

            if(tbuffer_len + buffer.tam > tbuffer_cap)
            {
                tbuffer = pdcrt_realojar_ctx(ctx, tbuffer, tbuffer_cap, tbuffer_cap * 2);
                if(!tbuffer)
                    pdcrt_enomem(ctx);
                tbuffer_cap *= 2;
            }

            memcpy(tbuffer + tbuffer_len, buffer.ptr, buffer.tam);
            tbuffer_len += buffer.tam;

            if(buffer.tam < buffer.cap)
                break;
        }

        PDCRT_DEFINE_RAICES(1);
        PDCRT_GUARDAR_RAIZ_K(0, k);
        pdcrt_texto *txt = pdcrt_crear_texto(ctx, PDCRT_GC(), tbuffer, tbuffer_len);
        PDCRT_CARGAR_RAIZ_K(0, k);

        pdcrt_desalojar_ctx(ctx, tbuffer, tbuffer_cap);
        pdcrt_desalojar_ctx(ctx, subbuffer, buffer_cap);

        return pdcrt_continuar(ctx, k, pdcrt_xmm_desde_obj(pdcrt_objeto_texto(txt)));
    }

    PDCRT_ASSERT(0 && "sin implementar");
}

void pdcrt_liberar_rsc_archivo(pdcrt_ctx *ctx, void *datos, size_t ndatos)
{
    PDCRT_ASSERT(ndatos == sizeof(pdcrt_rsc_archivo));
    pdcrt_rsc_archivo *arch = datos;
    if(arch->archivo)
    {
        (void) (*ctx->vio.vtable->op_cerrar_archivo)(ctx->vio.ctx, arch->archivo);
        arch->archivo = NULL;
        arch->eof = true;
        puts("cerrado");
    }
}
