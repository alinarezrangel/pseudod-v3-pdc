cmake_minimum_required(VERSION 3.22)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED lua5.4)

add_library(sexpr SHARED sexpr.c)

target_compile_options(sexpr PRIVATE ${LUA_CFLAGS})
target_link_libraries(sexpr ${LUA_LIBRARIES})

set_target_properties(sexpr PROPERTIES
        PREFIX ""
        SUFFIX ".so"
        OUTPUT_NAME "sexpr"
)

target_include_directories(sexpr PRIVATE ${LUA_INCLUDE_DIRS})

target_compile_options(sexpr PRIVATE -Wall -Wextra)
