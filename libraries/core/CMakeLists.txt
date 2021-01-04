
add_library(core)
target_sources(core
    PRIVATE
        vksize.hpp

        vkdebug.hpp
        vkdebug.cpp

        vkbuffer.hpp
        vkbuffer.cpp

        vkimage.hpp
        vkimage.cpp

        vkmemory.hpp
        vkmemory.cpp

        vkqueue.hpp
        vkqueue.cpp

        vkphysicaldevice.hpp
        vkphysicaldevice.cpp

        vkdevice.hpp
        vkdevice.cpp

        $<$<PLATFORM_ID:Windows>:win32_vkdebug.cpp>
)

target_include_directories(core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_compile_definitions(core
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            VK_USE_PLATFORM_WIN32_KHR
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
        $<$<PLATFORM_ID:Windows>:OS_WINDOWS>
)

target_link_libraries(core
    PUBLIC
        Vulkan::Vulkan
        glm
    PRIVATE
        range-v3::range-v3
        tinyobjloader::tinyobjloader
)

generate_export_header(core)
