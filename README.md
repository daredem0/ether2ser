# ether2ser

## System Overview

### 1) System partitioning
- Runs on W55 (W5500 hardware)
- Implements in silicon:
  - Ethernet MAC/PHY interface (via magnetics/RJ45)
  - SPI slave interface to RP2040
  - Frame RX/TX buffering in internal memory
  - MACRAW mode (for raw L2 frames), if used by your driver
- Does not implement:
  - Your bridging logic, framing, CRC, etc.
- Runs on APU (RP2040 bare-metal firmware)
- Implements:
  - W5500 driver + raw frame RX/TX logic
  - L2 tunnel fragmentation/reassembly
  - Bit-oriented HDLC-lite encoder/decoder with CRC-16
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

#### Block B — W55 Driver + MACRAW Adapter (APU)
- Runs where: APU
- Responsibilities:
  - Initialize W55 (SPI, reset, MAC, link)
  - Configure MACRAW for true L2 frame RX/TX
  - Provide a clean API:
    - eth_rx_frame() (non-blocking)
    - eth_tx_frame() (non-blocking)
  - Maintain counters: RX frames, TX frames, SPI errors
- Interfaces:
  - To W55 hardware:
    - SPI transactions
    - optional IRQ handling
  - To higher layers (LinkMapper):
    - RX path: emits eth_frame_t into eth_rx_queue
    - TX path: consumes eth_frame_t from eth_tx_queue
- Data structures
```C
typedef struct {
  uint16_t len;        // 64..1518 typical (or 1522 with VLAN later)
  uint8_t  buf[1600];  // pool-owned buffer
} eth_frame_t;
```
- Implementation location:
  - src/drivers/wiznet_w5500/w5500_spi.*
  - src/drivers/wiznet_w5500/w5500_raw.*
  - src/link/queues.* (buffer pool + queues)

#### Block C — L2 Tunnel Mapper (fragmentation/reassembly) (APU)
- Runs where: APU
- Responsibilities:
  - Fragment Ethernet frames into multiple "tunnel payload units"
  - Reassemble tunnel payload units back into Ethernet frames
  - Enforce bounded memory use (pool + drop policy)
  - Apply timeouts to incomplete reassembly
- Interfaces:
  - Inputs: eth_frame_t* from Ethernet RX queue
  - Outputs: tunnel_pkt_t* to HDLC TX queue (fragmented pieces)
  - Reverse direction: tunnel_pkt_t* from HDLC RX -> reassembly -> eth_frame_t* to Ethernet TX queue
- Payload format (inside HDLC payload)
```C
typedef struct __attribute__((packed)) {
  uint16_t frame_id;
  uint8_t  frag_index;
  uint8_t  frag_count;
  uint16_t eth_len;
  uint16_t payload_len;
  uint8_t  payload[];
} tunnel_hdr_t;
```
- Implementation location:
  - src/link/l2_tunnel.*
  - src/link/queues.* (tunnel packet pool + queues)

#### Block D — HDLC-lite Codec (APU)
- Runs where: APU
  - Responsibilities:
    - TX
      - Convert tunnel_pkt_t to a bitstream:
      - (start-of-burst only) N x 0x7E preamble flags
      - flag, payload, CRC-16, flag
      - bit stuffing
      - Provide packed bytes to PHY TX FIFO
  - RX
    - Consume packed bytes from PHY RX FIFO
    - Bit-level flag hunt across byte boundaries
    - Bit unstuff
    - CRC-16 verify
    - Emit validated tunnel_pkt_t payloads to LinkMapper
- Interfaces:
  - Inputs:
    - from LinkMapper TX queue: tunnel_pkt_t*
    - from PioPhy RX stream: uint8_t rx_bytes[]
  - Outputs:
    - to PioPhy TX stream: uint8_t tx_bytes[] (packed bits)
    - to LinkMapper RX queue: tunnel_pkt_t*
- Key internal streams:
  - phy_tx_fifo is fed with packed bytes, not symbols
  - RX delivers packed bytes which are decoded bitwise
- Implementation location:
  - src/link/hdlc_codec.*
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
- Data path (TX direction):
  - W55 MACRAW RX (W55)
    -> EthernetIO (APU): eth_frame_t*
    -> L2Tunnel (APU): tunnel_pkt_t* fragments
    -> HDLC TX (APU): packed-bit bytes
    -> PIO PHY TX FIFO (PIO): TXD + TXC (15 or 24)
    -> DB25
- Data path (RX direction):
  - DB25
    -> PIO PHY RX FIFO (PIO): packed bytes from RXD/RXC
    -> HDLC RX (APU): tunnel_pkt_t* validated
    -> L2Tunnel reassembly (APU): eth_frame_t*
    -> EthernetIO MACRAW TX (APU->W55)
    -> W55 TX (W55)
- Control/config path:
  - USB CDC CLI (APU)
    -> ConfigManager events (APU)
    -> apply to: BurstMgr, PioPhy, EthernetIO

### 4) Where each block is implemented (file map)
- W55 driver / raw frames: src/drivers/wiznet_w5500/*
- PIO programs: src/pio/*.pio
- PIO glue: src/pio/pio_phy.*
- HDLC codec: src/link/hdlc_codec.*
- Burst manager: src/link/burst_mgr.*
- L2 tunnel: src/link/l2_tunnel.*
- Queues + buffer pools: src/link/queues.*
- V24 control GPIO: src/link/v24_control.*
- Config + flash store: src/system/config.*, src/system/config_store.*
- USB CLI: src/system/cli_usb_cdc.*
- Scheduler/event queue: src/system/event_queue.*
- Top-level orchestration: src/main.c
