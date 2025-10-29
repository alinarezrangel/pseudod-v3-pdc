cmake_minimum_required(VERSION 3.22)

find_package(PkgConfig REQUIRED)
pkg_search_module(LUA lua5.4 lua-5.4)
if(NOT LUA_FOUND)
    message(WARNING "`lua5.4` not found via pkg-config, falling back to `lua`")
    pkg_search_module(LUA REQUIRED lua)
endif()

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
