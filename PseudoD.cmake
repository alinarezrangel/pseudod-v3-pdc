cmake_minimum_required(VERSION 3.30)

# Find the PseudoD compiler
set(PDC_EXECUTABLE "${PDC}")

# Find Lua for dependency extraction
find_program(LUA_EXECUTABLE NAMES lua lua5.4 lua-5.4)
if(NOT LUA_EXECUTABLE)
    message(FATAL_ERROR "lua interpreter not found in PATH. Please set LUA_EXECUTABLE manually.")
endif()

# Path to the deps.lua script
set(PDC_DEPS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/deps.lua" CACHE FILEPATH "Path to deps.lua script")

if(NOT DEFINED PSEUDOD_CURRENT_BINARY_DIR)
    set(PSEUDOD_CURRENT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/pd")
endif()

set(PSEUDOD_COMPILE_OPTIONS "$ENV{PDC_FLAGS}" CACHE STRING
        "Extra options to use when compiling PseudoD code")
set(PSEUDOD_CC_COMPILE_OPTIONS "$ENV{PDC_CFLAGS}" CACHE STRING
        "Extra options to use when compiling PseudoD-generated C code")
set(PSEUDOD_LINK_LIBRARIES "$ENV{PDC_LIBS}" CACHE STRING
        "Extra libraries to link with when linking PseudoD code")

define_property(TARGET PROPERTY PSEUDOD_COMPILE_OPTIONS)
define_property(TARGET PROPERTY PSEUDOD_PUBLIC_CC_COMPILE_OPTIONS)
define_property(TARGET PROPERTY PSEUDOD_PUBLIC_LINK_LIBRARIES)
define_property(TARGET PROPERTY PSEUDOD_PRIVATE_CC_COMPILE_OPTIONS)
define_property(TARGET PROPERTY PSEUDOD_PRIVATE_LINK_LIBRARIES)
define_property(TARGET PROPERTY PSEUDOD_INTERFACE_CC_COMPILE_OPTIONS)
define_property(TARGET PROPERTY PSEUDOD_INTERFACE_LINK_LIBRARIES)

define_property(TARGET PROPERTY PSEUDOD_COLLECTION_SOURCES)

function(add_pseudod_collection pdcoll)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "BASE;PACKAGE" "SOURCES")

    if(arg_BASE)
        if(arg_BASE MATCHES "^(.+)/+$")
            set(base "${CMAKE_MATCH_1}")
        else()
            set(base "${arg_BASE}")
        endif()
    endif()

    add_custom_target("${pdcoll}")

    set(sources "")
    foreach(source IN LISTS arg_SOURCES)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            message(FATAL_ERROR "Sources used in PseudoD collections should be files: ${source}")
        endif()

        if(base)
            if(source MATCHES "^${base}/+([^.]+)(\..*)$")
                list(APPEND sources "${arg_PACKAGE}/${CMAKE_MATCH_1}:${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            else()
                message(FATAL_ERROR "PseudoD source file for collection ${pdcoll} needs to have the form '${base}/<mod>.<ext>': ${source}")
            endif()
        else()
            if(source MATCHES "^${arg_PACKAGE}/([^.]+)(\..*)$")
                list(APPEND sources "${arg_PACKAGE}/${CMAKE_MATCH_1}:${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            else()
                message(FATAL_ERROR "Could not infer package and module name for PseudoD source file in collection ${pdcoll}: '${source}'")
            endif()
        endif()
    endforeach()

    set_target_properties("${pdcoll}"
            PROPERTIES PSEUDOD_COLLECTION_SOURCES "${sources}"
    )
endfunction()

function(_pseudod_get_direct_dependencies source_file outvar)
    execute_process(
            COMMAND ${LUA_EXECUTABLE} "${PDC_DEPS_SCRIPT}" direct "${source_file}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE output
            RESULT_VARIABLE result
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to extract dependencies from PseudoD file ${source_file}")
    endif()

    set("${outvar}" "${output}" PARENT_SCOPE)
endfunction()

function(add_pseudod_executable pdtarget)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "ENTRY" "COLLECTIONS")

    set(other_cfiles "")
    set(entry_cfile "")
    set(found_entry OFF)
    foreach(pdcoll IN LISTS arg_COLLECTIONS)
        get_target_property(sources_data "${pdcoll}" PSEUDOD_COLLECTION_SOURCES)

        foreach(source_data IN LISTS sources_data)
            if(source_data MATCHES "^([^/]+)/([^:]+):(.*)$")
                set(package "${CMAKE_MATCH_1}")
                set(module "${CMAKE_MATCH_2}")
                set(sfile "${CMAKE_MATCH_3}")
            else()
                message(FATAL_ERROR "Invalid collection entry: ${pdcoll} -> '${source_data}'")
            endif()

            file(MAKE_DIRECTORY "${PSEUDOD_CURRENT_BINARY_DIR}/${package}")
            set(ifile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.bdm.json")
            set(cfile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.c")
            set(tcfile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.c.bkp")

            _pseudod_get_direct_dependencies("${sfile}" direct_deps)

            set(interface_deps "")
            set(deps_args "")
            foreach(direct_dep IN LISTS direct_deps)
                string(REGEX REPLACE "\\.(pd|psd|pseudo|pseudod)$" "" path_no_ext "${direct_dep}")
                set(iface_dep "${PSEUDOD_CURRENT_BINARY_DIR}/${path_no_ext}.bdm.json")
                list(APPEND interface_deps "${iface_dep}")
                list(APPEND deps_args --cargar-db "${iface_dep}")
            endforeach()

            file(SHA1 "${sfile}" module_id)

            add_custom_command(
                    OUTPUT "${ifile}" "${cfile}"
                    BYPRODUCTS "${tcfile}"
                    COMMAND ${PDC_EXECUTABLE}
                        --id-modulo "pdh${module_id}"
                        --paquete "${package}"
                        --modulo "${module}"
                        -o "${tcfile}"
                        --guardar-db "${ifile}"
                        --guardar-solo-modulo
                        ${deps_args}
                        ${PSEUDOD_COMPILE_OPTIONS}
                        "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_COMPILE_OPTIONS>"
                        "${sfile}"
                    COMMAND ${CMAKE_COMMAND} -E rename "${tcfile}" "${cfile}"
                    COMMAND ${CMAKE_COMMAND} -E touch "${tcfile}"
                    DEPENDS "${sfile}" ${interface_deps}
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    COMMENT "Compiling PseudoD module ${package}/${module}"
                    VERBATIM
                    COMMAND_EXPAND_LISTS
            )

            if("${package}/${module}" STREQUAL arg_ENTRY)
                set(found_entry ON)
                set(entry_cfile "${cfile}")
            else()
                list(APPEND other_cfiles "${cfile}")
            endif()
        endforeach()
    endforeach ()

    if(NOT found_entry)
        message(FATAL_ERROR "PseudoD executable entry ${arg_ENTRY} not found in collections for ${pdtarget}")
    endif()

    add_library("${pdtarget}_obj" OBJECT ${other_cfiles})
    target_link_libraries("${pdtarget}_obj" PUBLIC pdcrt)
    target_compile_options("${pdtarget}_obj" PUBLIC "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_PUBLIC_CC_COMPILE_OPTIONS>")
    target_compile_options("${pdtarget}_obj" PRIVATE ${PSEUDOD_CC_COMPILE_OPTIONS} "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_PRIVATE_CC_COMPILE_OPTIONS>")
    target_compile_options("${pdtarget}_obj" INTERFACE "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_INTERFACE_CC_COMPILE_OPTIONS>")

    add_executable("${pdtarget}" ${entry_cfile} "$<TARGET_OBJECTS:${pdtarget}_obj>")
    target_compile_definitions("${pdtarget}" PRIVATE PDCRT_MAIN)
    target_compile_options("${pdtarget}" PUBLIC "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_PUBLIC_CC_COMPILE_OPTIONS>")
    target_compile_options("${pdtarget}" PRIVATE ${PSEUDOD_CC_COMPILE_OPTIONS} "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_PRIVATE_CC_COMPILE_OPTIONS>")
    target_compile_options("${pdtarget}" INTERFACE "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_INTERFACE_CC_COMPILE_OPTIONS>")

    target_link_libraries("${pdtarget}" PUBLIC pdcrt)
    target_link_libraries("${pdtarget}" PUBLIC "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_PUBLIC_LINK_LIBRARIES>")
    target_link_libraries("${pdtarget}" PRIVATE ${PSEUDOD_LINK_LIBRARIES} "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_PRIVATE_LINK_LIBRARIES>")
    target_link_libraries("${pdtarget}" INTERFACE "$<TARGET_PROPERTY:${pdtarget},PSEUDOD_INTERFACE_LINK_LIBRARIES>")
endfunction()

function(add_pseudod_library pdtarget)
    cmake_parse_arguments(PARSE_ARGV 0 arg "SHARED;STATIC" "" "COLLECTIONS")
    message(FATAL_ERROR "not implemented")
endfunction()

function(pseudod_target_compile_options pdtarget)
    get_target_property(opts "${pdtarget}" PSEUDOD_COMPILE_OPTIONS)
    if(opts STREQUAL "opts-NOTFOUND")
        set(opts "")
    endif()
    list(APPEND opts ${ARGN})
    set_target_properties("${pdtarget}" PROPERTIES PSEUDOD_COMPILE_OPTIONS "${opts}")
endfunction()

function(_pseudod_check_visibility msg vis)
    if((NOT vis STREQUAL "PUBLIC") AND (NOT vis STREQUAL "PRIVATE") AND (NOT vis STREQUAL "INTERFACE"))
        message(FATAL_ERROR "${msg}: Expected PUBLIC|PRIVATE|INTERFACE but got '${vis}'")
    endif()
endfunction()

function(pseudod_target_link_libraries pdtarget vis)
    _pseudod_check_visibility("pseudod_target_link_libraries" "${vis}")
    get_target_property(opts "${pdtarget}" PSEUDOD_${vis}_LINK_LIBRARIES)
    if(opts STREQUAL "opts-NOTFOUND")
        set(opts "")
    endif()
    list(APPEND opts ${ARGN})
    set_target_properties("${pdtarget}" PROPERTIES PSEUDOD_${vis}_LINK_LIBRARIES "${opts}")
endfunction()

function(pseudod_target_cc_compile_options pdtarget vis)
    _pseudod_check_visibility("pseudod_target_cc_compile_options" "${vis}")
    get_target_property(opts "${pdtarget}" PSEUDOD_${vis}_CC_COMPILE_OPTIONS)
    if(opts STREQUAL "opts-NOTFOUND")
        set(opts "")
    endif()
    list(APPEND opts ${ARGN})
    set_target_properties("${pdtarget}" PROPERTIES PSEUDOD_${vis}_CC_COMPILE_OPTIONS "${opts}")
endfunction()
