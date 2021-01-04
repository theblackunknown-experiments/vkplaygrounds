
##############################
##         Targets          ##
##############################

add_library(libobj2mesh)

target_sources(libobj2mesh
    PUBLIC
        obj2mesh.hpp
    PRIVATE
        obj2mesh.cpp
        obj2mesh_v0.cpp
)

target_include_directories(libobj2mesh
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(libobj2mesh
    PUBLIC
        core
        scene
    PRIVATE
        tinyobjloader::tinyobjloader
)

generate_export_header(libobj2mesh
    BASE_NAME
        obj2mesh
)

add_executable(obj2mesh)

target_sources(obj2mesh
    PRIVATE
        main_obj2mesh.cpp
)

target_link_libraries(obj2mesh
    PRIVATE
        libobj2mesh
        range-v3::range-v3
)
