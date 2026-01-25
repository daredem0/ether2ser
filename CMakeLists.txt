cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ether2serial C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(ether2serial
  src/main.c
)

target_link_libraries(ether2serial
  pico_stdlib
  hardware_pio
  hardware_spi
)

# USB CDC serial for your CLI
pico_enable_stdio_usb(ether2serial 1)
pico_enable_stdio_uart(ether2serial 0)

pico_add_extra_outputs(ether2serial)
