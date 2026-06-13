cmake_minimum_required(VERSION 3.30)
project(pseudod-pdc
  VERSION 1.0.0
  DESCRIPTION "Compilador y runtime de PseudoD"
  HOMEPAGE_URL "https://github.com/alinarezrangel/pseudod-v3-pdc"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64-v4 -Werror=vla -fwrapv")

include(CheckCSourceCompiles)
include(CMakeDependentOption)
include(CPack)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(ExternalProject)
include("./cmake/PseudoD2.cmake")

set(TOOL_PREFIX "" CACHE STRING
        "Variable con el prefijo de las bibliotecas y ejecutables.")

set(PSEUDOD_TOOLCHAIN "" CACHE STRING
        "Determina que toolchain de PseudoD será utilizado. Véase `docs/cmake.md` para más información.")

if(NOT PSEUDOD_TOOLCHAIN STREQUAL "")
    file(READ "${PSEUDOD_TOOLCHAIN}" toolchain)
    pseudod_load_toolchain(CUSTOM_PSEUDOD_TOOLCHAIN "${toolchain}")
    set(PSEUDOD_CURRENT_TOOLCHAIN CUSTOM_PSEUDOD_TOOLCHAIN)
else()
    message(NOTICE "PSEUDOD_TOOLCHAIN no fue especificado, usando el toolchain inferido")
endif()

option(PDCRT_ASAN "Activa ASan" OFF)
option(PDCRT_UBSAN "Activa UBSan" OFF)

add_library(pdcrt_lib STATIC
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

set_target_properties(pdcrt_lib PROPERTIES OUTPUT_NAME "${TOOL_PREFIX}pdcrt")

target_include_directories(pdcrt_lib PRIVATE libs/systemtap)
target_compile_options(pdcrt_lib PRIVATE $<$<CONFIG:Debug>:-O1>)

target_link_libraries(pdcrt_lib INTERFACE m)

if(PDCRT_ASAN)
    target_compile_options(pdcrt_lib PUBLIC -fsanitize=address)
    target_link_options(pdcrt_lib PUBLIC -fsanitize=address)
endif()
if(PDCRT_UBSAN)
    target_compile_options(pdcrt_lib PUBLIC -fsanitize=undefined)
    target_link_options(pdcrt_lib PUBLIC -fsanitize=undefined)
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

option(PDCRT_DTRACE "Activa la integración con DTrace / SystemTap" ON)

set(PDCRT_PUBLIC_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}/public-includes")

file(MAKE_DIRECTORY "${PDCRT_PUBLIC_INCLUDES}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/probes/pdcrt-plataforma.h.in"
  "${PDCRT_PUBLIC_INCLUDES}/pdcrt-plataforma.h"
  @ONLY
)

target_include_directories(pdcrt_lib PUBLIC
  "$<BUILD_INTERFACE:${PDCRT_PUBLIC_INCLUDES}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set(PDCRT_RUNTIME "${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/runtime.pd")
set(PDCRT_RUNTIME_OUT "${CMAKE_CURRENT_BINARY_DIR}/runtime.c")

add_custom_command(
  OUTPUT "${PDCRT_RUNTIME_OUT}"
  COMMAND $<TARGET_FILE:pdc>
        -Sc
        -Zid pdcrt_runtime
        -fno-builtin
        -fbuiltin-__RT
        "pdc/runtime/c/runtime:${PDCRT_RUNTIME}"
        -o "${PDCRT_RUNTIME_OUT}"
  JOB_POOL console
  DEPENDS pdc "${PDCRT_RUNTIME}"
  COMMENT "Compiling the PseudoD-side of the runtime"
  VERBATIM
)

set_source_files_properties("${PDCRT_RUNTIME_OUT}" PROPERTIES
        GENERATED TRUE
        COMPILE_OPTIONS "${PSEUDOD_CFLAGS}")
target_sources(pdcrt_lib PRIVATE "${PDCRT_RUNTIME_OUT}")

include("./pdc.cmake")

message(STATUS "Mounting shared pdc files at ${PSEUDOD_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY "${PSEUDOD_CURRENT_BINARY_DIR}")

ExternalProject_Add(inner_pdc_cgen
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/inner_pdc"
        BINARY_DIR "${PSEUDOD_CURRENT_BINARY_DIR}"
        CMAKE_ARGS
        "-DPSEUDOD_TOOLCHAIN=${PSEUDOD_TOOLCHAIN}"
        CMAKE_CACHE_ARGS
        "-DPSEUDOD_CFLAGS:STRING=${PSEUDOD_CFLAGS}"
        "-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}"
        "-DPSEUDOD_SOURCE_DIR:STRING=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DPSEUDOD_CURRENT_BINARY_DIR:STRING=${PSEUDOD_CURRENT_BINARY_DIR}"
        "-DPDC_DEPS_SCRIPT:STRING=${PDC_DEPS_SCRIPT}"
        "-DPDC_REPLACE_JSON_SCRIPT:STRING=${PDC_REPLACE_JSON_SCRIPT}"
        "-DCMAKE_INSTALL_PREFIX:STRING=${CMAKE_INSTALL_PREFIX}"
        "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}"
        "-DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}"
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

set_target_properties(pdc PROPERTIES OUTPUT_NAME "${TOOL_PREFIX}pdc")

if(PDCRT_ASAN)
  target_compile_options(pdc_int INTERFACE -fsanitize=address)
  target_link_options(pdc_int INTERFACE -fsanitize=address)
endif()
if(PDCRT_UBSAN)
  target_compile_options(pdc_int INTERFACE -fsanitize=undefined)
  target_link_options(pdc_int INTERFACE -fsanitize=undefined)
endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.txt.in"
        "${CMAKE_CURRENT_BINARY_DIR}/toolchain.txt"
        @ONLY
)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/local-toolchain.txt.in"
        "${CMAKE_CURRENT_BINARY_DIR}/local-toolchain.txt"
        @ONLY
)

add_custom_target(default_target ALL DEPENDS pdcrt_lib pdc)

# ===================================================================

install(TARGETS pdc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS pdcrt_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TOOL_PREFIX}pdc
)

install(DIRECTORY runtime/c/pdcrt/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TOOL_PREFIX}pdc/pdcrt
        FILES_MATCHING PATTERN "*.h"
)

install(FILES "${PDCRT_PUBLIC_INCLUDES}/pdcrt-plataforma.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TOOL_PREFIX}pdc/
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/toolchain.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${TOOL_PREFIX}pdc
)
