# El Lenguaje de Programación PseudoD #

![Logo de PseudoD](./docs/logo-128.png)

El nuevo(tm) compilador de PseudoD. No lo confundas con el [anterior][pdc-v1].

[pdc-v1]: https://github.com/alinarezrangel/pseudod-v3

Está sin terminar. Actualmente solo funciona en Linux sobre amd64 / x86-64.

## Ejemplo ##

```pseudod
[ hola-mundo.pd ]
utilizar bepd/builtins

Escribir: «hola, mundo»
```

```shell
pdc ejemplo/hola:hola-mundo.pd -k bepd:bepd -o hola-mundo
./hola-mundo
```

## Compilar e instalar ##

Necesitas descargar e instalar una versión de arranque desde
[alinarezrangel/pdc-rebootstrap][reboot]. Asegúrate de instalar el compilador
`boot-t1`. Esto instalará un archivo `toolchain.txt` en tu sistema. Reemplaza
`/ruta/al/archivo/toolchain.txt` por la ruta a dicho archivo.

Puedes reemplazar `/usr/local` por el lugar en el que quieres instalar
este proyecto.

[reboot]: https://github.com/alinarezrangel/pdc-rebootstrap

```shell
mkdir -p outputs/build
cd outputs/build
cmake -S ../.. \
      -B . \
      -DPSEUDOD_TOOLCHAIN=/ruta/al/archivo/toolchain.txt \
      --install-prefix /usr/local \
      -G Ninja
# Compilar:
cmake --build . -t default_target
# Instalar:
cmake --install .
```

Puedes leer [`docs/cmake.md`](docs/cmake.md) para más información sobre las opciones de CMake.

## Documentación ##

- [Opciones de CMake](docs/cmake.md).
- [Arquitectura](docs/arquitectura.md).
- [Modelo de compilación](docs/modelo-de-compilación.md).
- [Sistema de módulos](docs/módulos.md).
- [Manual del ejecutable `pdc`](docs/cli.md).
