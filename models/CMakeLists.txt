
add_custom_command(OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/CornellBox-Original.mesh"
    COMMAND $<TARGET_FILE:obj2mesh>
        "${CMAKE_CURRENT_LIST_DIR}/CornellBox/CornellBox-Original.obj"
        -o "CornellBox/CornellBox-Original.mesh"
    DEPENDS
        "CornellBox/CornellBox-Original.obj"
    COMMENT
        "Baking .obj models into custom mesh formats..."
)

add_custom_target(baked_meshes
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CornellBox-Original.mesh"
)
