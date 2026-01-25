cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ether2serial C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(ether2serial
  src/main.c
)


target_link_libraries(ether2serial
  pico_stdlib
  hardware_pio
  hardware_spi
)

# Generate C header from the PIO program
pico_generate_pio_header(ether2serial ${CMAKE_CURRENT_LIST_DIR}/src/pio/led_blink.pio)



# USB CDC serial for your CLI
pico_enable_stdio_usb(ether2serial 1)
pico_enable_stdio_uart(ether2serial 0)

pico_add_extra_outputs(ether2serial)


# -------------------- Flash targets (picotool) --------------------
# Find picotool
find_program(PICOTOOL_EXE picotool)
if (NOT PICOTOOL_EXE)
  message(WARNING "picotool not found in PATH. 'flash' targets will not be available.")
else()

  add_custom_target(flash_elf
    COMMAND ${PICOTOOL_EXE} load -f $<TARGET_FILE:ether2serial>
    COMMAND ${CMAKE_COMMAND} -E sleep 1
    COMMAND ${PICOTOOL_EXE} reboot -f
    DEPENDS ether2serial
    USES_TERMINAL
    COMMENT "Flashing ELF to RP2040 using picotool (device must be in BOOTSEL mode, unless your firmware supports reboot-to-bootsel)."
  )

  add_custom_target(reboot
    COMMAND ${PICOTOOL_EXE} reboot
    USES_TERMINAL
    COMMENT "Reboot RP2040 via picotool (device must be accessible)."
  )
endif()
