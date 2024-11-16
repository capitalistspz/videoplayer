set(BIN2S_PATH ${DKP_BIN2S})
include(embed)
message(STATUS "GLSL compiler path: ${GLSL_COMPILER_PATH}")
if (GLSL_COMPILER_PATH)
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/nv12torgb.gsh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND ${GLSL_COMPILER_PATH} -ps nv12torgb.frag -vs nv12torgb.vert -o ${CMAKE_CURRENT_BINARY_DIR}/nv12torgb.gsh)
endif ()
add_embedded_binary_lib(shaders INPUTS ${CMAKE_CURRENT_BINARY_DIR}/nv12torgb.gsh)
