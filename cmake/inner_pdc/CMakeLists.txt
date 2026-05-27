cmake_minimum_required(VERSION 3.30)
project(inner_pdc_cgen LANGUAGES NONE)

include("../PseudoD2.cmake")
include("../../pdc.cmake")

add_pseudod_compile(
        pdc_inner_pdc
        ALL
        COLLECTIONS col_bepd col_pdc
        ENTRY pdc/inicio
)
