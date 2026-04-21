# Flower Servo Calibrator

Standalone ESP-IDF accessory firmware for finding Flower servo values before
copying them into `main/board_config.h`.

This directory is not referenced by the Flower client `CMakeLists.txt`, so it is
not built into the production firmware.

From this directory, in an ESP-IDF-enabled terminal:

```powershell
idf.py set-target esp32s3
idf.py build flash monitor
```

The monitor prints built-in instructions. Use the serial commands to move the
tilt and rotation servos by pulse width, record safe mechanical limits, test
axis direction, then run `print` to generate `board_config.h` definitions.

Default pins match the current Flower firmware:

- Tilt: GPIO6
- Rotation: GPIO7

Change the `CAL_TILT_GPIO` and `CAL_ROTATION_GPIO` constants in
`main/servo_calibrator_main.c` if your wiring changes during testing.
