
##############################
##         Targets          ##
##############################

add_executable(00-sample WIN32)
target_sources(00-sample
    PRIVATE
        # generic

        vkutilities.hpp
        vkutilities.cpp

        vkengine.hpp
        vkengine.cpp

        vkpresentation.hpp
        vkpresentation.cpp

        vkpass.hpp
        vkpass.cpp

        vkrenderpass.hpp
        vkrenderpass.cpp

        vksurface.hpp
        $<$<PLATFORM_ID:Windows>:win32_vksurface.cpp>

        charconv.hpp
        $<$<PLATFORM_ID:Windows>:win32_charconv.cpp>

        console.hpp
        $<$<PLATFORM_ID:Windows>:win32_console.hpp>
        $<$<PLATFORM_ID:Windows>:win32_console.cpp>

        $<$<PLATFORM_ID:Windows>:win32_windowprocedure.hpp>
        $<$<PLATFORM_ID:Windows>:win32_windowprocedure.cpp>

        # sample specific

        vklauncher.hpp
        vklauncher.cpp

        vkapplication.hpp
        vkapplication.cpp

        vk00-sampledata.hpp
        vk00-sampledata.cpp

        # launchers

        $<$<PLATFORM_ID:Windows>:win32_main.cpp>
)

target_include_directories(00-sample
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(00-sample
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            VK_USE_PLATFORM_WIN32_KHR
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
        $<$<PLATFORM_ID:Windows>:OS_WINDOWS>
)

target_link_libraries(00-sample
    PRIVATE
        core
        libobj2mesh

        dearimgui
        Vulkan::Vulkan
        range-v3::range-v3
        glm

        baked_font
        baked_shaders
)

# add_dependencies(00-sample
#     baked_meshes
# )

if(WIN32)
    add_custom_command(TARGET 00-sample
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:core> $<TARGET_FILE_DIR:00-sample>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:scene> $<TARGET_FILE_DIR:00-sample>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libobj2mesh> $<TARGET_FILE_DIR:00-sample>
        COMMENT "Fetching runtime dependencies next to executable..."
    )
endif()
