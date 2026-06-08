# CMake #

Actualmente, este proyecto utiliza el sistema de construcción [cmake](https://cmake.org) para compilar los programas
y bibliotecas.

## `PSEUDOD_TOOLCHAIN` ##

Esta variable de cmake contiene la ruta al toolchain de PseudoD a usar para compilar el proyecto. Necesitas
instalar una versión de arranque desde [alinarezrangel/pdc-rebootstrap][reboot] (para saber qué versión de
arranque debes instalar, revisa el archivo [DIST_BOOTSTRAP](../DIST_BOOTSTRAP)). Al instalar la
versión de arranque, se instalará también un archivo `toolchain.txt`. La ruta a este archivo es la que debes
usar en `PSEUDOD_TOOLCHAIN`.

[reboot]: https://github.com/alinarezrangel/pdc-rebootstrap

Por ejemplo, `-DPSEUDOD_TOOLCHAIN=/usr/local/share/bt1-pdc/toolchain.txt` compilará con el toolchain
`/usr/local/share/bt1-pdc/toolchain.txt`.

## Variables útiles ##

Puedes especificar las siguientes variables al ejecutar cmake (`cmake -DFOO=BAR ...`) para configurar distíntos
aspectos de la compilación.

- `TOOL_PREFIX`: Prefijo a agregar a los programas y bibliotecas del proyecto cuando se instálen. Por ejemplo, si
    tiene como valor `mi-`, entonces el programa `pdc` será instalado como `mi-pdc`, `libpdcrt.a` será
    `libmi-pdcrt.a`, etc.
- `PSEUDOD_TOOLCHAIN`: Toolchain a usar.
- `LUA_EXECUTABLE`: Ejecutable de Lua 5.4 a usar.
- `PSEUDOD_CURRENT_BINARY_DIR`: Ruta al directorio donde se guardarán las salidas de compilación de PseudoD.
    Véase la sección "Configuraciones".
- `PSEUDOD_CFLAGS`: Banderas para el compilador de C a usar cuando se compile un archivo generado por `pdc`.
- `PSEUDOD_LDFLAGS`: Banderas para el compilador de C a usar cuando se enlace un programa o biblioteca generada por
    `pdc`.
- `PSEUDOD_LIBS`: Bibliotecas a enlazar con el compilador de C.
- `PDCRT_ASAN`: Si se debería usar [*ASan*](https://github.com/google/sanitizers/wiki/AddressSanitizer).
- `PDCRT_UBSAN`: Si se debería usar [*UBSan*](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html).
- `PDCRT_DBG`: Si el runtime se debería compilar en modo de depuración. Esto habilita ciertas validaciones internas.
- `PDCRT_DBG_GC`: Si el recolector de basura debería ser compilado en modo de depuración.
- `PDCRT_DBG_NO_K`: Desactiva el recolector *Cheney on the M.T.A.* y utiliza trampolines para implementar C.P.S.
    Principalmente usado para obtener perfiles de rendimiento en ciertas circunstancias.
- `PDCRT_EMP_INTR`: Agrega dos funciones al runtime que interceptan las interacciones con la pila dinámica. Esto es
    útil si necesitas poner *breakpoints* en el runtime para saber que valores son empujados / sacados de la pila y
    en qué momento.
- `PDCRT_DTRACE`: Compila la integración con DTrace / SystemTap / USDT.

## Configuraciones ##

Si decides tener múltiples configuraciones de cmake al mismo tiempo (por ejemplo, estás trabajando en tu propia copia
de pdc y quieres tener tanto la versión con los *sanitizers* como la versión sin ellos) verás que cada configuración
tiene su propia copia de los archivos compilados por `pdc`. El proceso de compilación de PseudoD es bastánte hermético
por lo que en teoría es posible compartir las salidas de `pdc` entre todas las configuraciones.

Para realizar esto, puedes fijar `PSEUDOD_CURRENT_BINARY_DIR` al ejecutar cmake a un directorio *fuera del directorio
de construcción*. Cuando múltiples configuraciones tienen un mismo `PSEUDOD_CURRENT_BINARY_DIR` las salidas de `pdc`
serán compartidas entre ellos.

Por ejemplo:

```shell
mkdir cmake-build-san cmake-build-nosan cmake-build-pdbin
PDBIN="$(pwd)/cmake-build-pdbin"
cd cmake-build-san
cmake -S .. -B . -DPSEUDOD_CURRENT_BINARY_DIR="$PDBIN" -DPDCRT_ASAN=ON -DPDCRT_UBSAN=ON
cmake --build . -t default_target  # (1)
cd ../cmake-build-nosan
cmake -S .. -B . -DPSEUDOD_CURRENT_BINARY_DIR="$PDBIN" -DPDCRT_ASAN=OFF -DPDCRT_UBSAN=OFF
cmake --build . -t default_target  # (2)
```

Las configuraciones compiladas en #1 y #2 compartirán los archivos C generados por `pdc`, pero tendrán sus propias
banderas y opciones de compilación de C.

## `PseudoD2.cmake` ##

Este archivo es una pequeña biblioteca con funciones y macros que te permiten compilar programas en PseudoD usando
cmake. Su uso general es:

```cmake
# 1. Define las colecciones de módulos:
pseudod_collection(col_bepd
        PACKAGE bepd
        SOURCES
        bepd/algoritmos.pd
        bepd/builtins.pd
        bepd/builtinsImpl.pd
        # ...
)

pseudod_collection(col_pdc
        PACKAGE pdc
        SOURCES
        pdc/abstraer.pd
        pdc/ast.pd
        # ...
)

# 2. Compila las colecciones a C
add_pseudod_compile(
        pdc_inner_pdc
        COLLECTIONS col_bepd col_pdc
)

# 3. Genera un ejecutable
add_pseudod_executable(pdc
        COLLECTIONS col_bepd col_pdc
        ENTRY pdc/inicio
        DEPENDS pdc_inner_pdc
)
```
