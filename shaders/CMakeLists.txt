
add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
    COMMAND glslangValidator -V -S vert -e main -o "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"   "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
    COMMAND glslangValidator -V -S frag -e main -o "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv" "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    COMMENT
        "Compiling GLSL shaders into SPIR-V binary file..."
)

add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.hpp"
    COMMAND glslangValidator -V -S vert -e main -o "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.hpp"   --variable-name shader_ui_vertex   "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
    COMMAND glslangValidator -V -S frag -e main -o "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.hpp" --variable-name shader_ui_fragment "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    COMMENT
        "Compiling GLSL shaders into SPIR-V headers..."
)

add_custom_target(bake_shaders_into_headers
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.hpp
)

target_include_directories(default-sample
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

# NOTE File not yet written configure ends
# target_sources(default-sample
#     PRIVATE
#         "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.hpp"
#         "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.hpp"
# )

add_dependencies(default-sample
    bake_shaders_into_headers
)
