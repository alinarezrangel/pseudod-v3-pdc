cmake_minimum_required(VERSION 3.30)

# Find the PseudoD compiler
set(PDC_EXECUTABLE "${PDC}")

# Find Lua for dependency extraction
find_program(LUA_EXECUTABLE NAMES lua5.4 lua-5.4 lua)
if(NOT LUA_EXECUTABLE)
    message(FATAL_ERROR "lua interpreter not found in PATH. Please set LUA_EXECUTABLE manually.")
endif()

set(PSEUDOD_CURRENT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/pd" CACHE PATH
        "Directory where the generated PseudoD files will be placed")
set(PSEUDOD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH
        "Directory under which the PseudoD modules live")

# Path to the deps.lua script
set(PDC_DEPS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/deps.lua" CACHE FILEPATH
        "Path to deps.lua script")

function(_pseudod_get_direct_dependencies source_file outvar)
    execute_process(
            COMMAND ${LUA_EXECUTABLE} "${PDC_DEPS_SCRIPT}" direct "${source_file}"
            WORKING_DIRECTORY "${PSEUDOD_SOURCE_DIR}"
            OUTPUT_VARIABLE output
            RESULT_VARIABLE result
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to extract dependencies from PseudoD file ${source_file}")
    endif()

    set("${outvar}" "${output}" PARENT_SCOPE)
endfunction()

function(pseudod_collection pdcoll)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "BASE;PACKAGE" "SOURCES")

    if(arg_BASE)
        if(arg_BASE MATCHES "^(.+)/+$")
            set(base "${CMAKE_MATCH_1}")
        else()
            set(base "${arg_BASE}")
        endif()
    endif()

    set(sources "")
    set(c_files "")
    set(intermediate_files "")
    foreach(source IN LISTS arg_SOURCES)
        if(base)
            if(source MATCHES "^${base}/+([^.]+)(\\..*)\$")
                list(APPEND sources "${arg_PACKAGE}/${CMAKE_MATCH_1}:${PSEUDOD_CURRENT_BINARY_DIR}/${source}")
                list(APPEND c_files "${arg_PACKAGE}/${CMAKE_MATCH_1}:${PSEUDOD_CURRENT_BINARY_DIR}/${arg_PACKAGE}/${CMAKE_MATCH_1}.c")
                list(APPEND intermediate_files "${PSEUDOD_CURRENT_BINARY_DIR}/${arg_PACKAGE}/${CMAKE_MATCH_1}.c")
            else()
                message(FATAL_ERROR "PseudoD source file for collection ${pdcoll} needs to have the form '${base}/<mod>.<ext>': ${source}")
            endif()
        else()
            if(source MATCHES "^${arg_PACKAGE}/([^.]+)(\\..*)\$")
                list(APPEND sources "${arg_PACKAGE}/${CMAKE_MATCH_1}:${PSEUDOD_SOURCE_DIR}/${source}")
                list(APPEND c_files "${arg_PACKAGE}/${CMAKE_MATCH_1}:${PSEUDOD_CURRENT_BINARY_DIR}/${arg_PACKAGE}/${CMAKE_MATCH_1}.c")
                list(APPEND intermediate_files "${PSEUDOD_CURRENT_BINARY_DIR}/${arg_PACKAGE}/${CMAKE_MATCH_1}.c")
            else()
                message(FATAL_ERROR "Could not infer package and module name for PseudoD source file in collection ${pdcoll}: '${source}'")
            endif()
        endif()
    endforeach()

    set("${pdcoll}_PACKAGE" "${arg_PACKAGE}" PARENT_SCOPE)
    set("${pdcoll}_SOURCES" "${sources}" PARENT_SCOPE)
    set("${pdcoll}_C_FILES" "${c_files}" PARENT_SCOPE)
    set("${pdcoll}_GEN_FILES" "${intermediate_files}" PARENT_SCOPE)
endfunction()

function(add_pseudod_compile new_target_name)
    cmake_parse_arguments(PARSE_ARGV 0 arg "ALL" "ENTRY" "COLLECTIONS")

    set(all_cfiles "")

    foreach(pdcoll IN LISTS arg_COLLECTIONS)
        set(sources_data "${${pdcoll}_SOURCES}")

        foreach(source_data IN LISTS sources_data)
            if(source_data MATCHES "^([^/]+)/([^:]+):(.*)$")
                set(package "${CMAKE_MATCH_1}")
                set(module "${CMAKE_MATCH_2}")
                set(sfile "${CMAKE_MATCH_3}")
            else()
                message(FATAL_ERROR "Invalid collection entry: ${pdcoll} -> '${source_data}'")
            endif()

            file(MAKE_DIRECTORY "${PSEUDOD_CURRENT_BINARY_DIR}/${package}")
            set(ifile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.ipd")
            set(tifile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.ipd.bkp")
            # NOTA: Debe ser el mismo .c que agregamos en `pseudod_collection()`
            set(cfile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.c")
            set(tcfile "${PSEUDOD_CURRENT_BINARY_DIR}/${package}/${module}.c.bkp")

            _pseudod_get_direct_dependencies("${sfile}" direct_deps)

            set(interface_deps "")
            set(deps_args "")
            foreach(direct_dep IN LISTS direct_deps)
                string(REGEX REPLACE "\\.(pd|psd|pseudo|pseudod)$" "" path_no_ext "${direct_dep}")
                set(iface_dep "${PSEUDOD_CURRENT_BINARY_DIR}/${path_no_ext}.ipd")
                list(APPEND interface_deps "${iface_dep}")
                list(APPEND deps_args "${path_no_ext}:${iface_dep}")
            endforeach()

            string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" module_id "${package}/${module}")

            add_custom_command(
                    OUTPUT "${ifile}" "${cfile}"
                    COMMAND ${PDC_EXECUTABLE}
                    -Sc
                    -Zid "pdzi${module_id}"
                    "${package}/${module}:${sfile}"
                    -o "${tcfile}"
                    -q "${package}/${module}:${tifile}"
                    ${deps_args}
                    COMMAND ${CMAKE_COMMAND} -E rename "${tcfile}" "${cfile}"
                    COMMAND ${CMAKE_COMMAND} -E rename "${tifile}" "${ifile}"
                    DEPENDS "${sfile}" ${interface_deps}
                    WORKING_DIRECTORY "${PSEUDOD_CURRENT_BINARY_DIR}"
                    COMMENT "Compiling PseudoD module ${package}/${module} to C"
                    VERBATIM
                    COMMAND_EXPAND_LISTS
            )

            list(APPEND all_cfiles "${cfile}")
        endforeach()
    endforeach()

    if(arg_ALL)
        set(opt_all "ALL")
    else()
        set(opt_all "")
    endif()

    add_custom_target("${new_target_name}" ${opt_all} DEPENDS ${all_cfiles})
endfunction()

function(add_pseudod_executable cc_new_target_name)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "ENTRY;DEPENDS" "COLLECTIONS")

    if(NOT arg_ENTRY)
        message(FATAL_ERROR "add_pseudod_executable: ENTRY is required")
    endif()

    set(entry_cfile "")
    set(other_cfiles "")
    set(found_entry OFF)

    foreach(pdcoll IN LISTS arg_COLLECTIONS)
        set(c_files "${${pdcoll}_C_FILES}")

        foreach(source_data IN LISTS c_files)
            if(source_data MATCHES "^([^/]+)/([^:]+):(.*)$")
                set(package "${CMAKE_MATCH_1}")
                set(module "${CMAKE_MATCH_2}")
                set(cfile "${CMAKE_MATCH_3}")
            else()
                message(FATAL_ERROR "Invalid collection entry: ${package} -> '${source_data}'")
            endif()

            if("${package}/${module}" STREQUAL "${arg_ENTRY}")
                set(found_entry ON)
                set(entry_cfile "${cfile}")
            else()
                list(APPEND other_cfiles "${cfile}")
            endif()
        endforeach()
    endforeach()

    if(NOT found_entry)
        message(FATAL_ERROR "add_pseudod_executable: entry ${arg_ENTRY} not found in collections")
    endif()

    set_source_files_properties(${other_cfiles} ${entry_cfile} PROPERTIES GENERATED TRUE)

    add_library("${cc_new_target_name}_int" INTERFACE)
    add_library("${cc_new_target_name}_obj" OBJECT ${entry_cfile})
    target_compile_definitions("${cc_new_target_name}_obj" PRIVATE PDCRT_MAIN)
    target_link_libraries("${cc_new_target_name}_obj" PUBLIC pdcrt "${cc_new_target_name}_int")
    add_executable("${cc_new_target_name}" ${other_cfiles} $<TARGET_OBJECTS:${cc_new_target_name}_obj>)
    target_link_libraries("${cc_new_target_name}" PUBLIC pdcrt "${cc_new_target_name}_int")

    if(arg_DEPENDS)
        add_dependencies("${cc_new_target_name}" "${arg_DEPENDS}")
    endif()
endfunction()
