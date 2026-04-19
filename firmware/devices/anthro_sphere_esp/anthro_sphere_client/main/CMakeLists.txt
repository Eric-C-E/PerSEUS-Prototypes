file(GLOB generated_face_assets "assets/faces/generated/*.c")

idf_component_register(
    SRCS
        "app_main.c"
        "display_init.c"
        "face_assets.c"
        "face_fsm.c"
        "face_renderer.c"
        "ui_boot.c"
        ${generated_face_assets}
    INCLUDE_DIRS "."
)
