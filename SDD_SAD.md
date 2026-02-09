# Software Design and Architecture Description (SDD/SAD)

## 1. Purpose
This document specifies the current software design and architecture of `ether2ser` as implemented in `src/`.

Scope:
- Runtime architecture and module decomposition
- Data and control flows
- Protocol handling (UDP, HDLC, synchronous V.24)
- Configuration, persistence, logging, and diagnostics
- Build and test structure

## 2. System Context
`ether2ser` runs on W55RP20-EVB-PICO (RP2040 + W5500) and bridges:
- Ethernet UDP datagrams
- Synchronous V.24 serial bitstream

Functional behavior:
- UDP payload -> HDLC bit-stuffed frame -> serial TX
- Serial RX bitstream -> HDLC sync + decode -> UDP payload TX
- USB CDC CLI for runtime configuration and status
- Persistent flash-backed configuration

## 3. Architectural Style
The firmware is a single-threaded polling system with one primary runtime loop.

Design decisions:
- Deterministic control flow without RTOS scheduling
- Static memory allocation for all runtime paths
- Direct high-throughput data path in loop body
- Event queue for command/configuration control-plane actions

## 4. Runtime Architecture
### 4.1 Initialization Path (`src/main.c`)
Startup order:
1. Initialize USB stdio and wait for host visibility.
2. Initialize W5500 and board I/O direction defaults.
3. Load persistent configuration (or defaults if invalid).
4. Build and initialize `app_ctx_t`.
5. Configure UDP socket and TX queue.
6. Initialize PIO TX/RX engines and baudrate monitor.
7. Enter `event_loop(app)`.

### 4.2 Main Loop (`src/system/event_loop.c`)
Loop cadence:
- Poll CLI input (`cli_poll`) and enqueue CLI events.
- Poll UDP RX (`w5500_poll_rx`) and enqueue HDLC TX frames.
- Drain TX queue into PIO TX FIFO (`tx_queue_drain`).
- Run TX completion/RTS holdoff logic (`tx_poll`) when queue is empty.
- Drain RX FIFO bytes into HDLC sync accumulator.
- Poll accumulator for complete frames and decode/forward immediately.
- Mirror sync internals into runtime statistics.
- Dispatch up to 20 queued events each iteration.
- Render CLI prompt when output occurred.
- Sleep `50 us`.

## 5. Module Decomposition
### 5.1 Application Context (`src/system/app_context.h`)
`app_ctx_t` is the central state container:
- Runtime config mirrors (`local`, `destination`, `sender`, `net`, `v24`)
- Persistent config snapshot
- UDP RX/TX buffers
- HDLC reconstructed frame buffer
- HDLC sync accumulator
- TX queue instance and backing storage
- Runtime statistics (`payload_statistics_t`)
- CLI prompt state

### 5.2 Control Plane
- `event_queue.*`: bounded ring-based event transport
- `event_dispatch.c`: handlers for config/status/CLI events
- `cli_usb_cdc.c`: USB line I/O and line buffering
- `cli_parser.c` + `cli_commands.c`: command parsing and command execution

### 5.3 Data Plane
- UDP ingress/egress: `drivers/w5500_driver.c`
- HDLC encode/decode/sync: `protocol/hdlc_*.c`
- Serial TX/RX engines: `drivers/pio_tx_rx_driver.c`
- Serial TX buffering: `drivers/tx_queue.c`

### 5.4 Persistence and Logging
- Flash config read/write/wipe: `system/persistent_config.*`
- Logging runtime and API: `system/log.c`, `system/common.h`

## 6. Data Flow Design
### 6.1 UDP to Serial
1. `w5500_poll_rx` receives UDP payload to `app->rx_frame_buffer`.
2. `tx_queue_enqueue_udp_frame` calls `hdlc_encode(..., lsb_first=true)`.
3. Encoded frame enters TX ring queue as `TX_QUEUE_ENTRY_T`.
4. `tx_queue_drain` pushes bytes to PIO via `tx_put`.
5. TX clock/data state machine emits synchronous serial data.

### 6.2 Serial to UDP
1. `rx_get` drains bytes from RX PIO FIFO.
2. `hdlc_sync_acc_process_byte` appends into sync accumulator.
3. `hdlc_sync_acc_poll` detects an aligned HDLC frame candidate.
4. `hdlc_decode(..., lsb_first=true)` performs unstuff + CRC check.
5. On decode success, `w5500_udp_tx` forwards payload.
6. Candidate window is consumed with `hdlc_sync_acc_consume_candidate`.

## 7. Event System Design
### 7.1 Queue Model (`src/system/event_queue.h`)
- Capacity: `EVENT_QUEUE_CAPACITY = 16`
- Inline payload: `bytes[16]` for small objects
- Pointer payload: external storage with explicit length
- Access helper: `event_get_payload_ptr(event, required_size, out)`

### 7.2 Event Categories
Used event types include:
- CLI and status: `EV_CLI_LINE`, `EV_STATUS`
- Config persistence: `EV_SAVE_CONFIG`, `EV_WIPE_CONFIG`
- Runtime settings: `EV_SET_NET_SETTINGS`, `EV_GET_NET_SETTINGS`, `EV_SET_V24_SETTINGS`, `EV_GET_V24_SETTINGS`
- Data-path event ids exist in enum (`EV_UDP_RX`, `EV_UDP_TX`, `EV_HDLC_DECODE`) and remain supported by dispatcher paths.

## 8. Protocol Design
### 8.1 HDLC Encoder (`src/protocol/hdlc_encoder.c`)
`hdlc_encode` behavior:
- Adds start/end flag bytes
- Emits payload and CRC bitwise
- Applies bit stuffing (insert `0` after five consecutive `1` bits)
- Supports configurable bit order (`lsb_first`)
- Produces byte-packed frame buffer

Secondary API:
- `hdlc_encode_byte` is present for byte-escaped framing compatibility/tests.

### 8.2 HDLC Synchronizer (`src/protocol/hdlc_sync.c`)
Accumulator behavior:
- States: `HUNTING`, `SYNCING`, `SYNCED`
- Searches for sync byte across bit offsets
- Builds aligned candidate frames in output buffer
- Tracks candidate window and consumes it explicitly
- Maintains bounded memory with hard-cap drop logic near buffer limits

### 8.3 HDLC Decoder (`src/protocol/hdlc_decoder.c`)
`hdlc_decode` behavior:
- Validates frame envelope
- Extracts bits with unstuffing
- Reassembles payload bytes
- Verifies CRC16
- Supports configurable bit order (`lsb_first`)

Secondary API:
- `hdlc_decode_byte` is present for compatibility/tests.

## 9. Driver Design
### 9.1 W5500 Driver (`src/drivers/w5500_driver.c`)
- UDP socket configuration and non-blocking poll model
- RX payload extraction and TX send API
- Runtime network reconfiguration support

### 9.2 PIO TX/RX Driver (`src/drivers/pio_tx_rx_driver.c`)
- TX and RX state machines on RP2040 PIO
- TX path manages RTS assertion/deassertion lifecycle
- TX poll applies configurable holdoff (`tx_rts_holdoff_us`) before releasing RTS after FIFO drain/stall
- End-of-burst behavior explicitly forces TX clock (`V24_TXC_DTE`) low in GPIO mode and then returns pin control to the configured PIO function
- Runtime clock/polarity update APIs used by V.24 config events
- Driver stores runtime-selected PIO instance and SM indices for both TX and RX paths (`tx_pio/tx_sm`, `rx_pio/rx_sm`) and uses them in FIFO access and update operations
- Optional instrumentation reports SM stall conditions

### 9.3 TX Queue (`src/drivers/tx_queue.c`)
- Queue depth: `TX_FRAME_QUEUE_SIZE = 32`
- Entry storage: `payload[4000]`, `HDLC_FRAME_T`, per-entry `offset`
- Tracks serialized bytes written on wire (`tx_wire_bytes`)
- Emits queue health diagnostics

## 10. Configuration and Persistence
### 10.1 Persistent Model (`src/system/persistent_config.*`)
- Flash-backed `config_t` with magic/version validation
- Includes network, UDP endpoint, V.24, and loglevel settings

### 10.2 Runtime Update Policy
- `set net ...` and `set v24 ...` paths apply runtime updates
- Save request is queued after setting changes
- `save` forces a write; `wipe` clears stored config

## 11. CLI Design
### 11.1 Commands
Implemented top-level commands include:
- `help`, `status`, `net`, `set`, `get`, `pininfo`, `save`, `wipe`, `reboot`

### 11.2 Categories
Supported configuration categories:
- `gpio`, `net`, `v24`, `loglevel`

### 11.3 Loglevel Interface
- `set loglevel <error|info|debug|trace>`
- `get loglevel`
- Alias `tracea` is accepted and mapped to `TRACE`.

## 12. Logging and Diagnostics
### 12.1 Logging API
`LOG_ERROR`, `LOG_INFO`, `LOG_DEBUG`, `LOG_TRACE` route through `log_write` and runtime level filtering.

### 12.2 Prompt Coordination
`log_take_emitted_flag()` indicates whether output occurred and triggers prompt redraw handling in loop logic.

### 12.3 Status Snapshot (`EV_STATUS`)
Status output includes:
- RX/TX frame counters and decode counters
- Pipeline gap counters
- TX queue usage and active transmission state
- Serial RX bytes and serialized TX wire bytes
- Sync wait/maintenance counters
- Accumulator internals and reconstructed length
- Active TX SM stall bit (derived from runtime-selected TX PIO/SM)

## 13. Build and Test Architecture
### 13.1 Firmware Build
Defined in `CMakeLists.txt`:
- Target: `ether2serial`
- Pico SDK + WIZnet integration
- PIO header generation for:
  - `pio/tx_clock.pio`
  - `pio/tck_txd.pio`
  - `pio/rck_rxd.pio`
  - `pio/led_activity_mirror.pio`

### 13.2 Unit Tests
When `BUILD_TESTS=ON`, Unity-based tests compile into `unit_tests`.
Coverage includes:
- HDLC common/encoder/decoder/sync
- Bitstuff encoder/decoder test suites
- CLI parser
- Event queue
- Ringbuffer

## 14. Operational Constraints
- Throughput and latency are bounded by polling loop service rate and configured serial baudrate.
- Event queue and TX queue are fixed-size; sustained ingress beyond service capacity causes backpressure or drops.
- Sync accumulator is bounded and may drop oldest data near hard limit to preserve forward progress.
- Frequent config writes increase flash wear; write policy should be tuned for deployment profile.
- Board pin usage must respect shared-function conflicts (for example, optional LED mirror pinning vs board control signals).
