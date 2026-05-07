//
// Created by alinarezrangel on 25/1/26.
//

#ifndef PDCRT_PDCRT_VIO_H
#define PDCRT_PDCRT_VIO_H

#include <stdbool.h>
#include <stdint.h>

#include "pdcrt-plataforma.h"
#include "pdcrt_base.h"


// Una cadena de carácteres. Su codificación dependerá de la
// implementación del VIO dada: un VIO para Windows podría usar
// WTF-8 (<https://wtf-8.codeberg.page/>) mientras que uno para
// UNIX podría tratar a la cadena como una secuencia de bytes
// arbitraria. Véase la estructura `pdcrt_vio_quirks`.
typedef struct pdcrt_vio_cadena
{
    const char *ptr;
    size_t tam;
} pdcrt_vio_cadena;

// Cadena mutable.
typedef struct pdcrt_vio_buffer
{
    char *ptr;
    size_t tam, cap;
} pdcrt_vio_buffer;

inline pdcrt_vio_cadena pdcrt_vio_buffer_como_cadena(pdcrt_vio_buffer buf)
{
    return (pdcrt_vio_cadena) { .ptr = buf.ptr, .tam = buf.tam };
}

typedef struct pdcrt_vio_quirks
{
    bool rutas_wtf8 : 1;
    bool linea_de_comandos_windows : 1;
    uint64_t reservado1 : 62;
    uint64_t reservado2;
} pdcrt_vio_quirks;

/*********** Fechas y Horas ***********/

typedef struct pdcrt_instante
{
    // Nanosegundos desde el UNIX Epoch (1.º de enero de 1970, UTC)
    int64_t segundos;
    uint32_t nanosegundos;
} pdcrt_instante;

typedef struct pdcrt_fecha_y_hora_gregoriana
{
    uint8_t segundo; // Segundo [0, 60]
    uint8_t minuto; // Minuto [0-59]
    uint8_t hora; // Hora [0-23]
    uint8_t mes; // Mes [1-12]
    uint8_t dia_del_mes; // Día del mes [1-31]
    uint8_t dia_de_la_semana; // Día de la semana [1-7] (1: Domingo, 2: Lunes, ..., 7: Sábado)
    uint16_t dia_del_anno; // Día del año [1-366]
    uint32_t uso_horario; // En segundos al este de UTC
    uint32_t nanosegundo; // Nanosegundo [0, 999999999]
    int64_t anno; // Año
    bool horario_de_verano;
} pdcrt_fecha_y_hora_gregoriana;

typedef enum pdcrt_tipo_reloj
{
    PDCRT_RELOJ_MONOTONICO,
    PDCRT_RELOJ_REAL,
} pdcrt_tipo_reloj;

struct pdcrt_reloj;
typedef struct pdcrt_reloj pdcrt_reloj;

/*********** Archivos ***********/

struct pdcrt_archivo;
typedef struct pdcrt_archivo pdcrt_archivo;

struct pdcrt_directorio;
typedef struct pdcrt_directorio pdcrt_directorio;

typedef enum pdcrt_intencion_abrir_archivo
{
    PDCRT_ABRIR_LECTURA = 1 << 0,
    PDCRT_ABRIR_ESCRITURA = 1 << 1,
} pdcrt_intencion_abrir_archivo;

typedef enum pdcrt_accion_crear
{
    // Error si no existe
    PDCRT_ACCION_ERROR_NUEVO = 0x1,
    // Crear si no existe
    PDCRT_ACCION_CREAR_NUEVO = 0x2,
} pdcrt_accion_crear;

typedef enum pdcrt_accion_abrir_archivo
{
    // Error si el archivo existe
    PDCRT_ACCION_ERROR_EXISTENTE = 0x1,
    // Abrir existente
    PDCRT_ACCION_ABRIR_EXISTENTE = 0x2,
    // Truncar existente
    PDCRT_ACCION_TRUNCAR_EXISTENTE = 0x3,
} pdcrt_accion_abrir_archivo;

typedef enum pdcrt_abrir_archivo_flags
{
    // Escribe al final del archivo
    PDCRT_ABRIR_ARCHIVO_CONCATENAR = 1 << 0,
    // El archivo puede ser compartido con nuevos subprocesos
    PDCRT_ABRIR_ARCHIVO_HEREDABLE = 1 << 1,
} pdcrt_abrir_archivo_flags;

typedef enum pdcrt_permisos_archivo
{
    PDCRT_PERMISO_LEER = 1 << 0,
    PDCRT_PERMISO_ESCRIBIR = 1 << 1,
    PDCRT_PERMISO_EJECUTAR = 1 << 2,
} pdcrt_permisos_archivo;

typedef struct pdcrt_opciones_abrir_archivo
{
    pdcrt_directorio *relativo_a;
    pdcrt_vio_cadena nombre;

    pdcrt_intencion_abrir_archivo intencion;
    pdcrt_accion_crear accion_crear;
    pdcrt_permisos_archivo permisos_al_crear;
    pdcrt_accion_abrir_archivo accion_abrir;
    pdcrt_abrir_archivo_flags flags;
} pdcrt_opciones_abrir_archivo;

typedef enum pdcrt_intencion_abrir_directorio
{
    PDCRT_ABRIR_ITERAR = 1 << 0,
    PDCRT_ABRIR_REFERENCIAR = 1 << 1,
} pdcrt_intencion_abrir_directorio;

typedef enum pdcrt_abrir_directorio_flags
{
    PDCRT_ABRIR_DIRECTORIO_HEREDABLE = 1 << 0,
} pdcrt_abrir_directorio_flags;

typedef struct pdcrt_opciones_abrir_directorio
{
    pdcrt_directorio *relativo_a;
    pdcrt_vio_cadena nombre;

    pdcrt_intencion_abrir_directorio intencion;
    pdcrt_accion_crear accion_crear;
    pdcrt_abrir_directorio_flags flags;
} pdcrt_opciones_abrir_directorio;

typedef struct pdcrt_registro_directorio
{
    pdcrt_vio_buffer nombre;
    size_t tam_nombre;
} pdcrt_registro_directorio;

typedef enum pdcrt_archivo_ancla
{
    PDCRT_ARCHIVO_ANCLA_INICIO = 0,
    PDCRT_ARCHIVO_ANCLA_ACTUAL = 1,
    PDCRT_ARCHIVO_ANCLA_FIN = 2,
} pdcrt_archivo_ancla;

typedef struct pdcrt_opciones_crear_directorio_temporal
{
    pdcrt_directorio *relativo_a;
    pdcrt_vio_buffer plantilla;

    pdcrt_intencion_abrir_directorio intencion;
    pdcrt_abrir_directorio_flags flags;
} pdcrt_opciones_crear_directorio_temporal;

/*********** Subprocesos ***********/

struct pdcrt_subproceso;
typedef struct pdcrt_subproceso pdcrt_subproceso;

typedef size_t pdcrt_pid;

typedef struct pdcrt_variable_de_entorno
{
    pdcrt_vio_cadena nombre, valor;
} pdcrt_variable_de_entorno;

typedef enum pdcrt_linea_de_comandos
{
    PDCRT_LINEA_DE_COMANDO_UNIX = 0,
    PDCRT_LINEA_DE_COMANDO_WINDOWS = 1,
} pdcrt_linea_de_comandos;

typedef struct pdcrt_opciones_crear_subproceso
{
    pdcrt_vio_cadena ejecutable;

    pdcrt_linea_de_comandos tipo_linea_de_comandos;
    union
    {
        struct
        {
            pdcrt_vio_cadena *argv;
            size_t argc;
        } como_unix;
        pdcrt_vio_cadena como_windows;
    } linea_de_comandos;

    pdcrt_directorio *directorio_actual;

    pdcrt_variable_de_entorno *entorno;
    size_t tam_entorno;
} pdcrt_opciones_crear_subproceso;

typedef enum pdcrt_tipo_estado_de_subproceso
{
    PDCRT_ESTADO_OK = 0,
    PDCRT_ESTADO_OTRA_SALIDA = 1,
} pdcrt_tipo_estado_de_subproceso;

typedef struct pdcrt_estado_de_subproceso
{
    pdcrt_tipo_estado_de_subproceso tipo;
    int resultado;
} pdcrt_estado_de_subproceso;

/*********** VIO ***********/

typedef enum pdcrt_io_error
{
    // OK
    PDCRT_IO_OK,
    // Otro tipo de error que no está en este enum
    PDCRT_IO_ERROR_DESCONOCIDO,
    // No tienes permisos para la operación
    PDCRT_IO_ERROR_SIN_PERMISO,
    // No hay suficiente memoria para completar la operación
    PDCRT_IO_ERROR_SIN_MEMORIA,
    // Uno o más argumentos son inválidos
    PDCRT_IO_ERROR_ARGUMENTO_INVALIDO,
    // Uno o más de los argumentos se refieren a una entidad que no existe
    PDCRT_IO_ERROR_NO_EXISTE,
    // Uno o más de los argumentos se refieren a una entidad que ya existe
    PDCRT_IO_ERROR_YA_EXISTE,
    // El recurso o ruta especificados no es un archivo
    PDCRT_IO_ERROR_NO_ES_ARCHIVO,
    // El recurso o ruta especificados no es un directorio
    PDCRT_IO_ERROR_NO_ES_DIRECTORIO,
    // La operación no está soportada
    PDCRT_IO_ERROR_OPERACION_NO_SOPORTADA,
} pdcrt_io_error;

typedef struct pdcrt_vio_limites
{
    size_t ruta_maxima, archivo_maximo, nombre_de_archivo_maximo;
    size_t entorno_maximo, linea_de_comandos_maxima;
    bool entorno_y_linea_de_comandos_compartidos;
    size_t num_maximo_recursos;
} pdcrt_vio_limites;

typedef struct pdcrt_vio_vtable
{
    pdcrt_vio_limites limites;
    pdcrt_vio_quirks quirks;

    /* Archivos y directorios */

    pdcrt_io_error (*op_abrir_archivo)(
        void *ctx,
        pdcrt_archivo **out_archivo,
        pdcrt_opciones_abrir_archivo *opciones);

    pdcrt_io_error (*op_abrir_directorio)(
        void *ctx,
        pdcrt_directorio **out_directorio,
        pdcrt_opciones_abrir_directorio *opciones);

    pdcrt_io_error (*op_cerrar_archivo)(void *ctx, pdcrt_archivo *archivo);
    pdcrt_io_error (*op_cerrar_directorio)(void *ctx, pdcrt_directorio *directorio);

    pdcrt_io_error (*op_crear_directorio_temporal)(void *ctx,
        pdcrt_directorio **out_directorio,
        pdcrt_opciones_crear_directorio_temporal *opciones);

    pdcrt_io_error (*op_obtener_ruta_actual)(
        void *ctx,
        pdcrt_vio_buffer *out_ruta,
        bool *out_completo);
    pdcrt_io_error (*op_cambiar_ruta_actual)(
        void *ctx,
        pdcrt_vio_cadena ruta);
    pdcrt_io_error (*op_cambiar_ruta_actual_dir)(void *ctx, pdcrt_directorio *dir);

    pdcrt_io_error (*op_iterar_directorio)(
        void *ctx,
        pdcrt_directorio *dir,
        bool *eof,
        pdcrt_registro_directorio *out_registro);

    pdcrt_io_error (*op_leer)(
        void *ctx,
        pdcrt_archivo *archivo,
        pdcrt_vio_buffer *out_buffers,
        size_t num_buffers);
    pdcrt_io_error (*op_escribir)(
        void *ctx,
        pdcrt_archivo *archivo,
        pdcrt_vio_cadena *cadenas,
        size_t num_cadenas,
        size_t *out_escrito);
    pdcrt_io_error (*op_posicionar)(
        void *ctx,
        pdcrt_archivo *archivo,
        pdcrt_archivo_ancla ancla,
        ssize_t offset,
        size_t *resultado);

    pdcrt_io_error (*op_borrar_archivo_por_ruta)(
        void *ctx,
        pdcrt_directorio *relativo_a,
        pdcrt_vio_cadena ruta);
    pdcrt_io_error (*op_borrar_directorio_por_ruta)(
        void *ctx,
        pdcrt_directorio *relativo_a,
        pdcrt_vio_cadena ruta);

    pdcrt_io_error (*op_renombrar_archivo)(
        void *ctx,
        pdcrt_directorio *primero_relativo_a,
        pdcrt_vio_cadena primera_ruta,
        pdcrt_directorio *segundo_relativo_a,
        pdcrt_vio_cadena segunda_ruta);

    /* Relojes, fechas y horas */

    pdcrt_io_error (*op_abrir_reloj)(
        void *ctx,
        pdcrt_reloj **out_reloj,
        pdcrt_tipo_reloj tipo);

    pdcrt_io_error (*op_cerrar_reloj)(void *ctx, pdcrt_reloj *reloj);

    pdcrt_io_error (*op_instante_actual)(
        void *ctx,
        pdcrt_reloj *reloj,
        pdcrt_instante *out_instante);

    pdcrt_io_error (*op_fecha_y_hora_gregoriana_local)(
        void *ctx,
        pdcrt_reloj *reloj,
        pdcrt_instante instante,
        pdcrt_fecha_y_hora_gregoriana *out_fecha_y_hora);

    pdcrt_io_error (*op_fecha_y_hora_gregoriana_utc)(
        void *ctx,
        pdcrt_reloj *reloj,
        pdcrt_instante instante,
        pdcrt_fecha_y_hora_gregoriana *out_fecha_y_hora);

    /* Procesos y subprocesos */

    pdcrt_io_error (*op_obtener_variable_de_entorno)(
        void *ctx,
        pdcrt_vio_cadena variable,
        pdcrt_vio_buffer *out_valor,
        size_t *tam_valor);

    pdcrt_io_error (*op_crear_subproceso)(
        void *ctx,
        pdcrt_subproceso **out_subproceso,
        pdcrt_opciones_crear_subproceso *opciones);

    pdcrt_io_error (*op_esperar_por_subproceso)(void *ctx, pdcrt_subproceso *subproceso);
    pdcrt_io_error (*op_matar_subproceso)(void *ctx, pdcrt_subproceso *subproceso, bool forzar);
    pdcrt_io_error (*op_estado_de_subproceso)(void *ctx,
        pdcrt_subproceso *subproceso,
        pdcrt_estado_de_subproceso *out_estado);
} pdcrt_vio_vtable;

typedef struct pdcrt_vio
{
    const pdcrt_vio_vtable *vtable;
    void *ctx;
} pdcrt_vio;

#endif //PDCRT_PDCRT_VIO_H
