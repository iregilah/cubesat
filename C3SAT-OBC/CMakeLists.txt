# C3SAT-OBC — CubeSat On-Board Computer firmware
# Target: ESP32-C6-DevKitC-1-N8 (RISC-V, 8 MB flash)
# Framework: ESP-IDF (>= 5.2), which provides the FreeRTOS kernel.
#
# This is a standard ESP-IDF top-level CMake file. Open the project folder
# directly in CLion (with the ESP-IDF plugin) or build from the terminal:
#     idf.py set-target esp32c6
#     idf.py build flash monitor
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(c3sat_obc)
