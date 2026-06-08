cmake_minimum_required(VERSION 3.30)
project(inner_pdc_cgen LANGUAGES NONE)

include("../PseudoD2.cmake")
include("../../pdc.cmake")

set(PSEUDOD_TOOLCHAIN "" CACHE STRING
        "Determina que toolchain de PseudoD será utilizado. Véase `docs/cmake.md` para más información.")

if(NOT PSEUDOD_TOOLCHAIN STREQUAL "")
    file(READ "${PSEUDOD_TOOLCHAIN}" toolchain)
    pseudod_load_toolchain(CUSTOM_PSEUDOD_TOOLCHAIN "${toolchain}")
    set(PSEUDOD_CURRENT_TOOLCHAIN CUSTOM_PSEUDOD_TOOLCHAIN)
else()
    message(NOTICE "PSEUDOD_TOOLCHAIN no fue especificado, usando el toolchain inferido")
endif()

add_pseudod_compile(
        pdc_inner_pdc
        ALL
        COLLECTIONS col_bepd col_pdc
)
