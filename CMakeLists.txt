cmake_minimum_required(VERSION 3.13)

option(BUILD_TESTS "Build unit tests." OFF)

set(SOURCES
    src/system/app_init.c
    src/system/event_queue.c
    src/system/cli_usb_cdc.c
    src/system/cli_commands.c
    src/system/cli_parser.c
    src/system/ringbuffer.c
    src/system/ringbuffer.c
    src/system/log.c
    src/system/baudrate_monitor.c
    src/system/persistent_config.c
    src/drivers/w5500_driver.c
    src/drivers/pio_tx_rx_driver.c
    src/drivers/tx_queue.c
    src/drivers/gpio_driver.c
    src/protocol/hdlc_common.c
    src/protocol/hdlc_encoder.c
    src/protocol/hdlc_decoder.c
    src/protocol/hdlc_sync.c
)

if(NOT BUILD_TESTS)
    # WIZnet chip configuration for W55RP20-EVB-PICO
    # This board has an integrated W5500 connected via internal PIO-based SPI
    set(WIZNET_CHIP W5500)
    add_definitions(-D_WIZCHIP_=W5500)
    add_definitions(-DDEVICE_BOARD_NAME=W55RP20_EVB_PICO)

    # W55RP20 internal SiP uses standard SPI (QSPI_SINGLE_MODE), not quad SPI
    # This configures the PIO-based SPI driver in the WIZnet port layer
    set(WIZCHIP_QSPI_MODE QSPI_SINGLE_MODE)
    add_definitions(-D_WIZCHIP_QSPI_MODE_=${WIZCHIP_QSPI_MODE})

    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

    project(ether2serial C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    pico_sdk_init()

    # Tools
    find_program(PICOTOOL_EXE picotool)
    if (NOT PICOTOOL_EXE)
    message(WARNING "picotool not found in PATH. Flash targets will not be available.")
    endif()

    # Helper: common Pico target defaults
    function(configure_pico_target target_name)
        target_link_libraries(${target_name}
            pico_stdlib
            hardware_gpio
            hardware_spi
            hardware_dma
        )

        target_include_directories(${target_name} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/src
        )

        # Core ioLibrary headers
        target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W5500
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/inc
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port
        )

        # Core ioLibrary sources + WIZnet port layer for W55RP20
        target_sources(${target_name} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/wizchip_conf.c
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/socket.c
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W5500/w5500.c
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_spi.c
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.c
        )

        # USB CDC serial (minicom) for CLI/printf
        pico_enable_stdio_usb(${target_name} 1)
        pico_enable_stdio_uart(${target_name} 0)
        pico_add_extra_outputs(${target_name})

        # Per-target flash helpers
        if (PICOTOOL_EXE)
            add_custom_target(flash_elf_${target_name}
            COMMAND ${PICOTOOL_EXE} load -f $<TARGET_FILE:${target_name}>
            COMMAND ${CMAKE_COMMAND} -E sleep 1
            COMMAND ${PICOTOOL_EXE} reboot -f
            DEPENDS ${target_name}
            USES_TERMINAL
            COMMENT "Flashing ELF for ${target_name} using picotool."
            )

            add_custom_target(flash_uf2_${target_name}
            COMMAND ${PICOTOOL_EXE} load -f $<TARGET_FILE_DIR:${target_name}>/$<TARGET_FILE_BASE_NAME:${target_name}>.uf2
            COMMAND ${CMAKE_COMMAND} -E sleep 1
            COMMAND ${PICOTOOL_EXE} reboot -f
            DEPENDS ${target_name}
            USES_TERMINAL
            COMMENT "Flashing UF2 for ${target_name} using picotool."
            )
        endif()
    endfunction()

    # Main firmware target
    add_executable(ether2serial
        src/main.c
        ${SOURCES}
    )
    # Generate PIO header for WIZnet's QSPI PIO code
    pico_generate_pio_header(ether2serial
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.pio
    )

    target_link_libraries(ether2serial
        hardware_pio
        hardware_spi
        hardware_dma
    )

    # Generate C header from the PIO program(s)
    pico_generate_pio_header(ether2serial
        ${CMAKE_CURRENT_LIST_DIR}/pio/tx_clock.pio
        ${CMAKE_CURRENT_LIST_DIR}/pio/tck_txd.pio
        ${CMAKE_CURRENT_LIST_DIR}/pio/rck_rxd.pio
    )

    configure_pico_target(ether2serial)

    if (PICOTOOL_EXE)
    add_custom_target(flash_elf
        DEPENDS flash_elf_ether2serial
    )
    add_custom_target(flash_uf2
        DEPENDS flash_uf2_ether2serial
    )
    add_custom_target(reboot
        COMMAND ${PICOTOOL_EXE} reboot
        USES_TERMINAL
        COMMENT "Reboot RP2040 via picotool."
    )
    endif()

    # Examples
    add_executable(ex_blink_leds
        src/examples/blink/blink_pio.c
        src/examples/blink/blink_cpu.c
        src/examples/blink/main.c
    )

    target_link_libraries(ex_blink_leds
        hardware_pio
    )

    pico_generate_pio_header(ex_blink_leds
        ${CMAKE_CURRENT_LIST_DIR}/pio/led_blink.pio
    )

    configure_pico_target(ex_blink_leds)

    # W5500 Ethernet initialization example for W55RP20-EVB-PICO
    # Demonstrates PIO-based SPI communication with internal W5500
    # NOTE: Requires Ethernet cable connected for PHY link establishment
    add_executable(ex_w55_echo
        src/examples/w55_udp_echo/main.c
    )

    # Core ioLibrary headers
    target_include_directories(ex_w55_echo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W5500
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/inc
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port
    )

    # Core ioLibrary sources + WIZnet port layer for W55RP20
    target_sources(ex_w55_echo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/wizchip_conf.c
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/socket.c
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W5500/w5500.c
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.c
    )

    # Generate PIO header for WIZnet's QSPI PIO code
    pico_generate_pio_header(ex_w55_echo
    ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.pio
    )

    target_link_libraries(ex_w55_echo
    hardware_pio
    hardware_dma
    )

    configure_pico_target(ex_w55_echo)
else()
    project(ether2serial_tests C)
    set(CMAKE_C_STANDARD 11)

    enable_testing()
    add_library(unity
        external/unity/src/unity.c
    )

    target_include_directories(unity
        PUBLIC external/unity/src
    )

    add_executable(unit_tests
        tests/test_runner.c
        tests/test_hdlc_encoder.c
        tests/test_hdlc_decoder.c
        tests/test_command_parser.c
        tests/test_event_queue.c
        tests/test_byte_alignment.c
        src/protocol/hdlc_common.c
        src/protocol/hdlc_encoder.c
        src/protocol/hdlc_decoder.c
        src/protocol/hdlc_sync.c
        src/system/cli_parser.c
        src/system/event_queue.c
    )

    target_include_directories(unit_tests PRIVATE
        external/unity/src
        src
    )

    target_link_libraries(unit_tests unity)

    add_test(NAME unit_tests COMMAND unit_tests)
endif()
