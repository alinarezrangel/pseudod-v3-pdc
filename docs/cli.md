# `pdc` -- CLI #

## NOMBRE ##

pdc -- El compilador de PseudoD y C.

## USO ##

    pdc [opciones...] ./archivos...
    pdc [opciones...] ../archivos...
    pdc [opciones...] /archivos...
    pdc [opciones...] módulos...
    pdc -Y paquete [opciones...] módulos...

Donde algunas opciones comúnes son:

    pdc [-b|-c|-S] [-shared] [-g] [-O|-Og] [-o archivo] [@archivo...] [opciones...]

## OPCIONES ##

Todas las opciones del compilador de la forma `-f$OPT` tienen una versión
invertida `-fno-$OPT`. Este manual siempre documenta primero la versión que
**no** sea predeterminada.

Las *entradas* son los archivos (de la forma `./$archivo` o `../$archivo` o
`/$archivo`) y los módulos (de la forma `$paquete/$módulo:$archivo`) que se
pasan como argumentos posicionales al programa. Puedes especificar varias
entradas de ambos tipos. Al generar ejecutables, se asumirá que la primera
entrada es el archivo principal del programa. Los módulos serán visibles
para los demás archivos por su nombre dado (antes del `:`) pero los archivos
no podrán ser importados desde otros archivos o módulos. Esto significa
que en general solo es útil usar archivos al inicio de la línea de comandos,
donde se procesarán como el archivo principal.

Por ejemplo:

    pdc ./foo.pd -o foo  # Compila foo.pd como un programa
    pdc ./foo.pd prog/bar:bar.pd -o foo  # Compila foo.pd con acceso a bar.pd mediante el nombre `prog/bar`
    pdc prog/bar:bar.pd ./foo -o bar # Compila bar.pd como un programa. foo.pd será compilado y enlazado, pero no se podrá importar desde bar.pd

Nota como la sintáxis de los archivos no te permiten pasar un archivo sin
que la ruta comience con `/`, `./` o `../`. Por ejemplo: `pdc foo.pd -o foo`
es un error.

### GENERAL ###

- `--version`: Escribe la versión y termina.
- `--help` / `-h` / `-?` / `--ayuda`: Muestra un mensaje de ayuda y termina.
- `--version`: Muestra la versión y nombre del programa.
- `--solo-version`: Muestra solo la versión del programa.
- `-v`: Muestra todos los comandos ejecutados.
- `-vv`: Muestra aún más información.
- `-vvv`: Muestra información de depuración.
- `@$archivo`: Lee las opciones desde el *archivo* dado. Las opciones deben
  estar separadas por espacios (ASCII 32 SPACE, ASCII 9 HT, ASCII 10 LF, ASCII
  13 CR). Las opciones también pueden incluir comillas (`"` y `'`) dentro de
  las cuales todos los carácteres excepto otras comillas del mismo tipo serán
  tomados literalmente. Por ejemplo: `foo bar baz` son `foo`, `bar` y `baz`.
  `"foo" 'bar baz'` es `foo` y `bar baz`. `"foo 'bar baz'"` es
  `foo 'bar baz'`. `foo" "bar baz` es `foo bar` y `baz`. `"foo"'"'"bar"`es
  `foo"bar`.
- `-x $lenguaje`: Controla el lenguaje que se asumirá de las siguientes entradas.
  Esta opción afecta como se procesan las siguientes entradas hasta que se
  encuentre otro `-x`.
- `-c`: Compila los archivos a un archivo objeto ("no enlaces el programa").
- `-b`: Compila los archivos a un *objeto PseudoD* en vez de un ejecutable. No
  enlaza el programa.
- `-S`: Compila los archivos a lenguaje ensamblador.
- `-Sc`: Compila los archivos a C.
- `-shared`: Genera un objeto de enlace dinámico. Implica `-fPIC`.
- `-no-pie` / `-pie`: Genera un *position-independent executable*. Implica `-fPIE`.
- `-I$ruta` / `-I $ruta`, `-iquote $ruta`, `-isystem $ruta`, `-idirafter $dir`:
  Agrega la *ruta* a la lista de rutas en las que buscar cabeceras de C.

    `-iquote $ruta` solo funciona para cabeceras de C que se especifiquen con
    comillas (`#include "foo"`).

    El órden en el que se buscarán las cabeceras de C es:

    1. `-iquote`
    2. `-I`
    3. `-isystem`
    4. Las rutas estándares especificadas al compilar **pdc**.
    5. `-idirafter`
- `-L$ruta` / `-L $ruta`: Agrega la *ruta* a la lista de rutas en las que
  buscar bibliotecas compiladas.
- `-l$biblioteca` / `-l $biblioteca`: Enlaza con la *biblioteca* dada.
- `-l:$archivo` / `-l :$archivo`: Enlaza con la biblioteca en el *archivo*
  dado. Si el archivo no contiene `/`, será buscado en las rutas de
  bibliotecas.
- `-K$ruta` / `-K $ruta`: Agrega una *ruta* a las rutas en las que buscar
  bibliotecas de PseudoD.
- `-k$nombre:$biblioteca` / `-k $nombre:$biblioteca`: Compila usando la
  interfáz de la *biblioteca* dada. Esta será accesible para el programa bajo
  el *nombre* especificado. Si *biblioteca* contiene algún `/`, es tratada como
  un archivo. Si no, es un nombre a buscar en las rutas especificadas con `-K`.
- `-o$archivo` / `-o $archivo`: Guarda el resultado de la compilación en el
  *archivo* dado.
- `-B$prefijo` / `-B $prefijo`: Busca los programas invocados por el compilador
  en el *prefijo* dado.
- `-q $modulo:$archivo`: Escribe la interfáz de PseudoD del *módulo* en el
  *archivo* dado.

### BIBLIOTECAS PSEUDOD ###

- `-Y$paquete` / `-Y $paquete`: Compila los archivos a una *biblioteca PseudoD* en vez de un
  ejecutable. Enlaza únicamente con otras bibliotecas PseudoD. La biblioteca
  contendrá todos los módulos públicos del paquete especificado.
- `-Zco $archivo` / `-Zcode-output $archivo`: Cuando se especifica `-Y`, `-o` siempre
  contendrá la *biblioteca PseudoD* generada. Sin embargo, si también se
  especifica `-Zco $archivo`, *archivo* contendrá el "código" mientras que la
  biblioteca solo contendrá la "interfaz". De esta forma es posible distribuir
  los archivos por separado, dando el código a todos los usuarios pero la
  interfaz solo a desarrolladores.

  Es particularmente útil al generar bibliotecas dinámicas:

        pdc -shared -Y biblioteca biblioteca/módulo:./archivo.pd -o lib.bpd -Zco lib.so
        # Puedes distribuir `lib.so` y `lib.bpd` por separado.

Cuando se especifica `-Y`, las opciones `-c`, `-b`, `-S` y `-shared` solo
afectan a la salida con `-Zco`, no a la salida con `-o`.

### COMPILADOR ###

- `-g`: Genera información de depuración.
- `-ggdb`: Genera información de depuración para GDB.
- `-fno-pic` / `-fno-PIC`: No genera *position-independent code*.
- `-fpie` / `-fPIE`: Genera código para un *position-independent executable*. Implica `-fPIC`.
- `-Zid $identificador`: Especifica el identificador del módulo para la siguiente entrada.
  La entrada debe ser un archivo PseudoD o un programa en C. Este identificador debe tener
  la forma `[a-zA-Z_][a-zA-Z0-9_]*` y será usado para representar al programa al nivel
  de la ABI.
- `-fno-builtin`: Deshabilita los builtins para los módulos.
- `-fno-builtin-$builtin`: Deshabilita únicamente el *builtin* especificado para los módulos.
- `-fno-main`: No genera una función `main` para el programa. La primera entrada será la que
  contendrá la función `main`.
- `-fsanitize=undefined`: Activa *UBSan*.
- `-fsanitize=address`: Activa *ASan*.

### ENLAZADOR ###

- `-Wl,$opciones`: Pasa las *opciones* (separadas por comas) al enlazador
  (*linker*).
- `-Xlinker $opción`: Pasa la *opción* dada al enlazador.
- `-T $archivo`: Usa el *archivo* dado como un *guión de enlace* (*linker
  script*).

### COMPILADOR DE C ###

- `-Wp,$opciones`: Pasa las *opciones* (separadas por comas) al preprocesador
  de C.
- `-Xpreprocessor $opción`: Pasa la *opción* dada al preprocesador de C.
- `-Wa,$opciones`: Pasa las *opciones* (separadas por comas) al ensamblador.
- `-Xassembler $opción`: Pasa la *opción* dada al ensamblador.
- `-Wc,$opciones`: Pasa las *opciones* (separadas por comas) al compilador de C.
- `-Xcdriver $opción`: Pasa la *opción* dada al compilador de C.

### OPTIMIZACIONES ###

- `-O`: Optimiza el programa (equivalente a `-O1`).
- `-O0`: No optimiza el programa.
- `-O1` / `-O2` / `-O3`: Optimiza el programa al nivel dado.
- `-Og`: Optimiza el programa para ser depurado.
- `-Os` / `-Oz`: Optimiza el tamaño del programa en vez de su velocidad.
- `-fwhole-program`: Optimiza todo el programa a la vez.

### PREPROCESADOR ###

- `-D$macro` / `-D$macro=$valor`: Define la macro de C con el valor
  especificado.
- `-U$macro`: Indefine la macro de C como si se usara `#undef`.
- `-include $archivo`: Incluye el *archivo* los programas en C.

## SALIDA ##

- 0: El programa compiló sin errores.
- 1: El programa tiene uno o más errores.
- 2: El programa compiló sin errores pero con una o más advertencias (solo si
  la opción `-Wproc` fue especificada).

## ENTORNO ##

- `LANG`, `LC_CTYPE`, `LC_MESSAGES`, `LC_ALL`: Afectan los mensajes del
  compilador.
- `TMPDIR`: Directorio donde se almacenarán los archivos temporales, de ser
  necesarios.
- `CPATH`, `C_INCLUDE_PATH`: Lista de rutas separadas con `:` (UNIX) o `;` (MS
  Windows) que serán agregadas a las rutas en las que buscar cabeceras de C
  (como si fuesen especificadas con `-I`).
- `SOURCE_DATE_EPOCH`: timestamp de UNIX en ASCII que se usará al generar
  código.
- `PDC_COLORS`: *TODO*

## EJEMPLOS ##

Asumamos los siguientes archivos:

    [ programa1.pd ]
    utilizar bepd/builtins
    Escribir: «¡Hola Mundo!»

    [ programa2.pd ]
    utilizar mi-programa/base (Saludar)
    Saludar: «Mundo»

    [ programa3.pd ]
    utilizar mi-programa/base (Saludar)
    utilizar mi-programa/extra (Mundo)
    Saludar: Mundo

    [ base.pd ]
    utilizar bepd/builtins
    procedimiento Saludar: AQuien
        Escribir: (Texto#Formatear: «¡Hola ~t!», AQuien)
    finprocedimiento

    [ extra.pd ]
    utilizar mi-programa/base ()
    variable Mundo
    fijar Mundo a «Mundo»

Compila `programa1.pd`:

    pdc ./programa1.pd -o programa1
    pdc -O3 ./programa1.pd -o programa1
    pdc -Oz ./programa1.pd -o programa1
    pdc -ggdb -Og ./programa1.pd -o programa1
    pdc -fpie ./programa1.pd -o programa1

Compila `programa2.pd` y `base.pd`:

    pdc mi-programa/inicio:programa2.pd mi-programa/base:base.pd -o programa2
    pdc -O3 mi-programa/inicio:programa2.pd mi-programa/base:base.pd -o programa2
    pdc -ggdb -Og mi-programa/inicio:programa2.pd mi-programa/base:base.pd -o programa2
    pdc -fpie mi-programa/inicio:programa2.pd mi-programa/base:base.pd -o programa2

Compila `base.pd` y `programa2.pd` por separado:

    pdc -b mi-programa/base:base.pd -o base.opd
    pdc -b mi-programa/inicio:programa2.pd -k mi-programa/base:./base.opd -o programa2.opd
    pdc ./base.opd ./programa2.opd -o programa2

Compila `base.pd`, `programa3.pd` y `extra.pd` por separado, creando una
biblioteca para `base` y `extra`:

    mkdir temp
    pdc -b ./base.pd -o temp/base.opd
    pdc -b ./extra.pd -k mi-programa/base:./temp/base.opd -o temp/extra.opd
    pdc -Y mi mi/base:temp/base.opd mi/extra:temp/extra.opd -o mi-programa.bpd
    # Podrías distribuir mi-programa.bpd como una biblioteca.
    
    pdc -b ./programa3.pd -k mi-programa:./mi-programa.bpd -o programa3.opd
    pdc ./programa3.opd -L. -lmi-programa -o programa3

Compila `base.pd`, `programa3.pd` y `extra.pd` por separado, creando una
biblioteca para `base` y `extra`. Genera una biblioteca de enlace dinámico:

    mkdir temp
    pdc -b ./base.pd -o temp/base.opd
    pdc -b ./extra.pd -k mi-programa/base:./temp/base.opd -o temp/extra.opd
    pdc -Y mi -shared mi/base:temp/base.opd mi/extra:temp/extra.opd -o mi-programa.bpd -Zco libmi-programa.so
    # Podrías distribuir mi-programa.bpd y libmi-programa.so como una biblioteca.
    
    pdc -b ./programa3.pd -k mi-programa:./mi-programa.bpd -o programa3.opd
    pdc ./programa3.opd -L. -lmi-programa -o programa3

Compila `base.pd`, `programa3.pd` y `extra.pd` por separado, creando una
biblioteca para `base` y `extra`. Genera una biblioteca de enlace estático:

    mkdir temp
    pdc -b ./base.pd -o temp/base.opd
    pdc -b ./extra.pd -k mi-programa/base:./temp/base.opd -o temp/extra.opd
    pdc -Y mi -fPIC mi/base:temp/base.opd mi/extra:temp/extra.opd -o mi-programa.bpd -Zco mi-programa.o
    ar rcs libmi-programa.a mi-programa.o
    # Podrías distribuir mi-programa.bpd y libmi-programa.a como una biblioteca.
    
    pdc -b ./programa3.pd -k mi-programa:./mi-programa.bpd -o programa3.opd
    pdc ./programa3.opd -L. -lmi-programa -o programa3
