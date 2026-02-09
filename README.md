# ether2ser
Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge firmware for RP2040 + W5500 (W55RP20-EVB-PICO).

**What It Does**
- Bridges UDP frames to synchronous V.24 via HDLC framing/deframing.
- Uses RP2040 PIO for clocked TX/RX.
- Provides a USB CDC CLI for configuration and status.
- Stores configuration in flash.

**Hardware**
- Target board: W55RP20-EVB-PICO (RP2040 + W5500).
- Ethernet required; V.24 wiring depends on your use case.
- PIO programs live in `pio/`.

**Repository Layout**
- `src/` firmware sources
- `src/system` app context, event loop/dispatch, CLI, config, logging
- `src/drivers` W5500, GPIO, TX/RX drivers
- `src/protocol` HDLC encode/decode/sync
- `src/examples` example firmware targets
- `external/` submodules (WIZnet-PICO-C, Unity)
- `pcb/` hardware design files

**Dependencies**
- Pico SDK (`PICO_SDK_PATH` set)
- ARM GCC toolchain (`arm-none-eabi-*`)
- CMake and Ninja/Make
- `picotool` for flash targets (optional but recommended)
- Submodules initialized

**Setup**
```bash
git submodule update --init --recursive
./setup_toolchain.sh
```
Note: `setup_toolchain.sh` is Arch Linux specific and uses `sudo`.

**Build (firmware)**
```bash
mkdir -p build
cmake -S . -B build
cmake --build build
```

**Flash**
- Use picotool targets if available:
```bash
cmake --build build --target flash_elf
cmake --build build --target flash_uf2
```
- Or flash individual targets:
```bash
cmake --build build --target flash_elf_ether2serial
cmake --build build --target flash_elf_ex_w55_echo
cmake --build build --target flash_elf_ex_blink_leds
```
- UF2 files are in `build/` after a successful build.

**Usage**
- Connect to the board’s USB CDC serial.
- Run `help` for available commands.
- Examples:
```text
set net ip 192.168.29.2/24
set net ip.remote 192.168.29.5
set net udp.port.local 6969
set v24 baudrate 9600
set v24 polarities txd,rxd,rts
```
- Defaults are defined in `src/drivers/w5500_driver.h`.

**Tests**
```bash
mkdir -p build-tests
cmake -S . -B build-tests -DBUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests
```

**Static Analysis**
```bash
cmake -S . -B build
cmake --build build --target check_clang_tidy
cmake --build build --target check_cppcheck
```
- `check_clang_tidy` requires `clang-tidy` in `PATH`.
- `check_cppcheck` requires `cppcheck` in `PATH`.

**Documentation (Doxygen)**
```bash
cmake -S . -B build
cmake --build build --target docs
```
- HTML entry point: `build/docs/doxygen/html/index.html`

**Coverage + Documentation**
```bash
cmake -S . -B build-tests -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON
cmake --build build-tests --target docs
```
- Doxygen HTML: `build-tests/docs/doxygen/html/index.html`
- Coverage report: `build-tests/docs/doxygen/html/coverage.html`
- In Doxygen landing page (`index.html`), use the **Related Pages** entry
  **Coverage Report** (or open `coverage.html` directly).

**License**
Apache-2.0 (see `LICENSE`).
