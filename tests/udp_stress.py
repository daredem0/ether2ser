#!/usr/bin/env python3
"""UDP stress tool with rate derived from baudrate and frame size."""

import argparse
import random
import socket
import struct
import time
import zlib
from dataclasses import dataclass, field
from typing import Optional, Tuple

DEFAULT_HOST = "192.168.29.20"
DEFAULT_PORT = 6969
DEFAULT_MAX_SIZE = 1472  # Max safe payload to avoid IP fragmentation.
DEFAULT_MIN_SIZE = 64
DEFAULT_BAUDRATE = 1_000_000.0  # bits per second
DEFAULT_DURATION = 10.0
DEFAULT_IDLE_TIMEOUT = 10.0

MAGIC = b"UDST"
FLAG_START = 0x01
FLAG_END = 0x02
HEADER_STRUCT = struct.Struct("!4sIIB3x")  # magic, seq, total, flags
HEADER_SIZE = HEADER_STRUCT.size
CHECKSUM_STRUCT = struct.Struct("!I")
CHECKSUM_SIZE = CHECKSUM_STRUCT.size

MAX_UDP_PAYLOAD = 65507
RED = "\033[31m"
ORANGE = "\033[38;5;214m"
RESET = "\033[0m"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="UDP stress sender/receiver tool.")
    parser.add_argument(
        "--mode",
        choices=("sender", "receiver", "both"),
        default="both",
        help="Run as sender, receiver, or both (loopback).",
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Target host for sender mode.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port.")
    parser.add_argument(
        "--size",
        type=int,
        default=DEFAULT_MAX_SIZE,
        help="Maximum datagram size in bytes (header included).",
    )
    parser.add_argument(
        "--min-size",
        type=int,
        default=DEFAULT_MIN_SIZE,
        help="Minimum datagram size in bytes (header included).",
    )
    parser.add_argument(
        "--baudrate",
        "--baud",
        dest="baudrate",
        type=float,
        default=DEFAULT_BAUDRATE,
        help="Target link baudrate in bits per second.",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=DEFAULT_DURATION,
        help="Send duration in seconds.",
    )
    parser.add_argument(
        "--min-delay-ms",
        type=float,
        default=0.0,
        help="Minimum delay between frames in milliseconds (sender/both).",
    )
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=DEFAULT_IDLE_TIMEOUT,
        help="Receiver stop timeout in seconds without any packet.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Random seed for reproducible frame sizes.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print per-packet logs.",
    )
    args = parser.parse_args()

    if args.size < HEADER_SIZE:
        parser.error(f"--size must be >= header size ({HEADER_SIZE} bytes)")
    if args.min_size < HEADER_SIZE:
        parser.error(f"--min-size must be >= header size ({HEADER_SIZE} bytes)")
    if args.min_size > args.size:
        parser.error("--min-size must be <= --size")
    if args.size > MAX_UDP_PAYLOAD:
        parser.error(f"--size must be <= {MAX_UDP_PAYLOAD} bytes")
    if args.baudrate <= 0:
        parser.error("--baudrate must be > 0")
    if args.duration <= 0:
        parser.error("--duration must be > 0")
    if args.min_delay_ms < 0:
        parser.error("--min-delay-ms must be >= 0")
    if args.idle_timeout <= 0:
        parser.error("--idle-timeout must be > 0")
    if args.mode in ("sender", "both"):
        min_data_size = HEADER_SIZE + CHECKSUM_SIZE
        if args.size < min_data_size:
            parser.error(f"--size must be >= {min_data_size} bytes to fit data checksum")
        if args.min_size < min_data_size:
            parser.error(f"--min-size must be >= {min_data_size} bytes to fit data checksum")

    return args


def compute_rate_pps(baudrate: float, frame_size: int) -> float:
    return baudrate / (frame_size * 8.0)


def estimate_total_frames(duration: float, rate_pps: float) -> int:
    return int(round(duration * rate_pps))


def create_socket(mode: str, port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if mode in ("receiver", "both"):
        sock.bind(("", port))
    sock.setblocking(False)
    return sock


def make_payload(seq: int, nbytes: int) -> bytes:
    return bytes((((seq * 31) + (i * 17) + (seq >> 8)) & 0xFF) for i in range(nbytes))


def compute_checksum(header: bytes, payload: bytes) -> int:
    return zlib.crc32(header + payload) & 0xFFFFFFFF


def build_packet(seq: int, total: int, flags: int, payload_len: int) -> bytes:
    header = HEADER_STRUCT.pack(MAGIC, seq, total, flags)
    if payload_len <= 0:
        return header
    if flags == 0:
        if payload_len < CHECKSUM_SIZE:
            raise ValueError("payload length too small for checksum")
        payload_data = make_payload(seq, payload_len - CHECKSUM_SIZE)
        checksum = CHECKSUM_STRUCT.pack(compute_checksum(header, payload_data))
        return header + payload_data + checksum
    return header + make_payload(seq, payload_len)


@dataclass
class SenderStats:
    computed_rate_pps: float
    planned_frames: int
    data_frames: int = 0
    start_sent: int = 0
    end_sent: int = 0
    bytes_total: int = 0
    start_time: float = 0.0
    end_time: float = 0.0
    size_min: Optional[int] = None
    size_max: Optional[int] = None

    def on_send(self, size: int, is_start: bool, is_end: bool) -> None:
        self.bytes_total += size
        if self.size_min is None or size < self.size_min:
            self.size_min = size
        if self.size_max is None or size > self.size_max:
            self.size_max = size
        if is_start:
            self.start_sent += 1
        elif is_end:
            self.end_sent += 1
        else:
            self.data_frames += 1


@dataclass
class ReceiverStats:
    packet_count: int = 0
    data_packets: int = 0
    start_packets: int = 0
    end_packets: int = 0
    duplicates: int = 0
    out_of_order: int = 0
    bytes_total: int = 0
    data_bytes_total: int = 0
    invalid_packets: int = 0
    invalid_checksum_frames: int = 0
    size_min: Optional[int] = None
    size_max: Optional[int] = None
    first_seq: Optional[int] = None
    last_seq: Optional[int] = None
    expected_total: Optional[int] = None
    end_seq: Optional[int] = None
    expected_next: int = 0
    missing_gaps: int = 0
    first_rx_time: Optional[float] = None
    last_rx_time: Optional[float] = None
    seen: set[int] = field(default_factory=set)

    def on_packet(self, data: bytes, now: float, verbose: bool) -> bool:
        self.packet_count += 1
        self.bytes_total += len(data)
        if self.size_min is None or len(data) < self.size_min:
            self.size_min = len(data)
        if self.size_max is None or len(data) > self.size_max:
            self.size_max = len(data)

        if self.first_rx_time is None:
            self.first_rx_time = now
        self.last_rx_time = now

        if len(data) < HEADER_SIZE:
            self.invalid_packets += 1
            if verbose:
                print(f"[rx] invalid packet (len={len(data)})")
            return False

        magic, seq, total, flags = HEADER_STRUCT.unpack(data[:HEADER_SIZE])
        if magic != MAGIC:
            self.invalid_packets += 1
            if verbose:
                print(f"[rx] invalid magic from seq={seq}")
            return False

        if flags & FLAG_START:
            self.start_packets += 1
            if total > 0:
                self.expected_total = total
            if verbose:
                print(f"[rx] START total={total}")
            return False

        if flags & FLAG_END:
            self.end_packets += 1
            self.end_seq = seq
            if total > 0:
                self.expected_total = total
            elif seq >= 0:
                self.expected_total = seq + 1
            if verbose:
                print(f"[rx] END seq={seq} total={total}")
            return True

        if len(data) < HEADER_SIZE + CHECKSUM_SIZE:
            self.invalid_checksum_frames += 1
            print(f"{ORANGE}INVALID FRAME {seq}{RESET}")
            if verbose:
                print(f"[rx] checksum missing seq={seq} bytes={len(data)}")
            return False

        payload_with_checksum = data[HEADER_SIZE:]
        payload_data = payload_with_checksum[:-CHECKSUM_SIZE]
        received_checksum = CHECKSUM_STRUCT.unpack(payload_with_checksum[-CHECKSUM_SIZE:])[0]
        calculated_checksum = compute_checksum(data[:HEADER_SIZE], payload_data)
        if received_checksum != calculated_checksum:
            self.invalid_checksum_frames += 1
            print(f"{ORANGE}INVALID FRAME {seq}{RESET}")
            if verbose:
                print(
                    f"[rx] checksum mismatch seq={seq} got=0x{received_checksum:08x} "
                    f"expected=0x{calculated_checksum:08x}"
                )
            return False

        payload_len = len(payload_data)
        self.data_packets += 1
        self.data_bytes_total += len(data)

        if seq in self.seen:
            self.duplicates += 1
            if verbose:
                print(f"[rx] duplicate seq={seq}")
            return False

        self.seen.add(seq)
        if self.first_seq is None:
            self.first_seq = seq
        if self.last_seq is None or seq > self.last_seq:
            self.last_seq = seq

        if seq == self.expected_next:
            self.expected_next += 1
        elif seq > self.expected_next:
            for missed_seq in range(self.expected_next, seq):
                print(f"{RED}MISSED {missed_seq}{RESET}")
            self.missing_gaps += seq - self.expected_next
            self.expected_next = seq + 1
        else:
            self.out_of_order += 1

        if verbose:
            print(f"[rx] DATA seq={seq} bytes={len(data)} payload={payload_len} checksum=ok")
        return False


def choose_frame_size(rng: random.Random, min_size: int, max_size: int) -> int:
    return rng.randint(min_size, max_size)


def send_start(sock: socket.socket, target: Tuple[str, int], total: int, stats: SenderStats) -> None:
    packet = build_packet(0, total, FLAG_START, 0)
    sock.sendto(packet, target)
    stats.on_send(len(packet), is_start=True, is_end=False)


def send_end(sock: socket.socket, target: Tuple[str, int], last_seq: int, total: int, stats: SenderStats) -> None:
    packet = build_packet(last_seq if last_seq >= 0 else 0, total, FLAG_END, 0)
    sock.sendto(packet, target)
    stats.on_send(len(packet), is_start=False, is_end=True)


def run_sender(sock: socket.socket, args: argparse.Namespace, rng: random.Random) -> SenderStats:
    rate_pps = compute_rate_pps(args.baudrate, args.size)
    interval = 1.0 / rate_pps
    min_delay = args.min_delay_ms / 1000.0
    effective_interval = max(interval, min_delay) if min_delay > 0 else interval
    planned_frames = estimate_total_frames(args.duration, rate_pps)
    stats = SenderStats(computed_rate_pps=rate_pps, planned_frames=planned_frames)

    target = (args.host, args.port)
    stats.start_time = time.monotonic()
    send_start(sock, target, planned_frames, stats)

    seq = 0
    end_time = stats.start_time + args.duration
    next_send = stats.start_time

    while time.monotonic() < end_time:
        now = time.monotonic()
        if now < next_send:
            time.sleep(next_send - now)
        else:
            next_send = now

        frame_size = choose_frame_size(rng, args.min_size, args.size)
        payload_len = frame_size - HEADER_SIZE
        packet = build_packet(seq, 0, 0, payload_len)
        sock.sendto(packet, target)
        stats.on_send(len(packet), is_start=False, is_end=False)
        if args.verbose:
            print(
                f"[tx] DATA seq={seq} bytes={len(packet)} "
                f"payload={payload_len - CHECKSUM_SIZE} checksum={CHECKSUM_SIZE}"
            )
        seq += 1
        next_send += effective_interval

    send_end(sock, target, seq - 1, seq, stats)
    stats.end_time = time.monotonic()
    return stats


def recv_available(sock: socket.socket, stats: ReceiverStats, verbose: bool) -> bool:
    end_received = False
    while True:
        try:
            data, _ = sock.recvfrom(65535)
        except BlockingIOError:
            break
        except InterruptedError:
            continue
        now = time.monotonic()
        if stats.on_packet(data, now=now, verbose=verbose):
            end_received = True
    return end_received


def run_receiver(sock: socket.socket, args: argparse.Namespace) -> Tuple[ReceiverStats, str]:
    stats = ReceiverStats()
    start_wait = time.monotonic()
    stop_reason = ""

    while True:
        if recv_available(sock, stats, args.verbose):
            stop_reason = "end_frame_received"
            break

        now = time.monotonic()
        ref = stats.last_rx_time if stats.last_rx_time is not None else start_wait
        if now - ref >= args.idle_timeout:
            stop_reason = f"idle_timeout_{args.idle_timeout:.1f}s"
            break

        time.sleep(0.001)

    return stats, stop_reason


def run_both(
    sock: socket.socket, args: argparse.Namespace, rng: random.Random
) -> Tuple[SenderStats, ReceiverStats, str]:
    rate_pps = compute_rate_pps(args.baudrate, args.size)
    interval = 1.0 / rate_pps
    min_delay = args.min_delay_ms / 1000.0
    effective_interval = max(interval, min_delay) if min_delay > 0 else interval
    planned_frames = estimate_total_frames(args.duration, rate_pps)

    tx_stats = SenderStats(computed_rate_pps=rate_pps, planned_frames=planned_frames)
    rx_stats = ReceiverStats()
    target = (args.host, args.port)

    tx_stats.start_time = time.monotonic()
    send_start(sock, target, planned_frames, tx_stats)
    next_send = tx_stats.start_time
    end_time = tx_stats.start_time + args.duration

    seq = 0
    end_sent = False
    stop_reason = ""

    while True:
        now = time.monotonic()
        did_work = False

        if now < end_time:
            if now >= next_send:
                if now > next_send:
                    next_send = now
                frame_size = choose_frame_size(rng, args.min_size, args.size)
                payload_len = frame_size - HEADER_SIZE
                packet = build_packet(seq, 0, 0, payload_len)
                sock.sendto(packet, target)
                tx_stats.on_send(len(packet), is_start=False, is_end=False)
                if args.verbose:
                    print(
                        f"[tx] DATA seq={seq} bytes={len(packet)} "
                        f"payload={payload_len - CHECKSUM_SIZE} checksum={CHECKSUM_SIZE}"
                    )
                seq += 1
                next_send += effective_interval
                did_work = True
        elif not end_sent:
            send_end(sock, target, seq - 1, seq, tx_stats)
            end_sent = True
            did_work = True

        if recv_available(sock, rx_stats, args.verbose):
            stop_reason = "end_frame_received"
            break

        if end_sent:
            ref = rx_stats.last_rx_time if rx_stats.last_rx_time is not None else tx_stats.start_time
            if now - ref >= args.idle_timeout:
                stop_reason = f"idle_timeout_{args.idle_timeout:.1f}s"
                break

        if not did_work:
            time.sleep(0.001)

    tx_stats.end_time = time.monotonic()
    return tx_stats, rx_stats, stop_reason


def print_sender_stats(stats: SenderStats) -> None:
    runtime = max(0.0, stats.end_time - stats.start_time)
    rate = (stats.data_frames / runtime) if runtime > 0.0 else 0.0
    bps = (stats.bytes_total * 8.0 / runtime) if runtime > 0.0 else 0.0
    size_min = stats.size_min if stats.size_min is not None else 0
    size_max = stats.size_max if stats.size_max is not None else 0

    print("[sender] stats")
    print(
        f"  data_frames={stats.data_frames} start={stats.start_sent} end={stats.end_sent} "
        f"planned_frames={stats.planned_frames} runtime={runtime:.3f}s"
    )
    print(
        f"  computed_rate={stats.computed_rate_pps:.3f}/s actual_rate={rate:.3f}/s "
        f"tx_bps={bps:.1f} size_min={size_min} size_max={size_max}"
    )


def print_receiver_stats(stats: ReceiverStats, stop_reason: str) -> None:
    runtime = 0.0
    if stats.first_rx_time is not None and stats.last_rx_time is not None:
        runtime = stats.last_rx_time - stats.first_rx_time
    rate = (stats.data_packets / runtime) if runtime > 0.0 else 0.0
    bps = (stats.data_bytes_total * 8.0 / runtime) if runtime > 0.0 else 0.0
    size_min = stats.size_min if stats.size_min is not None else 0
    size_max = stats.size_max if stats.size_max is not None else 0
    unique = len(stats.seen)

    expected = stats.expected_total
    missing = None
    loss_pct = None
    if expected is not None and expected >= 0:
        missing = max(0, expected - unique)
        loss_pct = (missing * 100.0 / expected) if expected > 0 else 0.0
    elif stats.first_seq is not None and stats.last_seq is not None:
        expected_range = stats.last_seq - stats.first_seq + 1
        missing = max(0, expected_range - unique)

    print(f"[receiver] stats (stop_reason={stop_reason})")
    print(
        f"  packets={stats.packet_count} data_packets={stats.data_packets} "
        f"start={stats.start_packets} end={stats.end_packets} invalid={stats.invalid_packets} "
        f"invalid_checksum={stats.invalid_checksum_frames}"
    )
    print(
        f"  unique_data={unique} dup={stats.duplicates} out_of_order={stats.out_of_order} "
        f"missing_gaps={stats.missing_gaps}"
    )
    if expected is not None:
        print(f"  expected_total={expected} missing={missing} loss_pct={loss_pct:.2f}")
    elif missing is not None:
        print(f"  missing_estimate={missing} (based on observed seq range)")
    print(
        f"  seq_first={stats.first_seq} seq_last={stats.last_seq} "
        f"rate={rate:.3f}/s rx_bps={bps:.1f} size_min={size_min} size_max={size_max}"
    )


def main() -> None:
    args = parse_args()
    rate_pps = compute_rate_pps(args.baudrate, args.size)
    planned_frames = estimate_total_frames(args.duration, rate_pps)
    print(
        f"mode={args.mode} host={args.host} port={args.port} "
        f"size={args.size} min_size={args.min_size} baudrate={args.baudrate:.1f} "
        f"computed_rate={rate_pps:.3f}/s min_delay_ms={args.min_delay_ms:.3f} "
        f"duration={args.duration:.3f}s "
        f"planned_frames={planned_frames} idle_timeout={args.idle_timeout:.1f}s"
    )

    rng = random.Random(args.seed)
    sock = create_socket(args.mode, args.port)
    try:
        if args.mode == "sender":
            tx_stats = run_sender(sock, args, rng)
            print_sender_stats(tx_stats)
            return
        if args.mode == "receiver":
            rx_stats, stop_reason = run_receiver(sock, args)
            print_receiver_stats(rx_stats, stop_reason)
            return
        tx_stats, rx_stats, stop_reason = run_both(sock, args, rng)
        print_sender_stats(tx_stats)
        print_receiver_stats(rx_stats, stop_reason)
    finally:
        sock.close()


if __name__ == "__main__":
    main()
