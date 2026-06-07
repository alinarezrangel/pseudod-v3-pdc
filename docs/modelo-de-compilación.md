# Modelo de compilación de PseudoD #

El modelo de compilación de PseudoD es diferente al de otros lenguajes populares como C. En C / C++, el compilador
es invocado una vez por cada *unidad de compilación* (básicamente cada `.c` / `.cxx` / `.cpp`). Cada una de estas
compilaciones lee más archivos de cabecera (`.h` / `.hpp` / `.hxx`). Quizás lo más importante: las banderas y opciones
de compilación afectan la compilación, ABI, API, etc. no solo del código siendo compilado (`.c`) sino también de
las cabeceras (`.h`): esto es debido a que todos los archivos forman parte de una misma unidad de compilación.

En PseudoD, el modelo de compilación es mucho más hermético: los archivos PseudoD (`.pd`) tienen dependencias en otros
módulos, pero dichos archivos no son abiertos automáticamente por el compilador: cada dependencia debe ser especificada
en la línea de comandos explícitamente, y si no lo es el compilador fallará con un error indicando que el módulo no
exíste (¡incluso si el archivo si exíste!). Estas dependencias directas corresponden a archivos `.ipd` (*interfaz
pseudod*).  Al compilar un archivo, no solo generas un archivo `.c` / `.o`, sino también un archivo `.ipd` para el
módulo compilado.

Los archivos `.ipd` corresponden, más o menos, con las cabeceras de C o los *signatures* de SML. Cada `.ipd` tiene
información de la API y la ABI del módulo correspondiente. Críticamente: al compilar un `.pd` las opciones de
compilación no afectan la API ni la ABI de los `.ipd`s de los que depende; en C, las opciones del compilador
afectan a las cabeceras directas e indirectas, en PseudoD, las opciones solo afectan al módulo directo, no a sus
dependencias.

## Un ejemplo ##

```c++
// foo.c
#include "frob.h"
```

```c++
// frob.h
#include "bar.h"
```

```c++
// bar.h
#ifdef FOO
void foo(void);
#else
void foo(int);
#endif
```

Imaginemos que estamos compilando `foo.c`:

```shell
gcc -c foo.c -o foo1.o
gcc -c -DFOO foo.c -o foo2.o
```

Nota como las opciones que pasamos al compilar `foo.c` afectan la API y ABI de una dependencia indirecta: `bar.h`.

En cambio, en PseudoD:

```pseudod
[ foo.pd ]
utilizar mi-programa/frob como Frob
```

```pseudod
[ frob.pd ]
utilizar mi-programa/bar como Bar
```

```pseudod
[ bar.pd ]
funcion Foo
    devolver 1
finfuncion
```

Al compilar:

```shell
pdc -c mi-programa/bar:bar.pd -q mi-programa/bar:bar.ipd -o bar.o
pdc -c mi-programa/frob:frob.pd mi-programa/bar:bar.ipd -q mi-programa/frob:frob.ipd -o frob.o
pdc -c mi-programa/foo:foo.pd mi-programa/frob:frob.ipd -q mi-programa/foo:foo.ipd -o foo.o
```

Nota las diferencias:

1. En PseudoD, cada dependencia directa debe ser especificada en la línea de comandos: el compilador no abre archivos
automáticamente. Por ejemplo, al compilar `frob.pd` debemos especificar `bar.ipd`.
2. El nombre de los archivos y el nombre de los módulos no necesariamente deben coincidir. Al compilar `frob.pd`,
debemos especificar no solo `bar.ipd` (el archivo) sino también `mi-programa/bar` (el módulo). Esto se hace con la
sintáxis `mi-programa/bar:bar.ipd` (`$MÓDULO:$ARCHIVO`).
3. El compilador genera 2 archivos: el código (`-o $F.o`) y la interfaz (`-q $F.ipd`).

Cabe resaltar a qué me refiero cuando digo que los nombres de los módulos no necesariamente deben corresponder a los
nombres de los archivos: en este ejemplo, los módulos `mi-programa/$ETC` corresponden a los archivos `$ETC.pd`, pero
podríamos tener archivos con cualquier nombre:

```pseudod
[ ejemplo1.pd ]
utilizar mi-programa/frob como Frob
```

```pseudod
[ ejemplo2.pd ]
utilizar mi-programa/bar como Bar
```

```pseudod
[ ejemplo3.pd ]
funcion Foo
    devolver 1
finfuncion
```

```shell
pdc -c mi-programa/bar:ejemplo3.pd -q mi-programa/bar:ejemplo3.ipd -o ejemplo3.o
pdc -c mi-programa/frob:ejemplo2.pd mi-programa/bar:ejemplo3.ipd -q mi-programa/frob:ejemplo2.ipd -o ejemplo2.o
pdc -c mi-programa/foo:ejemplo1.pd mi-programa/frob:ejemplo2.ipd -q mi-programa/foo:ejemplo1.ipd -o ejemplo1.o
```

La opción `-q` acepta un módulo y un archivo: el módulo cuya interfaz será guardada y el archivo en el que será
guardado. Por ejemplo, en:

```shell
pdc -c mi-programa/frob:frob.pd mi-programa/bar:bar.ipd -q mi-programa/frob:frob.ipd -o frob.o
```

Guardará la interfaz del módulo `mi-programa/frob` en `frob.ipd`, mientras que:

```shell
pdc -c mi-programa/frob:frob.pd mi-programa/bar:bar.ipd -q mi-programa/bar:bar2.ipd -o frob.o
```

Guardará la interfáz de `mi-programa/bar` en `bar2.ipd`; ¡La interfáz a guardar no necesariamente debe ser la del
archivo siendo compilado! Puede ser también para una dependencia o cualquier otro módulo especificado en la línea
de comandos.
