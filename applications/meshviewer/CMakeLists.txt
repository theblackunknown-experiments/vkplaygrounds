
##############################
##         Targets          ##
##############################

add_executable(meshviewer WIN32)
target_sources(meshviewer
    PRIVATE
        # generic

        vkutilities.hpp
        vkutilities.cpp

        vkapplication.hpp
        vkapplication.cpp

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

        # sample specific

        vkmeshviewer.hpp
        vkmeshviewer.cpp

        vkmeshviewerdata.hpp
        vkmeshviewerdata.cpp

        vkpassuioverlay.hpp
        vkpassuioverlay.cpp

        vkpassscene.hpp
        vkpassscene.cpp

        # launchers

        $<$<PLATFORM_ID:Windows>:win32_main.cpp>
)

target_include_directories(meshviewer
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(meshviewer
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            VK_USE_PLATFORM_WIN32_KHR
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
        $<$<PLATFORM_ID:Windows>:OS_WINDOWS>
)

target_link_libraries(meshviewer
    PRIVATE
        core
        libobj2mesh

        dearimgui
        utilities
        Vulkan::Vulkan
        range-v3::range-v3

        baked_font
        baked_shaders
)

# add_dependencies(meshviewer
#     baked_meshes
# )

if(WIN32)
    add_custom_command(TARGET meshviewer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:core> $<TARGET_FILE_DIR:meshviewer>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libobj2mesh> $<TARGET_FILE_DIR:meshviewer>
        COMMENT "Fetching runtime dependencies next to executable..."
    )
endif()
