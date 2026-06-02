cmake_minimum_required(VERSION 3.30)
project(pdcrt
  VERSION 1.0.0
  DESCRIPTION "El runtime de PseudoD"
  HOMEPAGE_URL "https://github.com/alinarezrangel/pseudod-v3-pdc"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64-v3 -Werror=vla -fwrapv -O1")

include(CheckCSourceCompiles)
include(CMakeDependentOption)
include(CPack)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(ExternalProject)

add_subdirectory(build-stage0)

option(PDCRT_ASAN "Activa ASan" OFF)
option(PDCRT_UBSAN "Activa UBSan" OFF)

add_library(pdcrt STATIC
        runtime/c/pdcrt/pdcrt.c
        runtime/c/pdcrt/pdcrt.h
        runtime/c/pdcrt/pdcrt_base.c
        runtime/c/pdcrt/pdcrt_base.h
        runtime/c/pdcrt/pdffi.c
        runtime/c/pdcrt/pdffi.h
        runtime/c/pdcrt/pdcrt_hash.c
        runtime/c/pdcrt/pdcrt_hash.h
        runtime/c/pdcrt/pdcrt_vio.c
        runtime/c/pdcrt/pdcrt_vio.h
        runtime/c/pdcrt/pdcrt_vios/vio_linux.c
        runtime/c/pdcrt/pdcrt_vios/vio_linux.h
        runtime/c/pdcrt/pdcrt_gc.c
        runtime/c/pdcrt/pdcrt_tabla.c
        runtime/c/pdcrt/pdcrt_impls.c
        runtime/c/pdcrt/pdcrt_ops.c
        runtime/c/pdcrt/pdcrt_ops.h
        runtime/c/pdcrt/pdcrt_dtrace.c
        runtime/c/pdcrt/pdcrt_dtrace.h
        runtime/c/pdcrt/pdcrt_impls/archivo.c
        runtime/c/pdcrt/pdcrt_impls/archivo.h)

target_include_directories(pdcrt PRIVATE libs/systemtap)

target_link_libraries(pdcrt INTERFACE m)

if(PDCRT_ASAN)
    target_compile_options(pdcrt PUBLIC -fsanitize=address)
    target_link_options(pdcrt PUBLIC -fsanitize=address)
endif()
if(PDCRT_UBSAN)
    target_compile_options(pdcrt PUBLIC -fsanitize=undefined)
    target_link_options(pdcrt PUBLIC -fsanitize=undefined)
endif()

function(feature_test feat_name probe_file)
  message(CHECK_START "Feature ${feat_name}")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/probes/${probe_file}" PROBE_SOURCE)
  check_c_source_compiles("${PROBE_SOURCE}" PDCRT_PLATAFORMA_${feat_name})
  set(PDCRT_PLATAFORMA_${feat_name} ${PDCRT_PLATAFORMA_${feat_name}} PARENT_SCOPE)
  if(PDCRT_PLATAFORMA_${feat_name})
    message(CHECK_PASS "yes")
  else()
    message(CHECK_FAIL "no")
  endif()
endfunction()

feature_test(PTR_32_BITS "ptr32.c")
feature_test(PTR_64_BITS "ptr64.c")

if(PDCRT_PLATAFORMA_PTR_64_BITS)
  set(PDCRT_PTR_BITS 64)
else()
  if(NOT PDCRT_PLATAFORMA_PTR_32_BITS)
    message(WARNING "No pude detectar el tamaño de los punteros en tu plataforma. Asumiendo 32 bits")
  endif()

  set(PDCRT_PTR_BITS 32)
endif()

option(PDCRT_DBG "Activa la depuración del runtime" OFF)
cmake_dependent_option(PDCRT_DBG_GC "Activa la depuración del recolector de basura" ON PDCRT_DBG OFF)
cmake_dependent_option(PDCRT_DBG_NO_K "Desactiva el uso de continuaciones" ON PDCRT_DBG OFF)
cmake_dependent_option(PDCRT_EMP_INTR "Activa la intercepción de las operaciones de la pila" OFF PDCRT_DBG OFF)

option(PDCRT_DTRACE "Activa la integración con DTrace / SystemTap" OFF)

# NOTA: Cuando actualice mi CMake a 4.1, todo lo siguiente se podría cambiar
# por un `find_program(... VALIDATE ...)`
set(PDC "PDC-NOTFOUND" CACHE STRING "Compilador de PseudoD a utilizar")

if(PDC STREQUAL "PDC-NOTFOUND")
  message(STATUS "Finding default PDC")

  find_program(PDC_PATH pdc NO_CACHE)

  execute_process(
    COMMAND "${PDC_PATH}" -I backends
    RESULT_VARIABLE PDC_STATUS
    OUTPUT_VARIABLE PDC_BACKENDS
  )

  set(FOUND_PDC OFF)
  if(NOT PDC_STATUS)  # 0 if ok
    if(PDC_BACKENDS MATCHES "(^|\n)cc($|\n)")
      set(PDC "${PDC_PATH}" CACHE STRING "Compilador de PseudoD a utilizar" FORCE)
      set(FOUND_PDC ON)
    endif()
  endif()

  if(NOT FOUND_PDC)
    message(FATAL_ERROR "No pude detectar un compilador de PseudoD que pueda compilar a C. Configura manualmente la variable PDC")
  endif()
endif()

set(PDCRT_PUBLIC_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}/public-includes")

file(MAKE_DIRECTORY "${PDCRT_PUBLIC_INCLUDES}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/probes/pdcrt-plataforma.h.in"
  "${PDCRT_PUBLIC_INCLUDES}/pdcrt-plataforma.h"
  @ONLY
)

set_target_properties(pdcrt PROPERTIES
  PUBLIC_HEADER "${PDCRT_PUBLIC_INCLUDES}/pdcrt-plataforma.h;${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/pdcrt/pdffi.h"
)

target_include_directories(pdcrt PUBLIC
  "$<BUILD_INTERFACE:${PDCRT_PUBLIC_INCLUDES}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set(PDCRT_RUNTIME "${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/runtime.pd")
set(PDCRT_RUNTIME_OUT "${CMAKE_CURRENT_BINARY_DIR}/runtime.c")

add_custom_command(
  OUTPUT "${PDCRT_RUNTIME_OUT}"
  COMMAND ${PDC}
        -Sc
        -Zid pdcrt_runtime
        -fno-builtin
        -fbuiltin-__RT
        "pdc/runtime/c/runtime:${PDCRT_RUNTIME}"
        -o "${PDCRT_RUNTIME_OUT}"
  JOB_POOL console
  DEPENDS "${PDCRT_RUNTIME}"
  COMMENT "Compiling the PseudoD-side of the runtime"
  VERBATIM
)

target_sources(pdcrt PRIVATE "${PDCRT_RUNTIME_OUT}")

include("./cmake/PseudoD2.cmake")
include("./pdc.cmake")

message(STATUS "Mounting shared pdc files at ${PSEUDOD_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY "${PSEUDOD_CURRENT_BINARY_DIR}")

ExternalProject_Add(inner_pdc_cgen
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/inner_pdc"
        BINARY_DIR "${PSEUDOD_CURRENT_BINARY_DIR}"
        CMAKE_ARGS
        "-DPDC=${PDC}"
        "-DPSEUDOD_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DPSEUDOD_CURRENT_BINARY_DIR=${PSEUDOD_CURRENT_BINARY_DIR}"
        "-DPDC_DEPS_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/deps.lua"
        INSTALL_COMMAND ""
        USES_TERMINAL_BUILD ON
        BUILD_ALWAYS ON
        BUILD_BYPRODUCTS ${PDC_GEN_FILES}
)

add_pseudod_executable(pdc
        COLLECTIONS col_bepd col_pdc
        ENTRY pdc/inicio
        DEPENDS inner_pdc_cgen
)
target_compile_options(pdc_int INTERFACE -ggdb -O1 -fno-var-tracking-assignments)

if(PDCRT_ASAN)
  target_compile_options(pdc_int INTERFACE -fsanitize=address)
  target_link_options(pdc_int INTERFACE -fsanitize=address)
endif()
if(PDCRT_UBSAN)
  target_compile_options(pdc_int INTERFACE -fsanitize=undefined)
  target_link_options(pdc_int INTERFACE -fsanitize=undefined)
endif()

add_custom_target(default_target ALL DEPENDS sexpr pdcrt pdc)
