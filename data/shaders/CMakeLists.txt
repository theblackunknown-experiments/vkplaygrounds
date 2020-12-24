
add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.fragment.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.fragment.spv"
    COMMAND glslangValidator
        -S vert
        -g
        # -H
        --entry-point ui_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
    COMMAND glslangValidator
        -S frag
        -g
        # -H
        --entry-point ui_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    COMMAND glslangValidator
        -S vert
        -g
        # -H
        --entry-point triangle_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/triangle.vertex.spv"
        "${CMAKE_CURRENT_LIST_DIR}/triangle.vertex.glsl"
    COMMAND glslangValidator
        -S frag
        -g
        # -H
        --entry-point triangle_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/triangle.fragment.spv"
        "${CMAKE_CURRENT_LIST_DIR}/triangle.fragment.glsl"
    COMMAND glslangValidator
        -S vert
        -g
        # -H
        --entry-point obj_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/obj.vertex.spv"
        "${CMAKE_CURRENT_LIST_DIR}/obj.vertex.glsl"
    COMMAND glslangValidator
        -S frag
        -g
        # -H
        --entry-point obj_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/obj.fragment.spv"
        "${CMAKE_CURRENT_LIST_DIR}/obj.fragment.glsl"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/triangle.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/triangle.fragment.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/obj.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/obj.fragment.glsl"
    COMMENT
        "Compiling GLSL shaders into SPIR-V binary file..."
)

add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.spv"
    COMMAND spirv-link
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
    COMMAND spirv-link
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.fragment.spv"
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/triangle.spv"
    COMMAND spirv-link
        "${CMAKE_CURRENT_BINARY_DIR}/obj.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.fragment.spv"
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/obj.spv"
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.fragment.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.fragment.spv"
    COMMENT
        "Compiling SPIR-V shaders into modules..."
)

add_executable(spirv2header)

target_sources(spirv2header
    PRIVATE
        spirv2header.cpp
)

add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle-shader.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/obj-shader.hpp"
    COMMAND $<TARGET_FILE:spirv2header>
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
        --variable-name kShaderUI
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
    COMMAND $<TARGET_FILE:spirv2header>
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.spv"
        --variable-name kShaderTriangle
        -o "${CMAKE_CURRENT_BINARY_DIR}/triangle-shader.hpp"
    COMMAND $<TARGET_FILE:spirv2header>
        "${CMAKE_CURRENT_BINARY_DIR}/obj.spv"
        --variable-name kShaderOBJ
        -o "${CMAKE_CURRENT_BINARY_DIR}/obj-shader.hpp"
    DEPENDS
        spirv2header.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/obj.spv"
    COMMENT
        "Generating C++ shaders for SPIR-V modules..."
)

add_custom_target(shaders_headers
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/triangle-shader.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/obj-shader.hpp"
)

add_library(baked_shaders INTERFACE)

# target_sources(baked_shaders
#     INTERFACE
#         "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
#         "${CMAKE_CURRENT_BINARY_DIR}/triangle-shader.hpp"
#         "${CMAKE_CURRENT_BINARY_DIR}/obj-shader.hpp"
# )

target_include_directories(baked_shaders
    INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(baked_shaders
    shaders_headers
)
