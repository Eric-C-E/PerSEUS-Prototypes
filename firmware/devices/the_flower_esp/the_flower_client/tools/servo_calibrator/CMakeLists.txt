# Standalone ESP-IDF accessory app. This is intentionally outside the Flower
# firmware component tree and is not built into the production Flower client.
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(flower_servo_calibrator)
