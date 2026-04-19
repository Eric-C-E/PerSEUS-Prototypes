# Generated Face Assets

This directory documents where converted LVGL C assets conceptually belong.

For ESP-IDF builds, place generated `.c` files under:

```text
main/assets/faces/generated/
```

Then add those `.c` files to `main/CMakeLists.txt` and register their exported
`lv_image_dsc_t` symbols in `main/face_assets.h` and `main/face_assets.c`.

Use LVGL 9 image descriptors:

`loading_screen.png`:

- `LV_COLOR_FORMAT_RGB565`
- 320x240

All other face frames:

- `LV_COLOR_FORMAT_I1`
- 320x240
- 1 bit per pixel
- palette index 0 = white
- palette index 1 = black
- source value 0 resolves to white
- source HIGH/nonzero resolves to black
