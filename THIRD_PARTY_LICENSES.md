# Third-Party Licenses

This file lists third-party software used by, vendored into, or referenced by this project.

Notes:
- This is an engineering inventory, not legal advice.
- For release/compliance, always ship the original license texts from the paths listed below.

## Direct dependencies in this repository

| Component | Usage | License | License file(s) |
|---|---|---|---|
| Unity (ThrowTheSwitch) | Unit test framework (`BUILD_TESTS=ON`) | MIT | `external/unity/LICENSE.txt` |
| WIZnet ioLibrary Driver | W5500 Ethernet stack (`wizchip_conf`, `socket`, `w5500`) | MIT-style permissive text | `external/WIZnet-PICO-C/libraries/ioLibrary_Driver/license.txt` |
| WIZnet RP2040 ioLibrary port files | PIO/SPI glue layer used by firmware | BSD-3-Clause (per SPDX headers) | Files under `external/WIZnet-PICO-C/port/ioLibrary_Driver/` (e.g. `src/wizchip_qspi_pio.c`, `src/wizchip_spi.c`) |

## SDK and transitive dependencies

These are commonly used through the Pico/WIZnet environment and may be present via submodules or external SDK path.

| Component | Typical usage in this project | License | License file(s) |
|---|---|---|---|
| Raspberry Pi Pico SDK | Core RP2040 SDK, stdio, hardware libs | BSD-3-Clause | `external/WIZnet-PICO-C/libraries/pico-sdk/LICENSE.TXT` (or your external `pico-sdk/LICENSE.TXT`) |
| TinyUSB | USB stack used by Pico stdio USB | MIT | `external/WIZnet-PICO-C/libraries/pico-sdk/lib/tinyusb/LICENSE` |
| lwIP | IP stack in Pico SDK tree (not primary stack for W5500 path) | BSD-3-Clause | `external/WIZnet-PICO-C/libraries/pico-sdk/lib/lwip/COPYING` |
| Mbed TLS | Crypto/TLS library present in SDK/WIZnet trees | Apache-2.0 OR GPL-2.0-or-later (dual) | `external/WIZnet-PICO-C/libraries/pico-sdk/lib/mbedtls/LICENSE`, `external/WIZnet-PICO-C/libraries/mbedtls/LICENSE` |

## Project license

This project itself is licensed under MIT:
- `LICENSE`

