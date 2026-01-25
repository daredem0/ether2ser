# ether2serial roadmap
## Dependencies
Look at `setup_toolchain.sh`

## Building
```
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Flashing
```
cmake --build . --target flash_elf
```

## Phase 1 — Hardware sanity + pin validation (1–2 sessions)

**Goal:** Prove the MAX3243 wiring and your chosen GPIO mapping are correct.

- Bring up a GPIO test firmware
  - CLI commands: set <pin> <0|1>, read <pin>
  - Toggle TXD/RTS/DTR outputs (logic side) and verify on RS-232 side with scope/LA
  - Read RXD/CTS/DSR/DCD inputs and verify they reflect RS-232 side changes
- Verify both MAX3243 charge pumps
  - Measure V+ and V− on each MAX3243 (should be generated rails)
  - Confirm FORCEON/FORCEOFF are tied as intended

**Exit:** You trust your PCB/prototype wiring and pin map.

## Phase 2 — PIO RX (clocked sampling) standalone

**Goal:** Sample RXD on RXC (pin 17) into bytes reliably.

- Implement PIO RX SM
  - Wait for RXC edges
  - Sample RXD
  - Pack 8 bits → push to FIFO
- Add CLI/USB commands:
  - `rx dump <nbytes>` (prints hex)
  - `rx stats` (FIFO overruns, clock present)
- Test source for RXC/RXD:
  - simplest: second RP2040 generating a known pattern + clock
  - or signal generator

**Exit:** Received bytes match the known pattern at 9.6/19.2/64k.

## Phase 3 — PIO TX generated clock mode (pin 24)

**Goal:** Generate TXC and output TXD synchronized, only when enabled.

- Implement PIO TX gen-clock SM
  - Side-set toggles TXC
  - Shifts TXD bits from FIFO
- CLI commands:
  - `tx rate 19200` (sets divider)
  - `tx pattern 55 <n>` (0x55 pattern)
  - `tx stop` (stops clock, holds TXD)
- Scope checks:
  - TXC frequency correct for all common rates up to 64k
  - TXD transitions aligned to TXC
  - TXC stops cleanly when idle

**Exit:** You can reliably generate clock/data bursts.

## Phase 4 — PIO TX external clock mode (pin 15) + TX_ACTIVE gating

**Goal:** Behave identically when TXC is provided externally: data only when queue non-empty.

- Implement PIO TX ext-clock SM
  - Wait for TXC edges (pin 15)
  - Read TX_ACTIVE GPIO:
    - if 0: hold TXD constant idle
    - if 1: shift bits from FIFO
- Test with external TXC source:
  - a second RP2040 generating TXC is ideal
  - verify "TX_ACTIVE=0 → TXD flat even though TXC keeps ticking"

**Exit:** External clock mode meets your "silent when idle" requirement.

## Phase 5 — HDLC-like codec on CPU (no Ethernet yet)

**Goal:** Get bit-oriented framing working end-to-end over your PIO "PHY".

- Implement CPU HDLC TX encoder
  - flags, bit-stuffing, CRC-16, closing flag
  - add "burst preamble flags" when TX starts
- Implement CPU HDLC RX decoder
  - bit-level flag hunt across byte boundaries
  - unstuffing
  - CRC-16 verify
  - emit payload frames
- Do loopback test
  - easiest: internally loop TX stream into RX path using a second board
  - or wire TXC24→RXC17 and TXD→RXD (logic side) for a local loopback
- CLI:
  - `hdlc send "hello"`
  - `hdlc selftest <len>`

**Exit:** Payload passes with zero CRC errors at 19.2 and 64k.

## Phase 6 — L2 tunnel fragmentation/reassembly (still no Ethernet)

**Goal:** Carry 1500-byte "Ethernet frame blobs" over HDLC reliably.

- Implement fragmentation header + reassembly
  - choose initial fragment payload size (start 256–512B)
  - reassembly timeout + drop stats
- CLI synthetic test:
  - `tunnel sendfake <len>` (64..1500)
  - verify reassembled bytes match

**Exit:** Full 1500-byte payload survives your link.

## Phase 7 — W5500 bring-up and raw L2 capture/transmit (critical risk area)

**Goal:** Prove you can do "true L2 forwarding" with W5500 on this board.

- Bring up W5500 basic init + link status + SPI comms
  - `eth status` (link up? MAC?)
- Implement MACRAW receive
  - capture raw Ethernet frames
  - print summary: dst/src MAC, EtherType, len
- Implement MACRAW transmit
  - send a simple test frame (e.g., broadcast) and verify with a PC running tcpdump

**Exit:** You can RX and TX raw Ethernet frames on the W5500 reliably.

## Phase 8 — End-to-end bridging (final functional goal)

**Goal:** Ethernet frames go over V.24 and back.

- Connect pipelines:
  - W5500 RX → enqueue → fragment → HDLC TX → PIO → RS-232
  - RS-232 RX → PIO → HDLC RX → reassemble → W5500 TX
- Use two devices for first real test (recommended):
  - Device A ↔ V.24 ↔ Device B
  - Each has Ethernet to a PC or switch
- Test plan:
  - See ARP traverse (must for L2)
  - ping across a topology that forces traffic through the link
  - tcpdump -e on both sides to confirm byte-perfect frames

**Exit:** Stable L2 bridging at 19.2–64k.

## ## Phase 9 — Configuration + persistence + robustness

**Goal:** Make it usable and reliable.

- USB CLI:
  - `set txc pin15|pin24`
  - `set rate …`
  - `set ip …`
  - `save / show / stats`
- Safe apply rules:
  - apply mode/rate only when TX idle
  - IP changes re-init W5500
- Hardening:
  - buffer pool limits + drop policy
  - reassembly timeout
  - watchdog
  - counters and diagnostics

**Exit:** "Appliance-like" behavior.
