
add_library(scene)
target_sources(scene
    PRIVATE
        vkmesh.hpp
        vkmesh.cpp

        vkcpumesh.hpp
        vkcpumesh.cpp

        vkgpumesh.hpp
        vkgpumesh.cpp

        vkcamera.hpp
        vkcamera.cpp

        vkmaterial.hpp
        vkmaterial.cpp

        vkrenderable.hpp
        vkrenderable.cpp
)

target_include_directories(scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_compile_definitions(scene
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
        $<$<PLATFORM_ID:Windows>:OS_WINDOWS>
)

target_link_libraries(scene
    PUBLIC
        glm
        Vulkan::Vulkan

    PRIVATE
        core
)

generate_export_header(scene)
