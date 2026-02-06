# ether2ser
Ethernet ↔ synchronous V.24 (RS-232/V.28) IP router using RP2040 + W5500.

## Setup
execute `setup_toolchain.sh`

## Building
```
mkdir build && cd build
cmake ..
cmake --build .
```

## Flashing
```
cmake --build . --target help | grep flash
cmake --build . --target flash_elf_ether2serial
cmake --build . --target flash_elf_ex_blink_leds
cmake --build . --target flash_elf_ex_w55_echo
```
## Test
```
mkdir build-test && cd build-test
cmake -DBUILD_TESTS=ON ..
cmake --build .
```
