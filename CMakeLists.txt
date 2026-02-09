cmake_minimum_required(VERSION 3.13)

option(BUILD_TESTS "Build unit tests." OFF)
option(ENABLE_COVERAGE "Enable coverage for unit tests" OFF)
option(ENABLE_WERROR "Treat warnings as errors for project targets" ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/system/app_init.c
    src/system/event_loop.c
    src/system/event_dispatch.c
    src/system/event_queue.c
    src/system/error.c
    src/system/cli_usb_cdc.c
    src/system/cli_commands.c
    src/system/cli_parser.c
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

set(STATIC_ANALYSIS_SOURCES
    src/main.c
    ${SOURCES}
)

# Get version from git
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT GIT_VERSION)
        set(GIT_VERSION "v0.1.0-dev")
    endif()
else()
    set(GIT_VERSION "v0.1.0-dev")
endif()

# Parse version components
string(REGEX MATCH "v([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${GIT_VERSION})
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})
set(VERSION_PATCH ${CMAKE_MATCH_3})

# Generate version.h
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
)

include_directories(${CMAKE_BINARY_DIR}/generated)


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

        # Core ioLibrary headers (SYSTEM: do not elevate vendor warnings to errors)
        target_include_directories(${target_name} SYSTEM PRIVATE
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

    if(ENABLE_WERROR)
        set(APP_WARNING_FLAGS
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            # WIZnet vendor headers declare static helpers without definitions in this TU.
            # Keep -Werror for project code but do not fail on this vendor-header pattern.
            -Wno-unused-function
        )
        set(APP_FIRMWARE_SOURCES
            src/main.c
            ${SOURCES}
        )
        set_source_files_properties(${APP_FIRMWARE_SOURCES}
            PROPERTIES COMPILE_OPTIONS "${APP_WARNING_FLAGS}"
        )
    endif()

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
        ${CMAKE_CURRENT_LIST_DIR}/pio/led_activity_mirror.pio
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

    # Core ioLibrary headers (SYSTEM: do not elevate vendor warnings to errors)
    target_include_directories(ex_w55_echo SYSTEM PRIVATE
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

    # Generate PIO header for WIZnet's QSPI PIO code.
    # Use a target-specific output dir to avoid duplicate custom-rule collisions
    # with the same header generated for ether2serial.
    set(EX_W55_ECHO_PIO_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/ex_w55_echo)
    pico_generate_pio_header(ex_w55_echo
        ${CMAKE_CURRENT_LIST_DIR}/external/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.pio
        OUTPUT_DIR ${EX_W55_ECHO_PIO_OUT_DIR}
    )
    target_include_directories(ex_w55_echo PRIVATE ${EX_W55_ECHO_PIO_OUT_DIR})

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
        tests/test_hdlc_bitstuff_encoder.c
        tests/test_command_parser.c
        tests/test_event_queue.c
        tests/test_byte_alignment.c
        tests/test_ringbuffer.c
        tests/test_log.c
        src/protocol/hdlc_common.c
        src/protocol/hdlc_encoder.c
        src/protocol/hdlc_decoder.c
        src/protocol/hdlc_sync.c
        src/system/cli_parser.c
        src/system/log.c
        src/system/event_queue.c
        src/system/ringbuffer.c
    )

    target_include_directories(unit_tests PRIVATE
        external/unity/src
        src
    )

    target_link_libraries(unit_tests unity)
    if(ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(unit_tests PRIVATE -O0 -g --coverage)
        target_link_options(unit_tests PRIVATE --coverage)
    endif()

    add_test(NAME unit_tests COMMAND unit_tests)
endif()


find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Configure Doxyfile with version
    configure_file(
        ${CMAKE_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_BINARY_DIR}/Doxyfile
        @ONLY
    )

    set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/docs/doxygen)
    set(DOXYGEN_HTML_DIR ${DOXYGEN_OUTPUT_DIR}/html)

    if(BUILD_TESTS AND ENABLE_COVERAGE)
        find_program(GCOVR_EXECUTABLE gcovr)
        if(GCOVR_EXECUTABLE)
            add_custom_target(docs
                COMMAND ${CMAKE_COMMAND} -E make_directory ${DOXYGEN_OUTPUT_DIR}
                COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
                COMMAND $<TARGET_FILE:unit_tests>
                COMMAND ${GCOVR_EXECUTABLE}
                        -r ${CMAKE_SOURCE_DIR}
                        --object-directory ${CMAKE_BINARY_DIR}
                        --exclude external/
                        --exclude tests/
                        --html-details
                        -o ${DOXYGEN_HTML_DIR}/coverage.html
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                DEPENDS unit_tests
                COMMENT "Generating API documentation + coverage report (${GIT_VERSION})"
                VERBATIM)
        else()
            message(WARNING "gcovr not found; docs target will be generated without coverage report.")
            add_custom_target(docs
                COMMAND ${CMAKE_COMMAND} -E make_directory ${DOXYGEN_OUTPUT_DIR}
                COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Generating API documentation (${GIT_VERSION})"
                VERBATIM)
        endif()
    else()
        add_custom_target(docs
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DOXYGEN_OUTPUT_DIR}
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation (${GIT_VERSION})"
            VERBATIM)
    endif()
endif()

# Static analysis targets
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(CLANG_TIDY_EXTRA_ARGS)
    find_program(ARM_NONE_EABI_GCC_EXE NAMES arm-none-eabi-gcc)
    if(ARM_NONE_EABI_GCC_EXE)
        execute_process(
            COMMAND ${ARM_NONE_EABI_GCC_EXE} -print-sysroot
            OUTPUT_VARIABLE ARM_NONE_EABI_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        list(APPEND CLANG_TIDY_EXTRA_ARGS --extra-arg=--target=arm-none-eabi)
        if(ARM_NONE_EABI_SYSROOT)
            list(APPEND CLANG_TIDY_EXTRA_ARGS --extra-arg=--sysroot=${ARM_NONE_EABI_SYSROOT})
        endif()

        execute_process(
            COMMAND ${ARM_NONE_EABI_GCC_EXE} -print-file-name=include
            OUTPUT_VARIABLE ARM_NONE_EABI_GCC_INCLUDE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        set(CLANG_TIDY_C_INCLUDE_DIRS)
        if(ARM_NONE_EABI_GCC_INCLUDE AND EXISTS "${ARM_NONE_EABI_GCC_INCLUDE}")
            list(APPEND CLANG_TIDY_C_INCLUDE_DIRS "${ARM_NONE_EABI_GCC_INCLUDE}")

            get_filename_component(ARM_NONE_EABI_GCC_INCLUDE_PARENT
                                   "${ARM_NONE_EABI_GCC_INCLUDE}" DIRECTORY)
            set(ARM_NONE_EABI_GCC_INCLUDE_FIXED
                "${ARM_NONE_EABI_GCC_INCLUDE_PARENT}/include-fixed")
            if(EXISTS "${ARM_NONE_EABI_GCC_INCLUDE_FIXED}")
                list(APPEND CLANG_TIDY_C_INCLUDE_DIRS "${ARM_NONE_EABI_GCC_INCLUDE_FIXED}")
            endif()
        endif()

        if(ARM_NONE_EABI_SYSROOT AND EXISTS "${ARM_NONE_EABI_SYSROOT}/include")
            list(APPEND CLANG_TIDY_C_INCLUDE_DIRS "${ARM_NONE_EABI_SYSROOT}/include")
        endif()

        # CI fallback include paths used by distro arm-none-eabi packages.
        if(EXISTS "/usr/arm-none-eabi/include")
            list(APPEND CLANG_TIDY_C_INCLUDE_DIRS "/usr/arm-none-eabi/include")
        endif()
        if(EXISTS "/usr/lib/arm-none-eabi/include")
            list(APPEND CLANG_TIDY_C_INCLUDE_DIRS "/usr/lib/arm-none-eabi/include")
        endif()
        file(GLOB ARM_NONE_EABI_GCC_INCLUDE_GLOB "/usr/lib/gcc/arm-none-eabi/*/include")
        list(APPEND CLANG_TIDY_C_INCLUDE_DIRS ${ARM_NONE_EABI_GCC_INCLUDE_GLOB})
        file(GLOB ARM_NONE_EABI_GCC_INCLUDE_FIXED_GLOB "/usr/lib/gcc/arm-none-eabi/*/include-fixed")
        list(APPEND CLANG_TIDY_C_INCLUDE_DIRS ${ARM_NONE_EABI_GCC_INCLUDE_FIXED_GLOB})

        list(REMOVE_DUPLICATES CLANG_TIDY_C_INCLUDE_DIRS)

        foreach(CLANG_TIDY_INC_DIR IN LISTS CLANG_TIDY_C_INCLUDE_DIRS)
            list(APPEND CLANG_TIDY_EXTRA_ARGS --extra-arg=-isystem
                                              --extra-arg=${CLANG_TIDY_INC_DIR})
        endforeach()
    endif()

    add_custom_target(check_clang_tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${CLANG_TIDY_EXTRA_ARGS}
                ${STATIC_ANALYSIS_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on project sources"
        VERBATIM
    )
else()
    add_custom_target(check_clang_tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-tidy is required for check_clang_tidy"
        VERBATIM
    )
endif()

find_program(CPPCHECK_EXE NAMES cppcheck)
if(CPPCHECK_EXE)
    add_custom_target(check_cppcheck
        COMMAND ${CPPCHECK_EXE}
            --enable=warning,style,performance,portability,information
            --inconclusive
            --check-level=exhaustive
            --std=c11
            --quiet
            -Isrc
            src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck on src/"
        VERBATIM
    )
else()
    add_custom_target(check_cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "cppcheck not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "cppcheck is required for check_cppcheck"
        VERBATIM
    )
endif()
