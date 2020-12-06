
add_executable(obj2mesh)

target_sources(obj2mesh
    PRIVATE
        obj2mesh.cpp
)

target_link_libraries(obj2mesh
    PRIVATE
        core
        range-v3::range-v3
)

add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/CornellBox-Original.mesh"
    COMMAND $<TARGET_FILE:obj2mesh>
        "${CMAKE_CURRENT_LIST_DIR}/CornellBox/CornellBox-Original.obj"
        -o "CornellBox/CornellBox-Original.mesh"
    DEPENDS
        obj2mesh.cpp
        "CornellBox/CornellBox-Original.obj"
    COMMENT
        "Baking .obj models into custom mesh formats..."
)

add_custom_target(baked_meshes
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CornellBox-Original.mesh"
)

add_dependencies(default-sample
    baked_meshes
)
