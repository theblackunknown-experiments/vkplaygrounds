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
