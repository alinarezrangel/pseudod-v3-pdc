cmake_minimum_required(VERSION 3.30)
project(pdcrt
  VERSION 1.0.0
  DESCRIPTION "El runtime de PseudoD"
  HOMEPAGE_URL "https://github.com/alinarezrangel/pseudod-v3-pdc"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64-v3 -Werror=vla")

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
endif()
if(PDCRT_UBSAN)
    target_compile_options(pdcrt PUBLIC -fsanitize=undefined)
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
  PUBLIC_HEADER "${PDCRT_PUBLIC_INCLUDES}/pdcrt-plataforma.h;${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/pdcrt/pdcrt.h"
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

include("./PseudoD.cmake")

set(INNER_PDC_CGEN_BUILD_DIR "${CMAKE_SOURCE_DIR}/outputs/inner_pdc_cgen"
    CACHE PATH "Directorio de build compartido para la generación de C de inner_pdc")
set(INNER_PDC_CGEN_OUTPUT_DIR "${INNER_PDC_CGEN_BUILD_DIR}/cgen")

ExternalProject_Add(inner_pdc_cgen
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/inner_pdc"
        BINARY_DIR "${INNER_PDC_CGEN_BUILD_DIR}"
        CMAKE_ARGS
        "-DPDC=${PDC}"
        "-DPSEUDOD_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DPSEUDOD_CGEN_OUTPUT_DIR=${INNER_PDC_CGEN_OUTPUT_DIR}"
        "-DPSEUDOD_COMPILE_OPTIONS=${PSEUDOD_COMPILE_OPTIONS}"
        INSTALL_COMMAND ""
        USES_TERMINAL_BUILD ON
        BUILD_ALWAYS ON
)

add_pseudod_executable_from_cfiles(inner_pdc
        CGEN_OUTPUT_DIR "${INNER_PDC_CGEN_OUTPUT_DIR}"
        PACKAGES bepd pdc
        ENTRY pdc/inicio
        DEPENDS inner_pdc_cgen
)
pseudod_target_cc_compile_options(inner_pdc PUBLIC -ggdb -O1)

if(PDCRT_ASAN)
  pseudod_target_cc_compile_options(inner_pdc PUBLIC -fsanitize=address)
  pseudod_target_link_libraries(inner_pdc PUBLIC -fsanitize=address)
endif()
if(PDCRT_UBSAN)
  pseudod_target_cc_compile_options(inner_pdc PUBLIC -fsanitize=undefined)
  pseudod_target_link_libraries(inner_pdc PUBLIC -fsanitize=undefined)
endif()

# ============= CPack =============

set(CPACK_PACKAGE_VENDOR "Alejandro Linarez Rangel")
set(CPACK_PACKAGE_DESCRIPTION "El runtime de PseudoD")
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/docs/logo-128.png")
set(CPACK_PACKAGE_CHECKSUM SHA256)
# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

install(
  TARGETS pdcrt
  EXPORT pdcrt_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  PUBLIC_HEADER COMPONENT Development
)

# install(
#   FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt"
#   DESTINATION share/doc/pdcrt
# )

install(
  EXPORT pdcrt_targets
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/pdcrt"
  NAMESPACE pdcrt_
  FILE pdcrt-targets.cmake
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pdcrt-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/pdcrt-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/pdcrt"
  PATH_VARS INSTALL_PREFIX
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/pdcrt-config-version.cmake"
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
  "${CMAKE_CURRENT_BINARY_DIR}/pdcrt-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/pdcrt-config-version.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/pdcrt"
)

export(
  EXPORT pdcrt_targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/pdcrt-targets.cmake"
  NAMESPACE pdcrt_
)

cpack_add_component(
  Development
  DISPLAY_NAME "Desarrollo"
  DESCRIPTION "Archivos necesarios para desarrollar programas en PseudoD"
  INSTALL_TYPES All Developer
)

cpack_add_install_type(All DISPLAY_NAME "Todo")
cpack_add_install_type(Developer DISPLAY_NAME "Desarrollador")
cpack_add_install_type(User DISPLAY_NAME "Usuario")
