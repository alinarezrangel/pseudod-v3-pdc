cmake_minimum_required(VERSION 3.22)
project(pdcrt
  VERSION 1.0.0
  DESCRIPTION "El runtime de PseudoD"
  HOMEPAGE_URL "https://github.com/alinarezrangel/pseudod-v3-pdc"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)

include(CheckCSourceCompiles)
include(CMakeDependentOption)
include(CPack)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(build-stage0)

add_library(pdcrt STATIC
  runtime/c/pdcrt.c)

target_link_libraries(pdcrt INTERFACE m)

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

feature_test(CLOCK_GETTIME_MONOTONIC "clock_gettime_monotonic.c")
feature_test(TIMESPEC_GET_MONOTONIC "timespec_get_monotonic.c")
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
cmake_dependent_option(PDCRT_EMP_INTR "Activa la intercepción de las operaciones de la pila" OFF PDCRT_DBG OFF)

option(PDCRT_LOG "Activa el sistema de logging del runtime" OFF)
cmake_dependent_option(PDCRT_LOG_GC "Activa el sistema de logging del recolector de basura" ON PDCRT_LOG OFF)

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
  PUBLIC_HEADER "${PDCRT_PUBLIC_INCLUDES}/pdcrt-plataforma.h;${CMAKE_CURRENT_SOURCE_DIR}/runtime/c/pdcrt.h"
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
  COMMAND "${PDC}"
  --id-modulo pdcrt_runtime
  --paquete pdc
  --modulo runtime/c/runtime
  -X Builtins:RT
  -o "${PDCRT_RUNTIME_OUT}"
  "${PDCRT_RUNTIME}"
  DEPENDS "${PDCRT_RUNTIME}"
  COMMENT "Compiling the PseudoD-side of the runtime"
  VERBATIM
)

target_sources(pdcrt PRIVATE "${PDCRT_RUNTIME_OUT}")

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
