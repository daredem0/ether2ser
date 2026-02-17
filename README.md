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

**Status Output**
- The `status` command prints live pipeline diagnostics.
- Example:
```text
ether2ser> status
status: ok
Current Baudrate estimation on pin 4: 109010.0 Hz
PIPE STATS
  Traffic
    Frames    : udp_rx=420  hdlc_tx=420  hdlc_rx=420  udp_tx=420
    Backlog   : tx->ready_gap=0  ready->udp_gap=0
    Serial    : rx_bytes=630410  tx_bytes=630410
    Rates     : udp_rx=420000/s  hdlc_rx=420000/s  udp_tx=420000/s  fail=0/s  rx_bytes=630410/s
  Decode / Sync
    Decode    : frame_ready=420  ok=420  fail=0
    FailReason: invalid=0  short=0  long=0  unstuff=0  crc=0
    SyncState : HUNTING
    SyncWait  : syncing=0  synced=0
    SyncMaint : consume=420  hardcap_events=0  hardcap_bytes=0
    Resync    : idle=0  hard=0  no_progress=0
    Accum     : pos=0  proc=0  state=0  off=0  cand_valid=0  cand_end=0
    RX Health : acc_pos_max=4  rx_fifo_stall=0  rx_drop_acc_full=0
  Buffers
    TX Queue  : used=0/32  active=0
    HighWater : tx=0  event=1  log=4
    Drops     : tx=0  event=0  log=0
    Recons    : len=0
    PIO TX    : stalled=1
```
- Field meanings

`General`

| Name | Meaning | Debug hint |
|---|---|---|
| `Current Baudrate estimation` | Measured RX clock frequency from the `v24_rxc` monitor pin. | If this is far from configured baud, debug physical clock/sampling before protocol logic. |

`Traffic`

| Name | Meaning | Debug hint |
|---|---|---|
| `Frames` | Stage counters across the pipeline (`udp_rx`, `hdlc_tx`, `hdlc_rx`, `udp_tx`). | Compare columns to locate where frames are being lost. |
| `udp_rx` | UDP frames accepted from Ethernet. | If low, issue is upstream/network side. |
| `hdlc_tx` | Frames encoded and queued to serial TX. | `udp_rx` and `hdlc_tx` should track closely. |
| `hdlc_rx` | HDLC frame candidates detected on serial RX. | Much larger than `hdlc_tx` usually means desync/false framing. |
| `udp_tx` | Frames successfully decoded and sent via UDP. | Gap to `hdlc_rx` indicates decode/sync failures. |
| `Backlog` | Gap metrics between stages. | `tx->ready_gap` grows when RX cannot keep up; `ready->udp_gap` grows on decode failures. |
| `tx->ready_gap` | `hdlc_tx - hdlc_rx`. | Persistent growth means receive/framing lag. |
| `ready->udp_gap` | `hdlc_rx - udp_tx`. | Persistent growth points at decode integrity issues. |
| `Serial` | Cumulative byte counters on wire (`rx_bytes`, `tx_bytes`). | Large divergence indicates one direction not flowing cleanly. |
| `Rates` | Per-second deltas from previous `status`. | First sample or very short interval can look inflated. |

`Decode / Sync`

| Name | Meaning | Debug hint |
|---|---|---|
| `Decode` | Decoder outcomes (`frame_ready`, `ok`, `fail`). | `fail` near zero is expected in stable runs. |
| `frame_ready` | HDLC candidates emitted by sync layer. | If this overshoots TX by a lot, suspect framing alignment/noise. |
| `ok` | Successful decode + CRC validation. | Should track `udp_tx`. |
| `fail` | Failed decode attempts. | Use `FailReason` to identify root cause. |
| `FailReason` | Breakdown (`invalid`, `short`, `long`, `unstuff`, `crc`). | `unstuff` dominance suggests bitstream corruption/alignment; `crc` points to data corruption after framing. |
| `SyncState` | Current sync FSM state (`HUNTING`, `SYNCING`, `SYNCED`). | Long time in `SYNCING`/`SYNCED` with low progress can indicate stuck candidate flow. |
| `SyncWait` | Lookahead waits for cross-byte alignment in `SYNCING`/`SYNCED`. | Fast growth indicates frequent boundary stalls. |
| `SyncMaint` | Maintenance counters (`consume`, `hardcap_events`, `hardcap_bytes`). | `hardcap_*` should remain near zero in healthy operation. |
| `Resync` | Recovery reason counters (`idle`, `hard`, `no_progress`). | Spikes indicate unstable decode path or over-aggressive thresholds. |
| `Accum` | Internal accumulator live state (`pos`, `proc`, `state`, `off`, candidate fields). | Non-zero `pos`/`proc` for long periods without progress indicates parser stall. |
| `RX Health` | RX path health (`acc_pos_max`, `rx_fifo_stall`, `rx_drop_acc_full`). | Rising `rx_fifo_stall` indicates service starvation; rising `rx_drop_acc_full` means accumulator backpressure. |

`Buffers`

| Name | Meaning | Debug hint |
|---|---|---|
| `TX Queue` | Current TX queue occupancy (`used/capacity`, `active`). | `used` near capacity indicates sustained overload. |
| `used` | Number of queued TX frames. | Trend matters more than instantaneous value. |
| `active` | `1` if one frame is currently being drained to TX FIFO. | `active=0` with `used>0` indicates drain path issue. |
| `HighWater` | Peak occupancy since boot (`tx`, `event`, `log`). | Useful to size buffers and detect near-overflow phases. |
| `Drops` | Cumulative dropped items (`tx`, `event`, `log`). | Any non-zero drop counter indicates data/control loss. |
| `Recons` | Current reconstructed frame length pending decode. | Large lingering values can indicate stuck/incomplete candidate handling. |
| `PIO TX` | TX PIO stall flag state. | Sticky hardware indicator; useful to correlate with underfeeding/transmit pacing. |

**UDP Test Scripts**
- `tests/udp_cli_sender.sh`
  - Interactive shell sender: reads text lines from stdin and sends each line as a UDP datagram with `socat`.
  - Uses `UDP-DATAGRAM:<ip>:<port>,broadcast`; edit `DEST_IP` and `DEST_PORT` inside the script before use.
  - Example:
  ```bash
  bash tests/udp_cli_sender.sh
  ```

- `tests/send_receive.py`
  - Configurable UDP sequence test with `sender`, `receiver`, or `both` mode.
  - Sender emits framed packets with sequence + timestamp; receiver reports gaps, duplicates, ordering, and rates.
  - Example (receiver):
  ```bash
  python tests/send_receive.py --mode receiver --port 6969 --idle-timeout 10
  ```
  - Example (sender):
  ```bash
  python tests/send_receive.py --mode sender --host 192.168.29.20 --port 6969 --size 1472 --rate 100 --duration 10
  ```

- `tests/stress.py`
  - Legacy quick stress script with hardcoded constants at the top (`HOST`, `PORT`, `SIZE`, `RATE`, `DURATION`).
  - Sends sequence-tagged packets and prints loss/RTT summary after a short drain period.
  - Example:
  ```bash
  python tests/stress.py
  ```

- `tests/udp_stress.py`
  - Extended stress/soak tool with `sender`, `receiver`, or `both` (loopback) mode.
  - Computes send rate from `baudrate / (frame_size * 8)`, supports random frame sizes (`--min-size` to `--size`),
    start/end markers, monotonic sequence tracking, loss stats, configurable idle timeout, and optional minimum inter-frame delay (`--min-delay-ms`).
  - Example (loopback):
  ```bash
  python tests/udp_stress.py --mode both --host 127.0.0.1 --port 6969 --baudrate 1000000 --size 1200 --min-size 64 --duration 30 --verbose
  ```
  - Example (separate receiver):
  ```bash
  python tests/udp_stress.py --mode receiver --port 6969 --idle-timeout 15 --verbose
  ```

- `tests/image_send_receive.py` (requested as `tests/image_send_and_receive.py`)
  - Image UDP sender/receiver: sender converts PNG/JPG to grayscale, max `256x256`, chunks with sequence + last-frame flag; receiver reassembles and writes PNG.
  - Receiver supports `--preview` to open the image after complete reception.
  - Example (receiver):
  ```bash
  python tests/image_send_receive.py --mode receiver --port 6969 --output received.png --preview
  ```
  - Example (sender):
  ```bash
  python tests/image_send_receive.py --mode sender --host 192.168.29.20 --port 6969 --size 1472 --rate 5 input.jpg
  ```

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
