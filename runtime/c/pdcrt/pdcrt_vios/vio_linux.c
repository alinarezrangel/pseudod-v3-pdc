//
// Created by alinarezrangel on 22/2/26.
//

// Ok: we use O_PATH which is linux specific
#define _GNU_SOURCE // NOLINT(*-reserved-identifier)

#include "vio_linux.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/stat.h>


static pdcrt_io_error pdcrt_vio_linux_abrir_archivo(
    void *ctx,
    pdcrt_archivo **out_archivo,
    pdcrt_opciones_abrir_archivo *opciones);
static pdcrt_io_error pdcrt_vio_linux_abrir_directorio(
    void *ctx,
    pdcrt_directorio **out_directorio,
    pdcrt_opciones_abrir_directorio *opciones);
static pdcrt_io_error pdcrt_vio_linux_cerrar_archivo(void *ctx, pdcrt_archivo *archivo);
static pdcrt_io_error pdcrt_vio_linux_cerrar_directorio(void *ctx, pdcrt_directorio *directorio);
static pdcrt_io_error pdcrt_vio_linux_crear_directorio_temporal(void *ctx,
    pdcrt_directorio **out_directorio,
    pdcrt_opciones_crear_directorio_temporal *opciones);
static pdcrt_io_error pdcrt_vio_linux_obtener_ruta_actual(
    void *ctx,
    pdcrt_vio_buffer *out_ruta,
    bool *out_completo);
static pdcrt_io_error pdcrt_vio_linux_cambiar_ruta_actual(
    void *ctx,
    pdcrt_vio_cadena ruta);
static pdcrt_io_error pdcrt_vio_linux_cambiar_ruta_actual_dir(void *ctx, pdcrt_directorio *dir);
static pdcrt_io_error pdcrt_vio_linux_iterar_directorio(
    void *ctx,
    pdcrt_directorio *dir,
    bool *eof,
    pdcrt_registro_directorio *out_registro);
static pdcrt_io_error pdcrt_vio_linux_leer(
    void *ctx,
    pdcrt_archivo *archivo,
    pdcrt_vio_buffer *out_buffers,
    size_t num_buffers);
static pdcrt_io_error pdcrt_vio_linux_escribir(
    void *ctx,
    pdcrt_archivo *archivo,
    pdcrt_vio_cadena *cadenas,
    size_t num_cadenas,
    size_t *out_escrito);
static pdcrt_io_error pdcrt_vio_linux_posicionar(
    void *ctx,
    pdcrt_archivo *archivo,
    pdcrt_archivo_ancla ancla,
    ssize_t offset,
    size_t *resultado);
static pdcrt_io_error pdcrt_vio_linux_borrar_archivo_por_ruta(
    void *ctx,
    pdcrt_directorio *relativo_a,
    pdcrt_vio_cadena ruta);
static pdcrt_io_error pdcrt_vio_linux_borrar_directorio_por_ruta(
    void *ctx,
    pdcrt_directorio *relativo_a,
    pdcrt_vio_cadena ruta);
static pdcrt_io_error pdcrt_vio_linux_renombrar_archivo(
    void *ctx,
    pdcrt_directorio *primero_relativo_a,
    pdcrt_vio_cadena primera_ruta,
    pdcrt_directorio *segundo_relativo_a,
    pdcrt_vio_cadena segunda_ruta);
static pdcrt_io_error pdcrt_vio_linux_abrir_reloj(
    void *ctx,
    pdcrt_reloj **out_reloj,
    pdcrt_tipo_reloj tipo);
static pdcrt_io_error pdcrt_vio_linux_cerrar_reloj(void *ctx, pdcrt_reloj *reloj);
static pdcrt_io_error pdcrt_vio_linux_instante_actual(
    void *ctx,
    pdcrt_reloj *reloj,
    pdcrt_instante *out_instante);
static pdcrt_io_error pdcrt_vio_linux_fecha_y_hora_gregoriana_local(
    void *ctx,
    pdcrt_reloj *reloj,
    pdcrt_instante instante,
    pdcrt_fecha_y_hora_gregoriana *out_fecha_y_hora);
static pdcrt_io_error pdcrt_vio_linux_fecha_y_hora_gregoriana_utc(
    void *ctx,
    pdcrt_reloj *reloj,
    pdcrt_instante instante,
    pdcrt_fecha_y_hora_gregoriana *out_fecha_y_hora);
static pdcrt_io_error pdcrt_vio_linux_obtener_variable_de_entorno(
    void *ctx,
    pdcrt_vio_cadena variable,
    pdcrt_vio_buffer *out_valor,
    size_t *tam_valor);
static pdcrt_io_error pdcrt_vio_linux_crear_subproceso(
    void *ctx,
    pdcrt_subproceso **out_subproceso,
    pdcrt_opciones_crear_subproceso *opciones);
static pdcrt_io_error pdcrt_vio_linux_esperar_por_subproceso(void *ctx, pdcrt_subproceso *subproceso);
static pdcrt_io_error pdcrt_vio_linux_matar_subproceso(void *ctx, pdcrt_subproceso *subproceso, bool forzar);
static pdcrt_io_error pdcrt_vio_linux_estado_de_subproceso(void *ctx,
    pdcrt_subproceso *subproceso,
    pdcrt_estado_de_subproceso *out_estado);

#ifdef PATH_MAX
#define PDCRT_LINUX_PATH_MAX PATH_MAX
#else
#define PDCRT_LINUX_PATH_MAX _POSIX_PATH_MAX
#endif

#ifdef NAME_MAX
#define PDCRT_LINUX_NAME_MAX NAME_MAX
#else
#define PDCRT_LINUX_NAME_MAX _POSIX_NAME_MAX
#endif

#ifdef ARG_MAX
#define PDCRT_LINUX_ARG_MAX ARG_MAX
#else
#define PDCRT_LINUX_ARG_MAX _POSIX_ARG_MAX
#endif

static const pdcrt_vio_vtable pdcrt_linux_vtable = {
    .limites = {
        .ruta_maxima = PDCRT_LINUX_PATH_MAX,
        .nombre_de_archivo_maximo = PDCRT_LINUX_NAME_MAX,
        .archivo_maximo = SIZE_MAX,
        .entorno_maximo = PDCRT_LINUX_ARG_MAX,
        .linea_de_comandos_maxima = PDCRT_LINUX_ARG_MAX,
        .entorno_y_linea_de_comandos_compartidos = true,
    },

    .quirks = {
        .rutas_wtf8 = false,
        .linea_de_comandos_windows = false,
    },

    .op_abrir_archivo = &pdcrt_vio_linux_abrir_archivo,
    .op_abrir_directorio = &pdcrt_vio_linux_abrir_directorio,
    .op_cerrar_archivo = &pdcrt_vio_linux_cerrar_archivo,
    .op_cerrar_directorio = &pdcrt_vio_linux_cerrar_directorio,
    .op_crear_directorio_temporal = &pdcrt_vio_linux_crear_directorio_temporal,
    .op_obtener_ruta_actual = &pdcrt_vio_linux_obtener_ruta_actual,
    .op_cambiar_ruta_actual = &pdcrt_vio_linux_cambiar_ruta_actual,
    .op_cambiar_ruta_actual_dir = &pdcrt_vio_linux_cambiar_ruta_actual_dir,
    .op_iterar_directorio = &pdcrt_vio_linux_iterar_directorio,
    .op_leer = &pdcrt_vio_linux_leer,
    .op_escribir = &pdcrt_vio_linux_escribir,
    .op_posicionar = &pdcrt_vio_linux_posicionar,
    .op_borrar_archivo_por_ruta = &pdcrt_vio_linux_borrar_archivo_por_ruta,
    .op_borrar_directorio_por_ruta = &pdcrt_vio_linux_borrar_directorio_por_ruta,
    .op_renombrar_archivo = &pdcrt_vio_linux_renombrar_archivo,
    .op_abrir_reloj = &pdcrt_vio_linux_abrir_reloj,
    .op_cerrar_reloj = &pdcrt_vio_linux_cerrar_reloj,
    .op_instante_actual = &pdcrt_vio_linux_instante_actual,
    .op_fecha_y_hora_gregoriana_local = &pdcrt_vio_linux_fecha_y_hora_gregoriana_local,
    .op_fecha_y_hora_gregoriana_utc = &pdcrt_vio_linux_fecha_y_hora_gregoriana_utc,
    .op_obtener_variable_de_entorno = &pdcrt_vio_linux_obtener_variable_de_entorno,
    .op_crear_subproceso = &pdcrt_vio_linux_crear_subproceso,
    .op_esperar_por_subproceso = &pdcrt_vio_linux_esperar_por_subproceso,
    .op_matar_subproceso = &pdcrt_vio_linux_matar_subproceso,
    .op_estado_de_subproceso = &pdcrt_vio_linux_estado_de_subproceso,
};

static int pdcrt_linux_marca = 0;

struct pdcrt_archivo
{
    int fd;
};

struct pdcrt_directorio
{
    int fd;
    DIR *dir;
};

struct pdcrt_subproceso
{
    bool vivo;
    int pid;
    bool capturo_el_estado_de_salida;
    int estado_de_salida;
    bool termino_por_otro_motivo;
};

struct pdcrt_reloj
{
    clockid_t clock;
};

pdcrt_vio pdcrt_vio_global_para_linux(void)
{
    return (pdcrt_vio) {
        .vtable = &pdcrt_linux_vtable,
        .ctx = &pdcrt_linux_marca,
    };
}

static bool pdcrt_vio_es_cstr(pdcrt_vio_cadena c)
{
    for(size_t i = 0; i < c.tam - 1; ++i)
        if(!c.ptr[i])
            return false;
    return c.ptr[c.tam - 1] == 0;
}

static pdcrt_io_error pdcrt_vio_linux_abrir_archivo(
    void *ctx,
    pdcrt_archivo **out_archivo,
    pdcrt_opciones_abrir_archivo *opciones)
{
    pdcrt_io_error err = PDCRT_IO_OK;

    PDCRT_BUG(ctx != &pdcrt_linux_marca, u8"ctx inválido");
    PDCRT_BUG(!out_archivo, "out_archivo es NULL");
    PDCRT_BUG(!opciones, "opciones es NULL");

    if(!pdcrt_vio_es_cstr(opciones->nombre))
    {
        err = PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        goto error;
    }

    *out_archivo = malloc(sizeof(pdcrt_archivo));
    if(!*out_archivo)
    {
        err = PDCRT_IO_ERROR_SIN_MEMORIA;
        goto error;
    }

    int oflags = 0, mode = 0;

    PDCRT_BUG_SI_FUERA_DE_MASCARA(PDCRT_ABRIR_LECTURA | PDCRT_ABRIR_ESCRITURA, opciones->intencion,
                                  "opciones->intencion no es LECTURA o ESCRITURA");
    if((opciones->intencion & (PDCRT_ABRIR_LECTURA | PDCRT_ABRIR_ESCRITURA)) == (PDCRT_ABRIR_LECTURA | PDCRT_ABRIR_ESCRITURA))
        oflags |= O_RDWR;
    else if(opciones->intencion & PDCRT_ABRIR_LECTURA)
        oflags |= O_RDONLY;
    else if(opciones->intencion & PDCRT_ABRIR_ESCRITURA)
        oflags |= O_WRONLY;

    PDCRT_BUG(opciones->accion_crear != PDCRT_ACCION_CREAR_NUEVO
              && opciones->accion_crear != PDCRT_ACCION_ERROR_NUEVO,
              "opciones->accion_crear no es CREAR_NUEVO o ERROR_NUEVO");
    if(opciones->accion_crear == PDCRT_ACCION_CREAR_NUEVO)
    {
        oflags |= O_CREAT;
        mode |= (opciones->permisos_al_crear & PDCRT_PERMISO_LEER) ? S_IRUSR | S_IRGRP : 0;
        mode |= (opciones->permisos_al_crear & PDCRT_PERMISO_ESCRIBIR) ? S_IWUSR | S_IWGRP : 0;
        mode |= (opciones->permisos_al_crear & PDCRT_PERMISO_EJECUTAR) ? S_IXUSR | S_IXGRP : 0;
    }

    PDCRT_BUG(
        opciones->accion_abrir != PDCRT_ACCION_ABRIR_EXISTENTE
        && opciones->accion_abrir != PDCRT_ACCION_ERROR_EXISTENTE
        && opciones->accion_abrir != PDCRT_ACCION_TRUNCAR_EXISTENTE,
        "opciones->accion_abrir no es ABRIR, ERROR o TRUNCAR");
    if(opciones->accion_abrir == PDCRT_ACCION_TRUNCAR_EXISTENTE)
    {
        oflags |= O_TRUNC;
    }
    else if(opciones->accion_abrir == PDCRT_ACCION_ERROR_EXISTENTE)
    {
        if(oflags & O_CREAT)
            oflags |= O_EXCL;
    }

    PDCRT_BUG_SI_FUERA_DE_MASCARA(PDCRT_ABRIR_ARCHIVO_CONCATENAR | PDCRT_ABRIR_ARCHIVO_HEREDABLE,
                                  opciones->flags,
                                  "opciones->flags no es CONCATENAR ni HEREDABLE");
    if(opciones->flags & PDCRT_ABRIR_ARCHIVO_CONCATENAR)
    {
        oflags |= O_APPEND;
    }
    if(!(opciones->flags & PDCRT_ABRIR_ARCHIVO_HEREDABLE))
    {
        oflags |= O_CLOEXEC;
    }

    int fd = 0;
    if(opciones->relativo_a)
        fd = openat(opciones->relativo_a->fd, opciones->nombre.ptr, oflags, mode);
    else
        fd = open(opciones->nombre.ptr, oflags, mode);

    if(fd < 0)
    {
        switch(errno)
        {
        case EACCES:
            err = PDCRT_IO_ERROR_SIN_PERMISO;
            goto error;
        case EBADF:
        case EINVAL:
        case ENAMETOOLONG:
            err = PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
            goto error;
        case EEXIST:
            err = PDCRT_IO_ERROR_YA_EXISTE;
            goto error;
        case ENOENT:
            err = PDCRT_IO_ERROR_NO_EXISTE;
            goto error;
        case ENOMEM:
            err = PDCRT_IO_ERROR_SIN_MEMORIA;
            goto error;
        case ENOTDIR:
            err = PDCRT_IO_ERROR_NO_ES_DIRECTORIO;
            goto error;
        default:
            err = PDCRT_IO_ERROR_DESCONOCIDO;
            goto error;
        }
    }

    if(opciones->accion_abrir == PDCRT_ACCION_ERROR_EXISTENTE
        && opciones->accion_crear == PDCRT_ACCION_ERROR_NUEVO)
    {
        err = PDCRT_IO_ERROR_YA_EXISTE;
        goto error;
    }

    (*out_archivo)->fd = fd;

    return err;
error:
    if(*out_archivo)
        free(*out_archivo);
    *out_archivo = NULL;
    return err;
}

static pdcrt_io_error pdcrt_vio_linux_abrir_directorio(
    void *ctx,
    pdcrt_directorio **out_directorio,
    pdcrt_opciones_abrir_directorio *opciones)
{
    pdcrt_io_error err = PDCRT_IO_OK;

    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");

    *out_directorio = malloc(sizeof(pdcrt_directorio));
    if(!*out_directorio)
    {
        err = PDCRT_IO_ERROR_SIN_MEMORIA;
        goto error;
    }

    if(!pdcrt_vio_es_cstr(opciones->nombre))
    {
        err = PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        goto error;
    }

    int oflags = O_DIRECTORY;
    bool mk = false;

    PDCRT_BUG_SI_FUERA_DE_MASCARA(PDCRT_ABRIR_ITERAR | PDCRT_ABRIR_REFERENCIAR, opciones->intencion,
                                  "opciones->intencion no es ITERAR o REFERENCIAR");
    if(opciones->intencion & PDCRT_ABRIR_ITERAR)
        oflags |= O_RDONLY;
    else if(opciones->intencion & PDCRT_ABRIR_REFERENCIAR)
        oflags |= O_PATH;

    PDCRT_BUG(opciones->accion_crear != PDCRT_ACCION_CREAR_NUEVO
              && opciones->accion_crear != PDCRT_ACCION_ERROR_NUEVO,
              "opciones->accion_crear no es CREAR_NUEVO o ERROR_NUEVO");
    if(opciones->accion_crear == PDCRT_ACCION_CREAR_NUEVO)
    {
        if(opciones->intencion & PDCRT_ABRIR_REFERENCIAR)
        {
            err = PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
            goto error;
        }

        mk = true;
    }

    PDCRT_BUG_SI_FUERA_DE_MASCARA(PDCRT_ABRIR_DIRECTORIO_HEREDABLE,
                                  opciones->flags,
                                  "opciones->flags no es HEREDABLE");
    if(!(opciones->flags & PDCRT_ABRIR_DIRECTORIO_HEREDABLE))
    {
        oflags |= O_CLOEXEC;
    }

    /* En las siguientes líneas no estoy revisando el valor de retorno de mkdir(2), ni estoy asegurandome de que no
     * tenga errores. Hasta donde tengo entendido esto es lo correcto: en general, si mkdir falla puede ser por uno
     * de los siguientes motivos:
     *
     * 1. El directorio ya existe: entonces el error debería ser ignorado.
     * 2. Permisos: ignoramos el error, el directorio puede existir o no, en ambos casos openat(2) hace lo correcto
     *    (fallar porque no existe / abrirlo si tenemos permisos de lectura).
     * 3. Errores del SO, por ejemplo, EDQUOT, ELOOP, ENAMETOOLONG, ENOSPC, EROFS, etc.: hay una condición de carrera
     *    entre el mkdir que falló y openat: si alguien más crea el directorio entre las llamadas, openat funcionará
     *    lo cual es lo correcto, mientras que si nadie crea el directorio openat fallará de todas formas.
     *
     * El resultado debería ser una aproximación de si openat tuviese una bandera O_CREAT_DIRECTORY (o si
     * O_CREAT | O_DIRECTORY funcionara).
     *
     * La unica parte mala de todo esto es que si mkdir funciona pero alguien más borra el directorio, openat fallará.
     * Idealmente las semanticas serían similares a openat + O_CREAT: si alguien borra el directorio debería permanecer
     * referenciado solo por este proceso.
     */
    int fd = 0;
    if(opciones->relativo_a)
    {
        if(mk)
            mkdirat(opciones->relativo_a->fd, opciones->nombre.ptr, 0755);
        fd = openat(opciones->relativo_a->fd, opciones->nombre.ptr, oflags);
    }
    else
    {
        if(mk)
            mkdir(opciones->nombre.ptr, 0755);
        fd = open(opciones->nombre.ptr, oflags);
    }

    if(fd < 0)
    {
        switch(errno)
        {
        case EACCES:
            err = PDCRT_IO_ERROR_SIN_PERMISO;
            goto error;
        case EBADF:
        case EINVAL:
        case ENAMETOOLONG:
            err = PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
            goto error;
        case EEXIST:
            err = PDCRT_IO_ERROR_YA_EXISTE;
            goto error;
        case ENOENT:
            err = PDCRT_IO_ERROR_NO_EXISTE;
            goto error;
        case ENOMEM:
            err = PDCRT_IO_ERROR_SIN_MEMORIA;
            goto error;
        default:
            err = PDCRT_IO_ERROR_DESCONOCIDO;
            goto error;
        }
    }

    DIR *dir = NULL;
    if(!(opciones->intencion & PDCRT_ABRIR_REFERENCIAR))
    {
        dir = fdopendir(fd);
        if(!dir)
        {
            switch(errno)
            {
            case EACCES:
                err = PDCRT_IO_ERROR_SIN_PERMISO;
                goto error;
            case EBADF:
                err = PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
                goto error;
            case ENOENT:
                err = PDCRT_IO_ERROR_NO_EXISTE;
                goto error;
            case ENOMEM:
                err = PDCRT_IO_ERROR_SIN_MEMORIA;
                goto error;
            case ENOTDIR:
                err = PDCRT_IO_ERROR_NO_ES_DIRECTORIO;
                goto error;
            default:
                err = PDCRT_IO_ERROR_DESCONOCIDO;
                goto error;
            }
        }
    }

    (*out_directorio)->fd = fd;
    (*out_directorio)->dir = dir;

    return err;
error:
    if(*out_directorio)
        free(*out_directorio);
    *out_directorio = NULL;
    return err;
}

static pdcrt_io_error pdcrt_vio_linux_cerrar_archivo(void *ctx, pdcrt_archivo *archivo)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    close(archivo->fd);
    free(archivo);
    return PDCRT_IO_OK;
}

static pdcrt_io_error pdcrt_vio_linux_cerrar_directorio(void *ctx, pdcrt_directorio *directorio)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    if(directorio->dir)
        closedir(directorio->dir);
    else
        close(directorio->fd);
    return PDCRT_IO_OK;
}

static char pdcrt_caracter_aleatorio_de_ruta(void)
{
    uint32_t r = arc4random_uniform(62);
    if(r < 10)
        return (char) ('0' + r);
    else if(r < 36)
        return (char) ('a' + (r - 10));
    else
        return (char) ('A' + (r - 36));
}

static pdcrt_io_error pdcrt_vio_linux_crear_directorio_temporal(void *ctx,
    pdcrt_directorio **out_directorio,
    pdcrt_opciones_crear_directorio_temporal *opciones)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!out_directorio, "out_directorio inválido");
    PDCRT_BUG(!opciones, "opciones inválidas");

    PDCRT_BUG(opciones->plantilla.cap >= opciones->plantilla.tam, "plantilla inválida");
    size_t ntmpl = opciones->plantilla.cap - opciones->plantilla.tam;
    if(ntmpl < 7)
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;

    for(size_t i = opciones->plantilla.tam; i < opciones->plantilla.cap; i++)
    {
        opciones->plantilla.ptr[i] = pdcrt_caracter_aleatorio_de_ruta();
    }
    opciones->plantilla.tam = opciones->plantilla.cap;
    opciones->plantilla.ptr[opciones->plantilla.tam - 1] = 0;
    pdcrt_vio_cadena nombre_del_archivo = pdcrt_vio_buffer_como_cadena(opciones->plantilla);
    PDCRT_BUG(!pdcrt_vio_es_cstr(nombre_del_archivo),
              "plantilla debió quedar como un cstr");

    pdcrt_opciones_abrir_directorio opciones_abrir = {
        .relativo_a = opciones->relativo_a,
        .nombre = nombre_del_archivo,
        .accion_crear = PDCRT_ACCION_CREAR_NUEVO,
        .intencion = opciones->intencion,
        .flags = opciones->flags,
    };
    return pdcrt_vio_linux_abrir_directorio(ctx, out_directorio, &opciones_abrir);
}

static pdcrt_io_error pdcrt_vio_linux_obtener_ruta_actual(
    void *ctx,
    pdcrt_vio_buffer *out_ruta,
    bool *out_completo)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    if(!getcwd(out_ruta->ptr, out_ruta->cap))
    {
        *out_completo = false;
        switch(errno)
        {
        case EACCES: return PDCRT_IO_ERROR_SIN_PERMISO;
        case ENOENT: return PDCRT_IO_ERROR_NO_EXISTE;
        case ERANGE:
        case ENOMEM:
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
    else
    {
        out_ruta->tam = strlen(out_ruta->ptr);
        *out_completo = true;
        return PDCRT_IO_OK;
    }
}

static pdcrt_io_error pdcrt_vio_linux_cambiar_ruta_actual(
    void *ctx,
    pdcrt_vio_cadena ruta)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");

    if(!pdcrt_vio_es_cstr(ruta))
    {
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
    }

    if(chdir(ruta.ptr) != 0)
    {
        switch(errno)
        {
        case EACCES: return PDCRT_IO_ERROR_SIN_PERMISO;
        case ENAMETOOLONG: return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case ENOENT: return PDCRT_IO_ERROR_NO_EXISTE;
        case ENOMEM: return PDCRT_IO_ERROR_SIN_MEMORIA;
        case ENOTDIR: return PDCRT_IO_ERROR_NO_ES_DIRECTORIO;
        default: return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
    else
    {
        return PDCRT_IO_OK;
    }
}

static pdcrt_io_error pdcrt_vio_linux_cambiar_ruta_actual_dir(void *ctx, pdcrt_directorio *dir)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!dir, "dir es NULL");

    if(fchdir(dir->fd) != 0)
    {
        switch(errno)
        {
        case EACCES: return PDCRT_IO_ERROR_SIN_PERMISO;
        case ENAMETOOLONG: return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case ENOENT: return PDCRT_IO_ERROR_NO_EXISTE;
        case ENOMEM: return PDCRT_IO_ERROR_SIN_MEMORIA;
        case ENOTDIR: return PDCRT_IO_ERROR_NO_ES_DIRECTORIO;
        default: return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
    else
    {
        return PDCRT_IO_OK;
    }
}

static pdcrt_io_error pdcrt_vio_linux_iterar_directorio(
    void *ctx,
    pdcrt_directorio *dir,
    bool *eof,
    pdcrt_registro_directorio *out_registro)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!dir, "dir inválido");
    PDCRT_BUG(!eof, "eof inválido");
    PDCRT_BUG(!out_registro, "out_registro inválido");

    if(!dir->dir)
        return PDCRT_IO_ERROR_SIN_PERMISO;

    long off = telldir(dir->dir);
    if(off < 0)
    {
        if(errno == EBADF)
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        else
            return PDCRT_IO_ERROR_DESCONOCIDO;
    }

    errno = 0;
    struct dirent *ent = readdir(dir->dir);
    if(!ent && errno != 0)
    {
        switch(errno)
        {
        case EBADF:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
    else if(!ent)
    {
        *eof = true;
        out_registro->tam_nombre = 0;
        return PDCRT_IO_OK;
    }
    else
    {
        *eof = false;
        out_registro->tam_nombre = ent->d_reclen;
        if(out_registro->nombre.cap < out_registro->tam_nombre)
        {
            seekdir(dir->dir, off);
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        }
        memcpy(out_registro->nombre.ptr, ent->d_name, out_registro->tam_nombre);
        out_registro->nombre.tam = out_registro->tam_nombre;
        return PDCRT_IO_OK;
    }
}

static pdcrt_io_error pdcrt_vio_linux_leer(
    void *ctx,
    pdcrt_archivo *archivo,
    pdcrt_vio_buffer *out_buffers,
    size_t num_buffers)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!archivo, "archivo inválido");
    PDCRT_BUG(!out_buffers, "out_buffers inválido");

    if(num_buffers >= INT_MAX)
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
#ifdef IOV_MAX
    if(num_buffers >= IOV_MAX)
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
#endif

    struct iovec *vecs = malloc(sizeof(struct iovec) * num_buffers);
    for(size_t i = 0; i < num_buffers; i++)
    {
        vecs[i].iov_base = out_buffers[i].ptr;
        vecs[i].iov_len = out_buffers[i].cap;
    }

    ssize_t readed = readv(archivo->fd, vecs, (int) num_buffers);
    if(readed >= 0)
    {
        free(vecs);
        size_t readed_bytes = (size_t) readed;
        for(size_t i = 0; i < num_buffers; i++)
        {
            if(readed_bytes > out_buffers[i].cap)
                out_buffers[i].tam = out_buffers[i].cap;
            else
                out_buffers[i].tam = readed_bytes;
            readed_bytes -= out_buffers[i].cap;
        }

        return PDCRT_IO_OK;
    }
    else
    {
        int err = errno;
        free(vecs);
        switch(err)
        {
        case EINVAL:
        case EBADF:
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case EISDIR:
            return PDCRT_IO_ERROR_NO_ES_ARCHIVO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_escribir(
    void *ctx,
    pdcrt_archivo *archivo,
    pdcrt_vio_cadena *cadenas,
    size_t num_cadenas,
    size_t *out_escrito)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!archivo, "archivo inválido");
    PDCRT_BUG(!cadenas, "cadenas inválidas");

    if(num_cadenas == 0)
        return PDCRT_IO_OK;

    if(num_cadenas >= INT_MAX)
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
#ifdef IOV_MAX
    if(num_cadenas >= IOV_MAX)
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
#endif

    struct iovec *vecs = malloc(sizeof(struct iovec) * num_cadenas);
    for(size_t i = 0; i < num_cadenas; i++)
    {
        // El (void*) es seguro ya que nunca modificamos las cadenas.
        vecs[i].iov_base = (void*) cadenas[i].ptr;
        vecs[i].iov_len = cadenas[i].tam;
    }

    ssize_t written = writev(archivo->fd, vecs, (int) num_cadenas);
    if(written >= 0)
    {
        free(vecs);
        *out_escrito = written;
        return PDCRT_IO_OK;
    }
    else
    {
        int err = errno;
        free(vecs);
        *out_escrito = 0;
        switch(err)
        {
        case EINVAL:
        case EBADF:
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case EISDIR:
            return PDCRT_IO_ERROR_NO_ES_ARCHIVO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_posicionar(
    void *ctx,
    pdcrt_archivo *archivo,
    pdcrt_archivo_ancla ancla,
    ssize_t offset,
    size_t *resultado)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!archivo, "archivo inválido");

    int whence;
    switch(ancla)
    {
    case PDCRT_ARCHIVO_ANCLA_ACTUAL:
        whence = SEEK_CUR;
        break;
    case PDCRT_ARCHIVO_ANCLA_INICIO:
        whence = SEEK_SET;
        break;
    case PDCRT_ARCHIVO_ANCLA_FIN:
        whence = SEEK_END;
        break;
    default:
        PDCRT_BUG(true || ancla, "ancla no tiene un valor válido");
    }

    off_t res = lseek(archivo->fd, offset, whence);
    if(res >= 0)
    {
        if(resultado)
            *resultado = res;
        return PDCRT_IO_OK;
    }
    else
    {
        if(resultado)
            *resultado = 0;
        switch(errno)
        {
        case EINVAL:
        case EBADF:
        case ENXIO:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case EPIPE:
            return PDCRT_IO_ERROR_NO_ES_ARCHIVO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_borrar_archivo_por_ruta(
    void *ctx,
    pdcrt_directorio *relativo_a,
    pdcrt_vio_cadena ruta)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");

    if(!pdcrt_vio_es_cstr(ruta))
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;

    int at_fd;
    if(relativo_a)
        at_fd = relativo_a->fd;
    else
        at_fd = AT_FDCWD;

    if(unlinkat(at_fd, ruta.ptr, 0) == 0)
    {
        return PDCRT_IO_OK;
    }
    else
    {
        switch(errno)
        {
        case EACCES:
        case EPERM:
        case EISDIR:
        case ENOTDIR:
        case EROFS:
            return PDCRT_IO_ERROR_SIN_PERMISO;
        case EBADF:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case ENOMEM:
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        case EEXIST:
            return PDCRT_IO_ERROR_NO_EXISTE;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_borrar_directorio_por_ruta(
    void *ctx,
    pdcrt_directorio *relativo_a,
    pdcrt_vio_cadena ruta)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");

    if(!pdcrt_vio_es_cstr(ruta))
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;

    int at_fd;
    if(relativo_a)
        at_fd = relativo_a->fd;
    else
        at_fd = AT_FDCWD;

    if(unlinkat(at_fd, ruta.ptr, AT_REMOVEDIR) == 0)
    {
        return PDCRT_IO_OK;
    }
    else
    {
        switch(errno)
        {
        case EACCES:
        case EPERM:
        case EISDIR:
        case ENOTDIR:
        case EROFS:
            return PDCRT_IO_ERROR_SIN_PERMISO;
        case EBADF:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case ENOMEM:
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        case EEXIST:
            return PDCRT_IO_ERROR_NO_EXISTE;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_renombrar_archivo(
    void *ctx,
    pdcrt_directorio *primero_relativo_a,
    pdcrt_vio_cadena primera_ruta,
    pdcrt_directorio *segundo_relativo_a,
    pdcrt_vio_cadena segunda_ruta)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");

    if(!pdcrt_vio_es_cstr(primera_ruta))
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
    if(!pdcrt_vio_es_cstr(segunda_ruta))
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;

    int primero_fd, segundo_fd;
    if(primero_relativo_a)
        primero_fd = primero_relativo_a->fd;
    else
        primero_fd = AT_FDCWD;
    if(segundo_relativo_a)
        segundo_fd = segundo_relativo_a->fd;
    else
        segundo_fd = AT_FDCWD;

    int res = renameat(primero_fd, primera_ruta.ptr,
                       segundo_fd, segunda_ruta.ptr);
    if(res == 0)
    {
        return PDCRT_IO_OK;
    }
    else
    {
        switch(errno)
        {
        case EACCES:
        case EPERM:
        case EROFS:
            return PDCRT_IO_ERROR_SIN_PERMISO;
        case EINVAL:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case ENOENT:
            return PDCRT_IO_ERROR_NO_EXISTE;
        case ENOMEM:
        case ENOSPC:
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        case EEXIST:
            return PDCRT_IO_ERROR_YA_EXISTE;
        case ENOTDIR:
            return PDCRT_IO_ERROR_NO_ES_DIRECTORIO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_abrir_reloj(
    void *ctx,
    pdcrt_reloj **out_reloj,
    pdcrt_tipo_reloj tipo)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!out_reloj, "out_reloj inválido");

    *out_reloj = malloc(sizeof(pdcrt_reloj));
    switch(tipo)
    {
    case PDCRT_RELOJ_MONOTONICO:
        (*out_reloj)->clock = CLOCK_MONOTONIC;
        break;
    case PDCRT_RELOJ_REAL:
        (*out_reloj)->clock = CLOCK_REALTIME;
        break;
    }

    return PDCRT_IO_OK;
}

static pdcrt_io_error pdcrt_vio_linux_cerrar_reloj(void *ctx, pdcrt_reloj *reloj)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!reloj, "reloj inválido");
    free(reloj);
    return PDCRT_IO_OK;
}

static pdcrt_io_error pdcrt_vio_linux_instante_actual(
    void *ctx,
    pdcrt_reloj *reloj,
    pdcrt_instante *out_instante)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!reloj, "reloj inválido");
    PDCRT_BUG(!out_instante, "out_instante inválido");

    struct timespec ts = {0};
    if(clock_gettime(reloj->clock, &ts) == 0)
    {
        out_instante->segundos = ts.tv_sec;
        out_instante->nanosegundos = ts.tv_nsec;
        return PDCRT_IO_OK;
    }
    else
    {
        switch(errno)
        {
        case EACCES:
            return PDCRT_IO_ERROR_SIN_PERMISO;
        case EINVAL:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_fecha_y_hora_gregoriana_local(
    void *ctx,
    pdcrt_reloj *reloj,
    pdcrt_instante instante,
    pdcrt_fecha_y_hora_gregoriana *out_fecha_y_hora)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!reloj, "reloj inválido");
    PDCRT_BUG(!out_fecha_y_hora, "out_fecha_y_hora inválido");

    time_t t = instante.segundos;
    struct tm tm = {0};
    tzset();
    if(localtime_r(&t, &tm) == &tm)
    {
        out_fecha_y_hora->segundo = tm.tm_sec;
        out_fecha_y_hora->nanosegundo = instante.nanosegundos;
        out_fecha_y_hora->minuto = tm.tm_min;
        out_fecha_y_hora->hora = tm.tm_hour;
        out_fecha_y_hora->mes = tm.tm_mon + 1;
        out_fecha_y_hora->dia_del_mes = tm.tm_mday;
        out_fecha_y_hora->dia_de_la_semana = tm.tm_wday + 1;
        out_fecha_y_hora->dia_del_anno = tm.tm_yday + 1;
        out_fecha_y_hora->uso_horario = tm.tm_gmtoff;
        out_fecha_y_hora->horario_de_verano = tm.tm_isdst;
        return PDCRT_IO_OK;
    }
    else
    {
        return PDCRT_IO_ERROR_DESCONOCIDO;
    }
}

static pdcrt_io_error pdcrt_vio_linux_fecha_y_hora_gregoriana_utc(
    void *ctx,
    pdcrt_reloj *reloj,
    pdcrt_instante instante,
    pdcrt_fecha_y_hora_gregoriana *out_fecha_y_hora)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!reloj, "reloj inválido");
    PDCRT_BUG(!out_fecha_y_hora, "out_fecha_y_hora inválido");

    time_t t = instante.segundos;
    struct tm tm = {0};
    tzset(); // ¿Esto es necesario?
    if(gmtime_r(&t, &tm) == &tm)
    {
        out_fecha_y_hora->segundo = tm.tm_sec;
        out_fecha_y_hora->nanosegundo = instante.nanosegundos;
        out_fecha_y_hora->minuto = tm.tm_min;
        out_fecha_y_hora->hora = tm.tm_hour;
        out_fecha_y_hora->mes = tm.tm_mon + 1;
        out_fecha_y_hora->dia_del_mes = tm.tm_mday; // tf??
        out_fecha_y_hora->dia_de_la_semana = tm.tm_wday + 1;
        out_fecha_y_hora->dia_del_anno = tm.tm_yday + 1;
        out_fecha_y_hora->uso_horario = tm.tm_gmtoff;
        out_fecha_y_hora->horario_de_verano = tm.tm_isdst;
        return PDCRT_IO_OK;
    }
    else
    {
        return PDCRT_IO_ERROR_DESCONOCIDO;
    }
}

static pdcrt_io_error pdcrt_vio_linux_obtener_variable_de_entorno(
    void *ctx,
    pdcrt_vio_cadena variable,
    pdcrt_vio_buffer *out_valor,
    size_t *tam_valor)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!tam_valor, "tam_valor inválido");

    if(!pdcrt_vio_es_cstr(variable))
        return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;

    // TODO: Debería tener mi propia copia de environ y no usar getenv
    char *env = getenv(variable.ptr);
    if(env == NULL)
        return PDCRT_IO_ERROR_NO_EXISTE;

    *tam_valor = strlen(env);
    if(out_valor)
    {
        size_t nbytes = out_valor->cap >= *tam_valor ? *tam_valor : out_valor->cap;
        memcpy(out_valor->ptr, env, nbytes);
        out_valor->tam = nbytes;
    }
    return PDCRT_IO_OK;
}

static pdcrt_io_error pdcrt_vio_linux_crear_subproceso(
    void *ctx,
    pdcrt_subproceso **out_subproceso,
    pdcrt_opciones_crear_subproceso *opciones)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!out_subproceso, "out_subproceso inválido");
    PDCRT_BUG(!opciones, "opciones inválidas");

    if(opciones->tipo_linea_de_comandos == PDCRT_LINEA_DE_COMANDO_WINDOWS)
    {
        return PDCRT_IO_ERROR_OPERACION_NO_SOPORTADA;
    }
    else
    {
        PDCRT_BUG(opciones->tipo_linea_de_comandos != PDCRT_LINEA_DE_COMANDO_UNIX,
                  "la línea de comandos debe ser tipo UNIX");
        PDCRT_BUG(!pdcrt_vio_es_cstr(opciones->ejecutable), "ejecutable debe ser un cstr");

        size_t args_y_env_sz = 0;
        for(size_t i = 0; i < opciones->linea_de_comandos.como_unix.argc; i++)
        {
            args_y_env_sz += opciones->linea_de_comandos.como_unix.argv[i].tam + 1;
        }
        for(size_t i = 0; i < opciones->tam_entorno; i++)
        {
            args_y_env_sz += opciones->entorno[i].nombre.tam + 1 + opciones->entorno[i].valor.tam + 1;
        }

        char **argv = malloc(sizeof(char *) * (opciones->linea_de_comandos.como_unix.argc + 1));
        if(!argv)
            return PDCRT_IO_ERROR_SIN_MEMORIA;

        char **envp = malloc(sizeof(char *) * (opciones->tam_entorno + 1));
        if(!envp)
        {
            free(argv);
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        }

        char *args_y_env = malloc(args_y_env_sz);
        if(!args_y_env)
        {
            free(envp);
            free(argv);
            return PDCRT_IO_ERROR_SIN_MEMORIA;
        }

        size_t cptr = 0;

        for(size_t i = 0; i < opciones->linea_de_comandos.como_unix.argc; i++)
        {
            PDCRT_BUG(cptr + opciones->linea_de_comandos.como_unix.argv[i].tam + 1 > args_y_env_sz,
                      "sin memoria");
            memcpy(args_y_env + cptr,
                   opciones->linea_de_comandos.como_unix.argv[i].ptr,
                   opciones->linea_de_comandos.como_unix.argv[i].tam);
            args_y_env[cptr + opciones->linea_de_comandos.como_unix.argv[i].tam] = 0;
            argv[i] = args_y_env + cptr;
            cptr += opciones->linea_de_comandos.como_unix.argv[i].tam + 1;
        }
        argv[opciones->linea_de_comandos.como_unix.argc] = NULL;

        for(size_t i = 0; i < opciones->tam_entorno; i++)
        {

            PDCRT_BUG(cptr + opciones->entorno[i].nombre.tam + opciones->entorno[i].valor.tam + 2 > args_y_env_sz,
                      "sin memoria");
            memcpy(args_y_env + cptr,
                   opciones->entorno[i].nombre.ptr,
                   opciones->entorno[i].nombre.tam);
            args_y_env[cptr] = '=';
            memcpy(args_y_env + cptr + 1 + opciones->entorno[i].nombre.tam,
                   opciones->entorno[i].valor.ptr,
                   opciones->entorno[i].valor.tam);
            args_y_env[cptr + opciones->entorno[i].nombre.tam + opciones->entorno[i].valor.tam + 1] = 0;
            envp[i] = args_y_env + cptr;
            cptr += opciones->entorno[i].nombre.tam + opciones->entorno[i].valor.tam + 1;
        }
        envp[opciones->tam_entorno] = NULL;

        pid_t cpid = fork();
        if(cpid == 0)
        {
            execve(opciones->ejecutable.ptr, argv, envp);
            perror("could not execve");
            abort();
        }
        else if(cpid > 0)
        {
            free(argv);
            free(envp);
            free(args_y_env);

            *out_subproceso = malloc(sizeof(pdcrt_subproceso));
            **out_subproceso = (pdcrt_subproceso) {
                .vivo = true,
                .pid = cpid,
                .capturo_el_estado_de_salida = false,
                .estado_de_salida = 0,
                .termino_por_otro_motivo = false,
            };
            return PDCRT_IO_OK;
        }
        else
        {
            free(argv);
            free(envp);
            free(args_y_env);

            switch(errno)
            {
            case EAGAIN:
            case ENOMEM:
                return PDCRT_IO_ERROR_SIN_MEMORIA;
            default:
                return PDCRT_IO_ERROR_DESCONOCIDO;
            }
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_esperar_por_subproceso(void *ctx, pdcrt_subproceso *subproceso)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!subproceso, "subproceso inválido");

    if(!subproceso->vivo)
        return PDCRT_IO_ERROR_NO_EXISTE;

    int wstatus;

restart:
    wstatus = 0;
    pid_t res = waitpid(subproceso->pid, &wstatus, 0);

    if(res >= 0)
    {
        if(WIFEXITED(wstatus))
        {
            subproceso->vivo = false;
            subproceso->capturo_el_estado_de_salida = true;
            subproceso->estado_de_salida = WEXITSTATUS(wstatus);
            subproceso->termino_por_otro_motivo = false;
        }
        else if(WIFSIGNALED(wstatus))
        {
            subproceso->vivo = false;
            subproceso->capturo_el_estado_de_salida = false;
            subproceso->estado_de_salida = false;
            subproceso->termino_por_otro_motivo = true;
        }
        else
        {
            goto restart;
        }

        return PDCRT_IO_OK;
    }
    else
    {
        switch(errno)
        {
        case ECHILD:
            subproceso->vivo = false;
            subproceso->capturo_el_estado_de_salida = false;
            subproceso->estado_de_salida = 0;
            subproceso->termino_por_otro_motivo = false;
            return PDCRT_IO_ERROR_NO_EXISTE;
        case EINTR:
            goto restart;
        case EINVAL:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_matar_subproceso(void *ctx, pdcrt_subproceso *subproceso, bool forzar)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!subproceso, "subproceso inválido");

    if(!subproceso->vivo)
        return PDCRT_IO_ERROR_NO_EXISTE;

    if(kill(subproceso->pid, forzar ? SIGKILL : SIGTERM) == 0)
    {
        return PDCRT_IO_OK;
    }
    else
    {
        switch(errno)
        {
        case EINVAL:
            return PDCRT_IO_ERROR_ARGUMENTO_INVALIDO;
        case EPERM:
            return PDCRT_IO_ERROR_SIN_PERMISO;
        default:
            return PDCRT_IO_ERROR_DESCONOCIDO;
        }
    }
}

static pdcrt_io_error pdcrt_vio_linux_estado_de_subproceso(void *ctx,
    pdcrt_subproceso *subproceso,
    pdcrt_estado_de_subproceso *out_estado)
{
    PDCRT_BUG(ctx != &pdcrt_linux_marca, "ctx inválido");
    PDCRT_BUG(!subproceso, "subproceso inválido");
    PDCRT_BUG(!out_estado, "out_estado inválido");

    if(subproceso->vivo)
        return PDCRT_IO_ERROR_YA_EXISTE;

    *out_estado = (pdcrt_estado_de_subproceso) {0};

    if(subproceso->capturo_el_estado_de_salida)
    {
        out_estado->tipo = PDCRT_ESTADO_OK;
        out_estado->resultado = subproceso->estado_de_salida;
        return PDCRT_IO_OK;
    }
    else if(subproceso->termino_por_otro_motivo)
    {
        out_estado->tipo = PDCRT_ESTADO_OTRA_SALIDA;
        out_estado->resultado = 0;
        return PDCRT_IO_OK;
    }
    else
    {
        return PDCRT_IO_ERROR_DESCONOCIDO;
    }
}
