# Módulos, colecciones y paquetes #

La unidad de compilación de PseudoD es el *módulo*, que corresponde directamente con un archivo de código fuente. Por
ejemplo, el siguiente archivo es un módulo:

```pseudod
utilizar bepd/builtins

Escribir: {Hola Mundo}
```

Todos los módulos tienen un nombre, el cual consiste de 2 partes: la *colección* y el *submódulo*. En el ejemplo
anterior `bepd/builtins`, `bepd` es la colección mientras que `builtins` es el submódulo. El submódulo puede contener
más separadores: `bepd/x/utf8` -> colección `bepd`, submódulo `x/utf8`.

Esta separación es importante, ya que la colección es personalizable por proyecto mientras que los submódulos no.
Imaginemos una biblioteca con la siguiente estructura:

```
libxml/
    inicio.pd
    dom.pd
    sax.pd
    contrib/
        dtd.pd
        schema.pd
        normalización.pd
```

Dentro de la biblioteca, la colección sería `libxml`. Por ejemplo, si quisiéramos importar `dom.pd` desde
`contrib/dtd.pd` escribiríamos: `utilizar libxml/dom como Dom`.

En la mayoría de los lenguajes de programación, el "nombre interno" de la biblioteca también debe corresponder a su
"nombre externo". En C, si una cabecera incluye `<libxml/dom.h>` entonces la biblioteca debe ser instalada como
`libxml` o la cabecera no podrá encontrarla. PseudoD tiene un sistema de módulos descentralizado, similar al de Rust.
En este sistema, la biblioteca puede ser instalada bajo cualquier nombre que el usuario desee. Por ejemplo, podríamos
instalar `libxml` como `la-biblioteca-xml`, en cuyo caso nuestro código sería `utilizar la-biblioteca-xml/dom.pd`.
**Esto no afecta como el código de la biblioteca está escrito**. Dentro de `libxml`, `dom.pd` siempre sería importado
como `utilizar libxml/dom`.

Esto significa que al usar una biblioteca es necesario especificar tanto el nombre de la biblioteca como el nombre que
la colección tendrá en el proyecto. Esto se especifíca en la configuración de tu sistema de construcción (como CMake).
