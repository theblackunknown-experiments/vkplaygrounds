
add_executable(spirv2header)

target_sources(spirv2header
    PRIVATE
        spirv2header.cpp
)

set(_SHADER_MODULE_NAMES
    ui
    obj
    triangle
    default-triangle
    mesh-color
    mesh-position
    mesh-normal
)

set(_SHADER_MODULE_ENTRY_POINT_ui               ui_main)
set(_SHADER_MODULE_ENTRY_POINT_obj              obj_main)
set(_SHADER_MODULE_ENTRY_POINT_triangle         triangle_main)
set(_SHADER_MODULE_ENTRY_POINT_default-triangle default_triangle_main)
set(_SHADER_MODULE_ENTRY_POINT_mesh-color       mesh_color_main)
set(_SHADER_MODULE_ENTRY_POINT_mesh-position    mesh_position_main)
set(_SHADER_MODULE_ENTRY_POINT_mesh-normal      mesh_normal_main)

set(_SHADER_MODULE_HEADER_CONSTANT_ui               kShaderUI)
set(_SHADER_MODULE_HEADER_CONSTANT_obj              kShaderOBJ)
set(_SHADER_MODULE_HEADER_CONSTANT_triangle         kShaderTriangle)
set(_SHADER_MODULE_HEADER_CONSTANT_default-triangle kShaderDefaultTriangle)
set(_SHADER_MODULE_HEADER_CONSTANT_mesh-color       kShaderMeshColor)
set(_SHADER_MODULE_HEADER_CONSTANT_mesh-position    kShaderMeshPosition)
set(_SHADER_MODULE_HEADER_CONSTANT_mesh-normal      kShaderMeshNormal)

set(_SHADER_MODULE_HEADERS )

foreach(_SHADER_MODULE ${_SHADER_MODULE_NAMES})
    add_custom_command(OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.vertex.spv"
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.fragment.spv"
        COMMAND glslangValidator
            -S vert
            -g
            --entry-point ${_SHADER_MODULE_ENTRY_POINT_${_SHADER_MODULE}}
            --source-entrypoint main
            --target-env vulkan1.2
            -o "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.vertex.spv"
            "${CMAKE_CURRENT_LIST_DIR}/${_SHADER_MODULE}.vertex.glsl"
        COMMAND glslangValidator
            -S frag
            -g
            --entry-point ${_SHADER_MODULE_ENTRY_POINT_${_SHADER_MODULE}}
            --source-entrypoint main
            --target-env vulkan1.2
            -o "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.fragment.spv"
            "${CMAKE_CURRENT_LIST_DIR}/${_SHADER_MODULE}.fragment.glsl"
        DEPENDS
            "${CMAKE_CURRENT_LIST_DIR}/${_SHADER_MODULE}.vertex.glsl"
            "${CMAKE_CURRENT_LIST_DIR}/${_SHADER_MODULE}.fragment.glsl"
        COMMENT
            "[shaders/${_SHADER_MODULE}] Compiling GLSL shaders into SPIR-V binary file..."
    )

    add_custom_command(OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.spv"
        COMMAND spirv-link
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.vertex.spv"
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.fragment.spv"
            --target-env vulkan1.2
            -o "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.spv"
        DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.vertex.spv"
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.fragment.spv"
        COMMENT
            "[shaders/${_SHADER_MODULE}] Compiling SPIR-V shaders into modules..."
    )

    add_custom_command(OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}-shader.hpp"
        COMMAND $<TARGET_FILE:spirv2header>
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.spv"
            --variable-name ${_SHADER_MODULE_HEADER_CONSTANT_${_SHADER_MODULE}}
            -o "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}-shader.hpp"
        DEPENDS
            spirv2header.cpp
            "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}.spv"
        COMMENT
            "[shaders/${_SHADER_MODULE}] Generating C++ shaders for SPIR-V modules..."
    )

    list(APPEND _SHADER_MODULE_HEADERS
        "${CMAKE_CURRENT_BINARY_DIR}/${_SHADER_MODULE}-shader.hpp")
endforeach()

add_custom_target(shaders_headers
    DEPENDS
        ${_SHADER_MODULE_HEADERS}
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
