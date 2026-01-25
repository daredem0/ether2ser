# ether2ser
Ethernet ↔ synchronous V.24 (RS-232/V.28) IP router using RP2040 + W5500.

This project implements an **L3 IP router** that forwards IP packets between Ethernet and synchronous serial interfaces, with PPP-like framing over the serial link.

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

## System Overview

### 1) System partitioning
- Runs on W55 (W5500 hardware)
- Implements in silicon:
  - Ethernet MAC/PHY interface (via magnetics/RJ45)
  - SPI slave interface to RP2040
  - IP packet RX/TX with hardware checksum offload
  - Socket-based UDP/TCP handling
- Does not implement:
  - IP routing logic, PPP framing, serial protocols
- Runs on APU (RP2040 bare-metal firmware)
- Implements:
  - W5500 driver + IP packet handling
  - IP routing between Ethernet and serial interfaces
  - PPP-like protocol with HDLC framing and CRC-16
  - Burst manager (TX queue non-empty -> TX active)
  - Control/status lines policy (RTS/DTR outputs; CTS/DSR/DCD inputs)
  - Configuration + flash persistence
  - Main scheduler/event queue + stats/logging
- Runs on PIO
- Implements:
  - Deterministic synchronous serial "PHY"
  - RX: sample RXD on RXC edges, pack into bytes
  - TX: shift TXD bits in sync to TXC (external pin 15 or generated pin 24)
  - FIFO interface to the APU

### 2) Block architecture

#### Block A — Ethernet PHY/MAC (W55)
- Runs where: W55 hardware
- Responsibilities:
  - Receive Ethernet frames from RJ45
  - Provide received frames to RP2040 via SPI
  - Transmit frames provided by RP2040
- Interfaces:
  - SPI register/memory interface (W55 <-> APU)
  - Interrupt pin (optional): frame available / socket event
- Implementation location: `src/drivers/wiznet_w5500/*`

#### Block B — W55 Driver + IP Adapter (APU)
- Runs where: APU
- Responsibilities:
  - Initialize W55 (SPI, reset, IP config, routing table)
  - Configure W5500 for IP packet handling with hardware offload
  - Provide a clean API:
    - eth_rx_packet() (non-blocking IP packet receive)
    - eth_tx_packet() (non-blocking IP packet transmit)
  - Maintain counters: RX packets, TX packets, routing errors
- Interfaces:
  - To W55 hardware:
    - SPI transactions
    - optional IRQ handling
  - To higher layers (IP Router):
    - RX path: emits ip_packet_t into ip_rx_queue
    - TX path: consumes ip_packet_t from ip_tx_queue
- Data structures
```C
typedef struct {
  uint16_t len;        // IP packet length (20..1500 typical)
  uint8_t  protocol;   // IP protocol (TCP=6, UDP=17, etc)
  uint32_t src_ip;
  uint32_t dst_ip;
  uint8_t  buf[1500];  // pool-owned buffer
} ip_packet_t;
```
- Implementation location:
  - src/drivers/wiznet_w5500/w5500_spi.*
  - src/drivers/wiznet_w5500/w5500_ip.*
  - src/link/queues.* (buffer pool + queues)

#### Block C — IP Router + PPP Adapter (APU)
- Runs where: APU
- Responsibilities:
  - Route IP packets between Ethernet and serial interfaces
  - Handle IP fragmentation when needed (large packets over serial)
  - Maintain routing table and ARP cache for Ethernet side
  - Implement simple NAT if routing between different subnets
  - Apply timeouts to incomplete fragments
- Interfaces:
  - Inputs: ip_packet_t* from Ethernet RX queue
  - Outputs: ppp_pkt_t* to HDLC TX queue (routed packets)
  - Reverse direction: ppp_pkt_t* from HDLC RX -> route -> ip_packet_t* to Ethernet TX queue
- Payload format (inside HDLC payload)
```C
typedef struct __attribute__((packed)) {
  uint16_t protocol;   // PPP protocol (IP=0x0021, IPv6=0x0057)
  uint8_t  payload[];  // IP packet data
} ppp_hdr_t;

typedef struct {
  uint16_t len;
  ppp_hdr_t ppp;
} ppp_pkt_t;
```
- Implementation location:
  - src/link/ip_router.*
  - src/link/ppp_adapter.*
  - src/link/queues.* (packet pool + queues)

#### Block D — HDLC-PPP Codec (APU)
- Runs where: APU
  - Responsibilities:
    - TX
      - Convert ppp_pkt_t to a bitstream:
      - (start-of-burst only) N x 0x7E preamble flags
      - flag, PPP payload, CRC-16, flag
      - bit stuffing for 0x7E and 0x7D escape sequences
      - Provide packed bytes to PHY TX FIFO
  - RX
    - Consume packed bytes from PHY RX FIFO
    - Bit-level flag hunt across byte boundaries
    - Bit unstuff and unescape PPP control characters
    - CRC-16 verify
    - Emit validated ppp_pkt_t payloads to IP Router
- Interfaces:
  - Inputs:
    - from IP Router TX queue: ppp_pkt_t*
    - from PioPhy RX stream: uint8_t rx_bytes[]
  - Outputs:
    - to PioPhy TX stream: uint8_t tx_bytes[] (packed bits)
    - to IP Router RX queue: ppp_pkt_t*
- Key internal streams:
  - phy_tx_fifo is fed with packed bytes, not symbols
  - RX delivers packed bytes which are decoded bitwise
- Implementation location:
  - src/link/hdlc_codec.*
  - src/link/ppp_protocol.*
  - src/link/burst_mgr.* (preamble + burst start logic)

#### Block E — Burst Manager (TX policy) (APU)
- Runs where: APU
- Responsibilities:
  - Implements your rule:
    - "TX active while TX queue non-empty"
    - No idle flags when idle
    - Send preamble flags only at burst start
    - Stop TX cleanly at end-of-frame then go silent
- Interfaces:
  - Observes HDLC TX input queue depth
  - Controls PHY:
    - phy_set_mode()
    - phy_set_bitrate()
    - phy_tx_start() / phy_tx_stop()
  - Controls TX_ACTIVE GPIO (your chosen simplicity)
- State machine: IDLE -> START -> PREAMBLE -> SEND -> DRAIN -> IDLE
- Implementation location:
  - src/link/burst_mgr.*
  - ties into src/pio/pio_phy.* and src/system/config.*

#### Block F — PIO PHY (PIO programs + APU glue)
- Runs where:
  - PIO: the state machines
  - APU: setup, enable/disable, drain/fill FIFOs
- Responsibilities (PIO):
  - RX SM: sample RXD on RXC (pin 17), pack bytes -> RX FIFO
  - TX SM ext: shift TXD on TXC15 edges, but only if TX_ACTIVE=1
  - TX SM gen: generate TXC24 + shift TXD while active
- Responsibilities (APU glue):
  - Load PIO programs
  - Configure pin mapping
  - Serve FIFOs:
    - RX: drain FIFO into a ring buffer
    - TX: keep FIFO filled from HDLC encoder output
  - Expose clean non-blocking calls to higher layers
- Interfaces:
  - PIO <-> APU
    - RX FIFO: bytes to APU
    - TX FIFO: bytes from APU
    - Optional IRQs: FIFO threshold events (optional)
- APU-facing API
```C
void phy_init(const phy_pins_t *pins);
void phy_set_mode(phy_mode_t mode);        // EXT15 or GEN24
void phy_set_bitrate(uint32_t bps);        // GEN24 only
void phy_tx_enable(bool en);               // controls TX_ACTIVE GPIO too
size_t phy_rx_read(uint8_t *dst, size_t max);
size_t phy_tx_write(const uint8_t *src, size_t len);

```
- Implementation location:
  - src/pio/v24_rx.pio
  - src/pio/v24_tx_extclk.pio
  - src/pio/v24_tx_genclk.pio
  - src/pio/pio_phy.*

#### Block G — V.24 Control/Status GPIO (APU)
- Runs where: APU
- Responsibilities:
  - Drive outputs:
    - RTS, DTR (static or CLI-configurable)
  - Sample inputs:
    - CTS, DSR, DCD (status/diagnostics; DCD ignored for RX gating for now)
  - Provide status to CLI (show)
- Interfaces
  - Simple API:
```C
void v24_ctrl_init(void);
void v24_ctrl_set_rts(bool);
void v24_ctrl_set_dtr(bool);
v24_status_t v24_ctrl_get_status(void);
```
- Implementation location:
  - src/link/v24_control.*
  - pin definitions in src/system/board_pins.h

#### Block H — Configuration + Persistence + USB CLI (APU)
- Runs where: APU
- Responsibilities:
  - USB CDC serial CLI (minicom connects)
  - Parse commands to update desired config
  - Apply config at safe points:
    - bitrate/mode only when TX idle
    - IP changes re-init W55
  - Persist config in flash with CRC
- Interfaces:
  - CLI <-> Config:
    - command handlers produce config_event_t posted to event queue
  - Config <-> Other blocks:
    - phy_set_mode, phy_set_bitrate
    - eth_set_ip
    - burst_mgr_set_preamble_flags
    - etc.
- Implementation location:
  - src/system/cli_usb_cdc.*
  - src/system/config.*
  - src/system/config_store.*

#### Block I — Scheduler / Event Queue (APU)
- Runs where: APU
- Responsibilities:
  - Single-threaded "hybrid" scheduler:
    - poll/drain/fill at high priority
    - handle CLI/config events
    - periodic ticks for timeouts/reassembly
- Interfaces:
  - Event queue API
  - "service" functions:
    - service_phy_rx(), service_phy_tx()
    - service_hdlc()
    - service_linkmapper()
    - service_eth()
    - service_cli()
    - service_config()
- Implementation location:
  - src/system/event_queue.*
  - main loop in src/main.c

### 3) Inter-component interfaces (summary)
- Data path (Ethernet -> Serial):
  - W55 IP RX (W55)
    -> EthernetIO (APU): ip_packet_t*
    -> IP Router (APU): ppp_pkt_t* (routed packets)
    -> HDLC-PPP TX (APU): packed-bit bytes with PPP framing
    -> PIO PHY TX FIFO (PIO): TXD + TXC (15 or 24)
    -> DB25
- Data path (Serial -> Ethernet):
  - DB25
    -> PIO PHY RX FIFO (PIO): packed bytes from RXD/RXC
    -> HDLC-PPP RX (APU): ppp_pkt_t* validated
    -> IP Router (APU): ip_packet_t* (routed packets)
    -> EthernetIO IP TX (APU->W55)
    -> W55 TX (W55)
- Control/config path:
  - USB CDC CLI (APU)
    -> ConfigManager events (APU)
    -> apply to: BurstMgr, PioPhy, EthernetIO

### 4) Where each block is implemented (file map)
- W55 driver / IP packets: src/drivers/wiznet_w5500/*
- PIO programs: src/pio/*.pio
- PIO glue: src/pio/pio_phy.*
- HDLC-PPP codec: src/link/hdlc_codec.*
- PPP protocol handler: src/link/ppp_protocol.*
- Burst manager: src/link/burst_mgr.*
- IP router + NAT: src/link/ip_router.*
- Queues + buffer pools: src/link/queues.*
- V24 control GPIO: src/link/v24_control.*
- Config + flash store: src/system/config.*, src/system/config_store.*
- USB CLI: src/system/cli_usb_cdc.*
- Scheduler/event queue: src/system/event_queue.*
- Top-level orchestration: src/main.c
