cmake_minimum_required(VERSION 3.15)

project(vkplaygrounds
    VERSION 0.0.0
    DESCRIPTION "Playground for Vulkan"
    LANGUAGES CXX
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

message(VERBOSE "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(VERBOSE "CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")

set(CMAKE_CXX_STANDARD   20)
set(CMAKE_CXX_EXTENSIONS OFF)

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

find_package(Vulkan REQUIRED)
find_package(range-v3 REQUIRED)

##############################
##     Imported Targets     ##
##############################

add_library(dearimgui INTERFACE)

target_sources(dearimgui
    INTERFACE
        dearimgui/imconfig.h
        dearimgui/imgui.cpp
        dearimgui/imgui.h
        dearimgui/imgui_demo.cpp
        dearimgui/imgui_draw.cpp
        dearimgui/imgui_internal.h
        dearimgui/imgui_widgets.cpp
        dearimgui/imstb_rectpack.h
        dearimgui/imstb_textedit.h
        dearimgui/imstb_truetype.h
)

target_include_directories(dearimgui
    INTERFACE
        dearimgui
)

target_compile_definitions(dearimgui
    INTERFACE
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)

##############################
##         Targets          ##
##############################

add_library(utilities INTERFACE)

target_sources(utilities
    INTERFACE
        utilities/utilities.hpp
        utilities/ziprange.hpp
        utilities/enumeraterange.hpp
)

target_include_directories(utilities
    INTERFACE
        utilities
)

add_executable(default-sample WIN32)
target_sources(default-sample
    PRIVATE
        src/vkdebug.hpp
        src/vkdebug.cpp

        src/vkutilities.hpp
        src/vkutilities.cpp

        src/vkapplication.hpp
        src/vkapplication.cpp

        src/vkbuffer.hpp
        src/vkbuffer.cpp

        src/vkimage.hpp
        src/vkimage.cpp

        src/vkmemory.hpp
        src/vkmemory.cpp

        src/vkdevice.hpp
        src/vkdevice.cpp

        src/vkphysicaldevice.hpp
        src/vkphysicaldevice.cpp

        src/vkqueue.hpp
        src/vkqueue.cpp

        src/vkengine.hpp
        src/vkengine.cpp

        src/vkpresentation.hpp
        src/vkpresentation.cpp

        src/vkpass.hpp
        src/vkpass.cpp

        src/vkrenderpass.hpp
        src/vkrenderpass.cpp

        src/vksurface.hpp
        $<$<PLATFORM_ID:Windows>:src/win32_vksurface.cpp>

        src/sample0/vksample0.hpp
        src/sample0/vksample0.cpp

        src/sample0/vkpassuioverlay.hpp
        src/sample0/vkpassuioverlay.cpp

        src/sample0/vkpassscene.hpp
        src/sample0/vkpassscene.cpp

        # src/dearimgui/dearimguishowcase.hpp
        # src/dearimgui/dearimguishowcase.cpp

        $<$<PLATFORM_ID:Windows>:src/win32_main.cpp>
        $<$<PLATFORM_ID:Windows>:src/win32_vkdebug.cpp>
)

target_include_directories(default-sample
    PRIVATE
        src
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(default-sample
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            VK_USE_PLATFORM_WIN32_KHR
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            NOCOMM
        >
        $<$<PLATFORM_ID:Windows>:OS_WINDOWS>
)
target_link_libraries(default-sample
    PRIVATE
        dearimgui
        utilities
        Vulkan::Vulkan
        range-v3::range-v3
)

add_subdirectory(fonts)
add_subdirectory(shaders)

##############################
##          Config          ##
##############################

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vkplaygrounds-config.cmake.in" [=[
    @PACKAGE_INIT@
    include("@PACKAGE_PATH_EXPORT_TARGETS@")
]=])

set(PATH_EXPORT_TARGETS "${CMAKE_INSTALL_DATADIR}/vkplaygrounds-targets.cmake")
configure_package_config_file(
    "${CMAKE_CURRENT_BINARY_DIR}/vkplaygrounds-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vkplaygrounds-config.cmake"
    PATH_VARS
        PATH_EXPORT_TARGETS
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
)

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/vkplaygrounds-config-version.cmake"
  COMPATIBILITY AnyNewerVersion
)

##############################
##         Install          ##
##############################

install(
    TARGETS default-sample
    EXPORT vkplaygrounds-targets
    COMPONENT runtime
)

install(EXPORT vkplaygrounds-targets
    NAMESPACE vkplaygrounds::
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/vkplaygrounds-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/vkplaygrounds-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)

if(INSTALL_HEADERS)
    install(FILES
        ${PUBLIC_HEADERS}
        TYPE INCLUDE
    )
endif()

##############################
##          Fixes           ##
##############################

# https://gitlab.kitware.com/cmake/cmake/-/issues/18399#note_459496
set_property(
    SOURCE
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/ui-shader.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle-shader.hpp
    PROPERTY
        GENERATED 1
)

set_property(
    SOURCE
        ${CMAKE_CURRENT_BINARY_DIR}/fonts/font.hpp
    PROPERTY
        GENERATED 1
)
