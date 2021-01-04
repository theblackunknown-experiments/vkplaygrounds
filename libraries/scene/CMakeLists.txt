
add_library(scene)
target_sources(scene
    PRIVATE
        vkmesh.hpp
        vkmesh.cpp

        vkcpumesh.hpp
        vkcpumesh.cpp

        vkgpumesh.hpp
        vkgpumesh.cpp
)

target_include_directories(scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_compile_definitions(scene
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            VK_USE_PLATFORM_WIN32_KHR
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
        $<$<PLATFORM_ID:Windows>:OS_WINDOWS>
)

target_link_libraries(scene
    # PUBLIC
    #     Vulkan::Vulkan
    #     glm
    PRIVATE
        glm

        core
        # range-v3::range-v3
)

generate_export_header(scene)
