cmake_minimum_required(VERSION 3.15)

project(blk-imgui
    VERSION 0.0.0
    DESCRIPTION "Playground with imgui"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)

##############################
##         Options          ##
##############################

option(BUILD_SHARED_LIBS "Build shared libraries"          ON)
option(INSTALL_HEADERS   "Install the development headers" ON)

##############################
##        Includes          ##
##############################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

##############################
##        Packages          ##
##############################

find_package(unofficial-imgui CONFIG REQUIRED)
find_package(Vulkan                  REQUIRED)

##############################
##         Targets          ##
##############################

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "NYI ${CMAKE_SYSTEM_NAME}")
endif()

message(VERBOSE "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(VERBOSE "CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")

add_custom_command(OUTPUT
        "${CMAKE_BINARY_DIR}/ui.vertex.spv"
        "${CMAKE_BINARY_DIR}/ui.fragment.spv"
    COMMAND glslangValidator -V -S vert -e main -o "${CMAKE_BINARY_DIR}/ui.vertex.spv"   "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
    COMMAND glslangValidator -V -S frag -e main -o "${CMAKE_BINARY_DIR}/ui.fragment.spv" "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    COMMENT
        "Compiling GLSL shaders into SPIR-V binary file..."
)

add_custom_command(OUTPUT
        "${CMAKE_BINARY_DIR}/ui.vertex.hpp"
        "${CMAKE_BINARY_DIR}/ui.fragment.hpp"
    COMMAND glslangValidator -V -S vert -e main -o "${CMAKE_BINARY_DIR}/ui.vertex.hpp"   --variable-name shader_ui_vertex   "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
    COMMAND glslangValidator -V -S frag -e main -o "${CMAKE_BINARY_DIR}/ui.fragment.hpp" --variable-name shader_ui_fragment "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/ui.vertex.glsl"
        "${CMAKE_CURRENT_LIST_DIR}/ui.fragment.glsl"
    COMMENT
        "Compiling GLSL shaders into SPIR-V headers..."
)

add_executable(sample-imgui WIN32)
target_sources(sample-imgui
    PRIVATE
        $<$<PLATFORM_ID:Windows>:winmain.cpp>
        vulkandebug.hpp
        $<$<PLATFORM_ID:Windows>:winvulkandebug.cpp>

        vulkanapplicationbase.hpp
        vulkanphysicaldevicebase.hpp
        vulkandevicebase.hpp
        vulkansurfacebase.hpp

        vulkaninstancemixin.hpp
        vulkanphysicaldevicemixin.hpp
        vulkandevicemixin.hpp
        vulkansurfacemixin.hpp
        vulkanqueuemixin.hpp

        vulkanapplication.hpp
        vulkanapplication.cpp
        vulkanphysicaldevice.hpp
        vulkanphysicaldevice.cpp
        vulkandevice.hpp
        vulkandevice.cpp
        vulkansurface.hpp
        vulkansurface.cpp
        vulkandearimgui.hpp
        vulkandearimgui.cpp
        "${CMAKE_BINARY_DIR}/ui.vertex.hpp"
        "${CMAKE_BINARY_DIR}/ui.fragment.hpp"
)

target_include_directories(sample-imgui
    PRIVATE
        ${CMAKE_BINARY_DIR}
)

#  If defined, the following flags inhibit definition
#     of the indicated items.
#
#  NOGDICAPMASKS     - CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#  NOVIRTUALKEYCODES - VK_*
#  NOWINMESSAGES     - WM_*, EM_*, LB_*, CB_*
#  NOWINSTYLES       - WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#  NOSYSMETRICS      - SM_*
#  NOMENUS           - MF_*
#  NOICONS           - IDI_*
#  NOKEYSTATES       - MK_*
#  NOSYSCOMMANDS     - SC_*
#  NORASTEROPS       - Binary and Tertiary raster ops
#  NOSHOWWINDOW      - SW_*
#  OEMRESOURCE       - OEM Resource values
#  NOATOM            - Atom Manager routines
#  NOCLIPBOARD       - Clipboard routines
#  NOCOLOR           - Screen colors
#  NOCTLMGR          - Control and Dialog routines
#  NODRAWTEXT        - DrawText() and DT_*
#  NOGDI             - All GDI defines and routines
#  NOKERNEL          - All KERNEL defines and routines
#  NOUSER            - All USER defines and routines
#  NONLS             - All NLS defines and routines
#  NOMB              - MB_* and MessageBox()
#  NOMEMMGR          - GMEM_*, LMEM_*, GHND, LHND, associated routines
#  NOMETAFILE        - typedef METAFILEPICT
#  NOMINMAX          - Macros min(a,b) and max(a,b)
#  NOMSG             - typedef MSG and associated routines
#  NOOPENFILE        - OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#  NOSCROLL          - SB_* and scrolling routines
#  NOSERVICE         - All Service Controller routines, SERVICE_ equates, etc.
#  NOSOUND           - Sound driver routines
#  NOTEXTMETRIC      - typedef TEXTMETRIC and associated routines
#  NOWH              - SetWindowsHook and WH_*
#  NOWINOFFSETS      - GWL_*, GCL_*, associated routines
#  NOCOMM            - COMM driver routines
#  NOKANJI           - Kanji support stuff.
#  NOHELP            - Help engine interface.
#  NOPROFILER        - Profiler interface.
#  NODEFERWINDOWPOS  - DeferWindowPos routines
#  NOMCX             - Modem Configuration Extensions
target_compile_definitions(sample-imgui
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            VK_USE_PLATFORM_WIN32_KHR
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
)

target_link_libraries(sample-imgui
    PRIVATE
        unofficial-imgui::imgui
        Vulkan::Vulkan
)

##############################
##          Config          ##
##############################

file(WRITE "${CMAKE_BINARY_DIR}/blk-imgui-config.cmake.in" [=[
    @PACKAGE_INIT@
    include("@PACKAGE_PATH_EXPORT_TARGETS@")
]=])

set(PATH_EXPORT_TARGETS "${CMAKE_INSTALL_DATADIR}/blk-imgui-targets.cmake")
configure_package_config_file(
    "${CMAKE_BINARY_DIR}/blk-imgui-config.cmake.in"
    "${CMAKE_BINARY_DIR}/blk-imgui-config.cmake"
    PATH_VARS
        PATH_EXPORT_TARGETS
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}
)

write_basic_package_version_file("${CMAKE_BINARY_DIR}/blk-imgui-config-version.cmake"
  COMPATIBILITY AnyNewerVersion
)

##############################
##         Install          ##
##############################

install(
    TARGETS sample-imgui
    EXPORT blk-imgui-targets
    COMPONENT runtime
)

install(EXPORT blk-imgui-targets
    NAMESPACE blk-imgui::
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)

install(
    FILES
        "${CMAKE_BINARY_DIR}/blk-imgui-config.cmake"
        "${CMAKE_BINARY_DIR}/blk-imgui-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)

if(INSTALL_HEADERS)
    install(FILES
        ${PUBLIC_HEADERS}
        TYPE INCLUDE
    )
endif()
