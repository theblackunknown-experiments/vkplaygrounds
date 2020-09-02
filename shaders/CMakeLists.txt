
add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
    COMMAND glslangValidator
        -S vert
        -g
        -H
        --entry-point ui_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
    COMMAND glslangValidator
        -S frag
        -g
        -H
        --entry-point ui_main
        --source-entrypoint main
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    COMMENT
        "Compiling GLSL shaders into SPIR-V binary file..."
)

add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
    COMMAND spirv-link
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
        --target-env vulkan1.2
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv"
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
    COMMAND spirv2header
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
        --variable-name kShaderUI
        -o "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
    DEPENDS
        spirv2header.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
    COMMENT
        "Generating C++ shaders for SPIR-V modules..."
)

add_custom_target(shaders_compile_stages
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.spv
        ${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.spv
)

add_custom_target(shaders_link_modules
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/ui.spv"
)

add_custom_target(shaders_headers
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
)

target_include_directories(default-sample
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(default-sample
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/ui-shader.hpp"
)

add_dependencies(default-sample
    shaders_headers
)
