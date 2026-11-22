# ether2ser
Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge firmware for RP2040 + W5500 (W55RP20-EVB-PICO).

**What It Does**
- Bridges UDP frames to synchronous V.24 via HDLC framing/deframing.
- Uses RP2040 PIO for clocked TX/RX.
- Provides the same management resources over framed USB CDC commands and HTTP/1.1 REST.
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

**Management Interfaces**
- USB CDC is a human-facing command console. Interactive commands use short
  verbs and formatted output; they do not show protocol framing.
- Commands:
```text
help [resource]
status
mem
net
config
v24
mode [bridge|gpio]
pins
pininfo <signal>
get|set gpio <signal> [value]
get|set net <setting> [value]
get|set v24 <setting> [value]
get|set loglevel [value]
save
wipe wipe
reboot
```
- Examples:
```text
status
mode gpio
set gpio rts 1
mode bridge
set net ip.local 192.168.29.2/24
set net ip.remote 192.168.29.5
set net udp.port.local 6969
set v24 baudrate 9600
save
```
- `tools/serial_console.py send` adds a reserved `@` prefix and receives
  `@response` markers for reliable automation. The interactive console client
  sends normal unframed commands, so humans see only the formatted response.
- `bridge` owns the V.24 pins and rejects manual GPIO writes. `gpio` disables the
  UDP/V.24 bridge and permits writes only to non-reserved output signals. Mode
  changes are persisted and rebooted.
- HTTP uses the configured W5500 IPv4 address and fixed TCP port 80; no additional
  persistent network setting is required. Socket 2 is reserved for one bounded,
  nonblocking HTTP connection.
- REST paths are the command resource paths with dots replaced by slashes:
```text
GET  /api/v1/status
GET  /api/v1/config
PUT  /api/v1/system/mode       {"value":"gpio"}
PUT  /api/v1/gpio/rts          {"value":1}
PUT  /api/v1/v24/baudrate      {"value":16000}
POST /api/v1/actions/save
POST /api/v1/actions/wipe      {"confirm":"wipe"}
POST /api/v1/actions/reboot
```
- HTTP responses are JSON and close the connection. Effects that can disrupt the
  connection are applied only after the TCP connection has closed.
- The HTTP service has no authentication or TLS. Use it only on the trusted,
  isolated LAN assumed by this firmware.
- Defaults are defined in `src/drivers/w5500_driver.h`.
- The W5500 MAC is derived from the static IPv4 address as `02:e2:<IPv4 bytes>`. For example, `192.168.29.20` uses `02:e2:c0:a8:1d:14`. Boards on one L2 segment must have distinct IPv4 addresses.

**Status Output**
- `status` returns the shared runtime snapshot as an aligned, human-readable
  report. REST returns the same snapshot as structured JSON.
- Serial example:
```text
status: ok
Operating mode: bridge
Current Baudrate estimation on pin 4: 16000.0 Hz
Current Baudrate estimation on pin 0: 16000.0 Hz
Current Baudrate estimation based on DMA: 16000 Hz
PIPE STATS
  Traffic
    Frames    : udp_rx=420  hdlc_tx=420  hdlc_rx=420
    UDP TX    : attempts=420  ok=420  fail=0
    Backlog   : tx->ready_gap=0  ready->udp_gap=0
  Decode / Sync
    Decode    : frame_ready=420  ok=420  fail=0
    FailReason: invalid=0  short=0  long=0  unstuff=0  crc=0
  Buffers
    TX Queue  : used=0/64  active=0
    Drops     : tx=0  event=0  log=0
    PIO TX    : stalled=1  underruns=0  cts_restarts=0
```
- Rate fields are sampled by the event loop on a fixed one-second interval.
  Querying status does not reset or distort the sampling interval.

| Report section | Meaning | Healthy indication |
|---|---|---|
| Baudrate estimates | GPIO clock estimates and DMA-byte-rate estimate. | DMA estimate near the applied clock; GPIO edge monitoring may undercount at high rates. |
| `Traffic / Frames` | UDP ingress, HDLC TX, and HDLC RX stage counters. | Counters track without a growing stage gap. |
| `Traffic / UDP TX` | Datagram attempts, exact-length sends, and failures. | `attempts == ok`, `fail == 0`; only host receipt proves delivery. |
| `Decode / Sync` | Decode totals, reason counters, synchronization and retained first failure. | No increasing CRC, unstuff, invalid, short, or long failures. |
| `RX Health` | Accumulator high-water, FIFO stalls, dropped bytes, idle filtering and DMA overflow. | Stall, full-drop, and overflow counters remain zero. |
| `Buffers` | Current/peak bounded-buffer use, throttling and drops. | Capacity headroom and zero drops. |
| `PIO TX` / `CTS` | TX stall, underrun recovery, CTS restarts and current CTS state. | A completed frame may leave `stalled=1`; underrun/restart counters should not grow in stable traffic. |

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

**Shared USB consoles (Alacritty + Zellij)**

`tools/serial_console.py` opens one Alacritty window with a Zellij pane for
each board. A background broker owns the serial ports, so both your panes
and automation can send complete commands and observe the same output.
It uses Python 3's standard library on Linux; no pyserial installation is
needed. Close minicom/other programs using the ports first.

Use `/dev/serial/by-path/` paths: the two currently connected boards report
the same USB serial number, so their `by-id` names collide. For the current
USB connections:

```bash
python3 tools/serial_console.py launch \
  --device pico-a=/dev/serial/by-path/pci-0000:02:00.0-usb-0:4:1.0 \
  --device pico-b=/dev/serial/by-path/pci-0000:02:00.0-usb-0:3:1.0
```

Change the paths if you move USB ports or use another PC. In each pane,
press empty Enter to synchronize, then use commands such as `get status` or
`get net`. Commands are edited locally and submitted as whole lines, so agent
input cannot be inserted into your unfinished command. Ctrl-C/D closes a
pane client. The broker stays running when clients/windows close.

From another terminal, or an agent execution tool:

```bash
python3 tools/serial_console.py status
python3 tools/serial_console.py sync pico-a
python3 tools/serial_console.py send pico-a "get status"
python3 tools/serial_console.py sync pico-b
python3 tools/serial_console.py send pico-b "get net"
```

Commands to the same board are serialized; the two boards operate
independently. The broker completes a command only on its matching
`@response <sequence> end ...` marker, so prompts and concurrent firmware
logs cannot terminate a response early. A firmware `error` marker or timeout
returns a nonzero result. Timeout does not retry and requires `sync` before
another command. Persistent background logging does not affect framed command
completion. `save` writes flash, and mode changes or `reboot` interrupt the
console after their response.

The broker reconnects to the same USB path after a reboot; synchronize
again after reconnecting. Logs are in `/tmp/ether2ser-console-UID/` by
default: each board's `.jsonl` records timestamps, TX source/command, and
base64-encoded RX bytes. `broker.log` and `terminal.log` contain process
diagnostics. The directory/socket are private to your user. Logs append
until removed; `/tmp` can be cleared on reboot. To use a different directory,
put `--state-dir PATH` **before** the subcommand in every invocation.

```bash
# Run the broker without a GUI, using the same --device arguments:
python3 tools/serial_console.py serve --device pico-a=/dev/ttyACM0
# Release both ports before flashing or switching to another serial tool:
python3 tools/serial_console.py stop
# Test the broker with emulated boards, without hardware:
python3 tests/test_serial_console.py -v
```

**Native firmware tests**
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
