set_source_files_properties(pdc/compilador.pd PROPERTIES PSEUDOD_COMPILE_OPTIONS "-Dtoolchain=${PSEUDOD_TARGET_TOOLCHAIN_PATH}")

pseudod_collection(col_bepd
        PACKAGE bepd
        SOURCES
        bepd/algoritmos.pd
        bepd/builtins.pd
        bepd/builtinsImpl.pd
        bepd/datos/caja.pd
        bepd/datos/conjunto.pd
        bepd/datos/diccionario.pd
        bepd/datos/pila.pd
        bepd/datos/resultado.pd
        bepd/intrinsics.pd
        bepd/utilidades/arreglo.pd
        bepd/utilidades/iteración.pd
        bepd/utilidades/texto.pd
        bepd/utilidades/texto/ascii.pd
        bepd/utilidades/texto/utf8.pd
        bepd/utilidades/texto/utf16.pd
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
        bepd/x/subproceso.pd
)

pseudod_collection(col_pdc
        PACKAGE pdc
        SOURCES
        pdc/abstraer.pd
        pdc/ast.pd
        pdc/backend/c/revc.pd
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
        pdc/cabeceras.pd
)

set(PDC_GEN_FILES ${col_bepd_GEN_FILES} ${col_pdc_GEN_FILES})
