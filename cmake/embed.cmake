message(STATUS "BIN2S path: ${BIN2S_PATH}")

function(add_embedded_binary_lib libname)
    set(multiValueArgs INPUTS)
    cmake_parse_arguments(PARSE_ARGV 0 arg
            "" "" "${multiValueArgs}"
    )

    get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
    if(NOT "ASM" IN_LIST languages)
        message(FATAL_ERROR "add_embedded_binary_lib: ASM language is not enabled")
    endif ()

    set(output_folder "${CMAKE_BINARY_DIR}/${libname}")
    foreach (filepath ${arg_INPUTS})
        file(REAL_PATH ${filepath} real_filepath)
        cmake_path(GET real_filepath FILENAME basename)

        string(REPLACE "." "_" out_filename "${basename}")
        set(s_path "${output_folder}/${out_filename}.S")
        set(h_path "${output_folder}/${out_filename}.h")

        add_custom_command(
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT "${h_path}" "${s_path}"
                DEPENDS ${real_filepath}
                COMMAND ${DKP_BIN2S} -a ${DKP_BIN2S_ALIGNMENT} -H "${h_path}" "${real_filepath}" > "${s_path}"
        )
        list(APPEND intm_files ${s_path} ${h_path})
        set_source_files_properties(${s_path} PROPERTIES LANGUAGE "ASM")
    endforeach ()
    add_library(shaders OBJECT ${intm_files})
    target_include_directories(shaders INTERFACE "${output_folder}")
endfunction()