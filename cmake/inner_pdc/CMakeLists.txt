cmake_minimum_required(VERSION 3.30)
project(inner_pdc_cgen LANGUAGES NONE)

include("../../PseudoD.cmake")

pseudod_cgen_collection(src_bepd
    PACKAGE bepd
    SOURCES
    bepd/algoritmos.pd
    bepd/builtins.pd
    bepd/builtinsImpl.pd
    bepd/datos/caja.pd
    bepd/datos/conjunto.pd
    bepd/datos/diccionario.pd
    bepd/datos/diccionario/alist.pd
    bepd/datos/diccionario/hashmap.pd
    bepd/datos/diccionario/interfaz.pd
    bepd/datos/pila.pd
    bepd/datos/resultado.pd
    bepd/intrinsics.pd
    bepd/utilidades/arreglo.pd
    bepd/utilidades/iteración.pd
    bepd/utilidades/texto.pd
    bepd/utilidades/texto/ascii.pd
    bepd/x/adhoc.pd
    bepd/x/algebraico.pd
    bepd/x/cli.pd
    bepd/x/control.pd
    bepd/x/datos.pd
    bepd/x/datos2.pd
    bepd/x/entorno.pd
    bepd/x/enum.pd
    bepd/x/json.pd
    bepd/x/lazy.pd
    bepd/x/logs.pd
    bepd/x/puerto.pd
    bepd/x/puerto/conPosición.pd
    bepd/x/puerto/deArchivo.pd
    bepd/x/sexpr.pd
    bepd/x/sistemaDeArchivos/archivo.pd
    bepd/x/sistemaDeArchivos/rutas.pd
    bepd/x/utf8.pd
)

pseudod_cgen_collection(src_pdc
    PACKAGE pdc
    SOURCES
    pdc/abstraer.pd
    pdc/ast.pd
    pdc/backend/c.pd
    pdc/backend/c/ir.pd
    pdc/backend/c/lowerer.pd
    pdc/capturas.pd
    pdc/catamorfismos.pd
    pdc/combinadores.pd
    pdc/compilador.pd
    pdc/cst.pd
    pdc/dependencias.pd
    pdc/info.pd
    pdc/inicio.pd
    pdc/módulos.pd
    pdc/nombres.pd
    pdc/parser.pd
    pdc/tabla.pd
    pdc/tokenizador.pd
    pdc/tokens.pd
    pdc/validación.pd
    pdc/subproceso.pd
)

pseudod_cgen_generate(
    COLLECTIONS src_bepd src_pdc
    ENTRY pdc/inicio
)
