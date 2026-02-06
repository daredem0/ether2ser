# Software Design & Architecture Description (SDD/SAD)

## 1. Purpose and Scope
This document describes the software design and architecture of **ether2ser**, a firmware project that bridges Ethernet UDP traffic to synchronous V.24 (RS‑232/V.28) signaling using an RP2040 with an integrated W5500 (W55RP20‑EVB‑PICO). It covers the firmware structure found in `src/`, the major modules, runtime behavior, data flow, and design constraints. Hardware‑dependent behavior is described at the interface level.

## 2. System Overview
**ether2ser** is a single‑process, event‑driven firmware that:
- Receives UDP frames over Ethernet (W5500), HDLC‑encodes them, and transmits over a synchronous V.24 link driven by RP2040 PIO.
- Receives synchronous V.24 bytes, reconstructs HDLC frames with bit‑alignment recovery, decodes them, and transmits the payload via UDP.
- Exposes a USB CDC CLI for configuration and diagnostics.
- Persists configuration to on‑chip flash.

The architecture is intentionally simple: a single main loop polls inputs, queues events, and dispatches handlers.

## 3. Hardware and Platform Context
- **Target board**: W55RP20‑EVB‑PICO (RP2040 + W5500).
- **Ethernet**: W5500 hardware offload, accessed through WIZnet ioLibrary via PIO‑based SPI.
- **Synchronous V.24**: PIO programs generate TX clock/data and sample RX clock/data.
- **USB CDC**: CLI and logging via `stdio` over USB.
- **Persistent storage**: last 4 KB sector of flash.

Pin assignments live in `src/platform/pinmap.h` (and `src/system/board_pins.h` provides a generic mapping template).

## 4. High‑Level Architecture
```
+--------------------+        +----------------------+
| USB CDC CLI        |        | Ethernet (W5500)     |
| - cli_usb_cdc      |        | - w5500_driver       |
+---------+----------+        +----------+-----------+
          |                               |
          v                               v
      Event Queue <------------------- Event Queue
          |                               |
          v                               v
+---------+-------------------------------+---------+
|                 Event Dispatch                   |
| (event_dispatch.c)                               |
+---------+-------------------------------+---------+
          |                               |
          v                               v
+---------+-----------+        +----------+----------+
| V.24 / PIO TX        |        | HDLC Sync/Decode   |
| - tx_queue / pio     |        | - hdlc_sync, decode|
+---------------------+        +---------------------+
```

The **main loop** (event loop) is responsible for:
- Polling USB CLI and Ethernet RX
- Draining the TX queue to PIO
- Capturing RX bytes and assembling HDLC frames
- Dispatching a limited number of queued events per iteration

## 5. Runtime Model
### 5.1 Startup Sequence
1. Initialize USB CDC.
2. Initialize W5500 driver and GPIOs.
3. Read persistent config (if valid) or fall back to defaults.
4. Initialize app context (`init_app`).
5. Open UDP socket, initialize TX queue, configure PIO for V.24.
6. Initialize event queue and enter the event loop.

### 5.2 Event Loop
The loop is a single, tight polling loop:
- `cli_poll()` reads USB CDC and enqueues `EV_CLI_LINE`.
- `w5500_poll_rx()` reads UDP and enqueues `EV_UDP_RX`.
- `tx_queue_drain()` sends HDLC‑encoded bytes through PIO.
- `rx_get()` reads a byte from RX PIO and feeds the HDLC accumulator.
- `hdlc_sync_acc_poll()` emits an `EV_HDLC_DECODE` when a full HDLC frame is recovered.
- A small fixed number of events (currently 2) are popped and dispatched each iteration.

### 5.3 Event Dispatch
`event_dispatch()` is responsible for the business logic, including:
- CLI execution
- Network settings updates
- V.24 settings updates
- Config persistence
- UDP transmit after HDLC decode

## 6. Core Data Structures
### 6.1 `app_ctx_t` (System State)
Holds all runtime state and buffers:
- Configuration (persistent + current)
- UDP config (local, remote, sender)
- V.24 config and polarities
- RX/TX frame buffers
- HDLC sync accumulator
- TX queue ring buffer

This context is passed into the event loop and dispatcher to avoid globals.

### 6.2 `event_t`
Event payload container used by the queue:
- `type` defines the action (`EV_*`).
- `data` is a tagged union: inline bytes or a pointer.
- `data_len` and `is_inline` are used for validation.

Helper `event_get_payload_ptr()` provides safe access to inline or pointer payloads.

### 6.3 `event_queue_data_t`
A small typed payload for settings events (network/V.24). The payload encodes the setting ID and its value (IP, port, baudrate, polarities).

### 6.4 `HDLC_FRAME_T` and `UDP_FRAME_T`
- `HDLC_FRAME_T` is an encoded frame with `payload`, `length`, and `capacity`.
- `UDP_FRAME_T` holds raw UDP payload and length.

### 6.5 `TX_QUEUE_ENTRY_T`
Stores one HDLC‑encoded frame for serial transmission, with a local payload buffer and an offset for partial draining.

## 7. Module‑Level Design
### 7.1 System Modules (`src/system`)
- **event_loop.c**: Orchestrates polling and event dispatch; maintains loop timing.
- **event_dispatch.c**: Dispatches all events and applies side effects.
- **event_queue.c**: Fixed‑size ring queue for events (capacity 16).
- **cli_usb_cdc.c**: Reads USB CDC; creates line events with a small pool of buffers.
- **cli_parser.c**: Parses CLI commands, IPs, ports, and V.24 arguments.
- **cli_commands.c**: CLI command handlers; uses event queue to apply settings.
- **persistent_config.c**: Reads/writes configuration to flash (last sector).
- **baudrate_monitor.c**: Estimates RXC baudrate using GPIO edge interrupts.
- **ringbuffer.c**: Generic ring buffer used by the TX queue.

### 7.2 Driver Modules (`src/drivers`)
- **w5500_driver.c**: W5500 init, UDP socket management, RX/TX.
- **pio_tx_rx_driver.c**: PIO TX/RX clock/data setup, byte I/O, RTS/CTS handling.
- **tx_queue.c**: HDLC encoding and buffered transmission to PIO.
- **gpio_driver.c**: Pin initialization and default polarity settings.

### 7.3 Protocol Modules (`src/protocol`)
- **hdlc_encoder/decoder**: Adds/removes flags, escape sequences, and CRC16.
- **hdlc_sync**: Bit‑alignment recovery and frame detection.
- **hdlc_common**: CRC16 implementation and constants.

## 8. Data Flow
### 8.1 UDP → V.24
1. `w5500_poll_rx()` receives UDP payload into `rx_frame_buffer`.
2. Enqueues `EV_UDP_RX`.
3. `event_dispatch()` handles `EV_UDP_RX`:
   - Calls `tx_queue_enqueue_udp_frame()`.
4. `tx_queue_enqueue_udp_frame()`:
   - HDLC‑encodes payload into a `TX_QUEUE_ENTRY_T`.
   - Pushes into ring buffer.
5. `tx_queue_drain()` feeds bytes via `tx_put()` to PIO.

### 8.2 V.24 → UDP
1. `rx_get()` reads PIO data bytes.
2. `hdlc_sync_acc_process_byte()` accumulates bytes.
3. `hdlc_sync_acc_poll()` detects a full frame and writes to `reconstructed_frame`.
4. `EV_HDLC_DECODE` is queued with a pointer to the reconstructed frame.
5. `event_dispatch()` handles `EV_HDLC_DECODE`:
   - Runs `hdlc_decode()`, checks CRC, and writes payload into `tx_frame_buffer`.
   - Enqueues `EV_UDP_TX`.
6. `EV_UDP_TX` triggers `w5500_udp_tx()`.

## 9. Event Model
### 9.1 Event Types
- `EV_CLI_LINE`: A CLI line string from USB CDC.
- `EV_UDP_RX`: UDP frame ready for HDLC encoding.
- `EV_UDP_TX`: UDP frame ready for transmission.
- `EV_HDLC_DECODE`: HDLC frame ready for decode.
- `EV_SAVE_CONFIG` / `EV_WIPE_CONFIG`: persistence actions.
- `EV_SET_*` / `EV_GET_*`: configuration operations.

### 9.2 Payload Handling
- **Pointer payloads**: large buffers; caller owns lifetime.
- **Inline payloads**: small `event_queue_data_t` structures copied into the event.

`event_get_payload_ptr()` validates size and returns a payload pointer regardless of inline/pointer storage.

## 10. CLI Design
### 10.1 CLI Flow
1. `cli_usb_cdc` reads input and enqueues `EV_CLI_LINE`.
2. `event_dispatch()` calls `handle_cli_line()`.
3. `cli_parser` parses the command and arguments.
4. `cli_commands` executes and often enqueues config events.

### 10.2 Command Categories
- `help`, `status`, `net`, `set`, `get`, `pininfo`, `save`, `wipe`, `reboot`
- `set/get gpio`, `set/get net`, `set/get v24` subcommands

CLI commands are designed to be asynchronous: they emit events rather than mutate hardware directly where possible.

## 11. Configuration Persistence
- Stored in the last flash sector (4 KB).
- `config_t` contains network, V.24, and log settings.
- Validity is checked via a magic constant (`0xCAFEBABE`).
- `EV_SAVE_CONFIG` writes the config; `EV_WIPE_CONFIG` clears it.

## 12. V.24 / PIO Design
- TX uses a PIO program (`tck_txd`) to generate clock and serialize bytes.
- RX uses a PIO program (`rck_rxd`) to sample data with the RX clock.
- `tx_put()` writes bytes to PIO; `tx_poll()` controls RTS based on PIO stall status.
- `rx_get()` checks RX FIFO and returns a byte if available.

## 13. HDLC Protocol Handling
### 13.1 Encoding
- Adds start/end flags (`0x7E`), escapes `0x7E` and `0x7D` bytes.
- Appends CRC16‑CCITT (false) to payload.

### 13.2 Decoding
- Validates leading/trailing flags.
- Unescapes escaped bytes.
- Verifies CRC16 and returns payload length.

### 13.3 Sync / Bit Alignment
- `hdlc_sync_acc` can align frames that are bit‑shifted relative to byte boundaries.
- State machine: `HUNTING → SYNCING → SYNCED`.
- A full frame is returned when a closing flag is detected.

## 14. Error Handling & Logging
- Error codes are centralized in `system/error.h`.
- Logging uses `LOG_INFO` and `LOG_DEBUG` macros with a global `current_log_level`.
- Some modules print directly to `printf()` for diagnostics.

## 15. Build & Test Architecture
- Firmware build uses Pico SDK and WIZnet ioLibrary.
- Unit tests are built when `BUILD_TESTS=ON` and avoid hardware‑dependent modules.
- Existing tests cover HDLC, CLI parsing, event queue, ring buffer, and HDLC sync alignment.

## 16. Constraints and Trade‑offs
- **Single‑threaded loop**: easy to reason about but can backlog if handlers are slow.
- **Fixed queues/buffers**: deterministic memory, but limited capacity.
- **Immediate config save**: simple but flash‑wear heavy (noted in code).
- **No dynamic allocation**: avoids fragmentation and simplifies embedded reliability.

## 17. Known Limitations and Risks
- Event queue capacity is limited (16). Bursts may drop events.
- HDLC sync accumulator does not enforce output buffer capacity in `hdlc_sync_acc_poll`.
- Saving config on every settings change impacts flash lifetime.
- RX/TX loops are polling‑based; latency depends on loop timing.

## 18. Future Improvements (Non‑Binding)
- Debounce or batch config saves to reduce flash wear.
- Add capacity checks in HDLC sync output path.
- Add non‑blocking or prioritized event handling for time‑critical paths.
- Improve test coverage for dispatcher logic (requires hardware abstraction or stubs).

---

This document reflects the architecture as implemented in `src/` at the time of writing and is intended as a baseline for a future, more formal SDD.
