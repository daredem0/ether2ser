#!/usr/bin/env python3
import argparse
import os
import select
import socket
import struct
import sys
import time
from dataclasses import dataclass, field

DEFAULT_HOST = "192.168.29.20"
DEFAULT_PORT = 6969
DEFAULT_SIZE = 1472  # Max is 1472 to avoid IP fragmentation.
DEFAULT_RATE = 1.0
DEFAULT_DURATION = 10.0
DEFAULT_IDLE_TIMEOUT = 10.0


def make_payload(seq: int, nbytes: int) -> bytes:
    # Deterministic pattern that changes with sequence and byte position.
    return bytes((((seq * 31) + (i * 17) + (seq >> 8)) & 0xFF) for i in range(nbytes))


class EscKeyWatcher:
    """Best-effort ESC watcher for POSIX terminals."""

    def __init__(self) -> None:
        self._enabled = False
        self._fd = None
        self._old_term = None
        self._old_flags = None

    def __enter__(self) -> "EscKeyWatcher":
        if os.name != "posix" or not sys.stdin.isatty():
            return self
        try:
            import fcntl
            import termios
            import tty

            self._fd = sys.stdin.fileno()
            self._old_term = termios.tcgetattr(self._fd)
            self._old_flags = fcntl.fcntl(self._fd, fcntl.F_GETFL)
            tty.setcbreak(self._fd)
            fcntl.fcntl(self._fd, fcntl.F_SETFL, self._old_flags | os.O_NONBLOCK)
            self._enabled = True
        except Exception:
            self._enabled = False
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if not self._enabled or self._fd is None:
            return
        try:
            import fcntl
            import termios

            fcntl.fcntl(self._fd, fcntl.F_SETFL, self._old_flags)
            termios.tcsetattr(self._fd, termios.TCSADRAIN, self._old_term)
        except Exception:
            pass

    def esc_pressed(self) -> bool:
        if not self._enabled:
            return False
        try:
            ready, _, _ = select.select([sys.stdin], [], [], 0.0)
            if not ready:
                return False
            ch = sys.stdin.read(1)
            return ch == "\x1b"
        except Exception:
            return False


@dataclass
class SenderStats:
    packet_size: int
    payload_size: int
    target_rate_pps: float
    requested_duration_s: float
    sent: int = 0
    start_time: float = 0.0
    end_time: float = 0.0


@dataclass
class ReceiverStats:
    packet_count: int = 0
    unique_count: int = 0
    invalid_headers: int = 0
    total_bytes: int = 0
    first_seq: int | None = None
    last_seq: int | None = None
    expected_next: int = 0
    missing_gaps: int = 0
    out_of_order: int = 0
    duplicates: int = 0
    first_rx_time: float | None = None
    last_rx_time: float | None = None
    seen: set[int] = field(default_factory=set)
    matched: int = 0
    rtts_ms: list[float] = field(default_factory=list)

    def on_packet(self, data: bytes, now: float, sent_times: dict[int, float] | None) -> None:
        self.packet_count += 1
        self.total_bytes += len(data)

        if len(data) < 4:
            self.invalid_headers += 1
            print(f"[rx] invalid header (len={len(data)})")
            return

        seq = struct.unpack("!I", data[:4])[0]
        print(f"[rx] seq={seq} bytes={len(data)}")

        if self.first_rx_time is None:
            self.first_rx_time = now
        self.last_rx_time = now

        if self.first_seq is None:
            self.first_seq = seq
        self.last_seq = seq

        if seq in self.seen:
            self.duplicates += 1
        else:
            self.seen.add(seq)
            self.unique_count += 1

        if seq == self.expected_next:
            self.expected_next += 1
        elif seq > self.expected_next:
            gap = seq - self.expected_next
            self.missing_gaps += gap
            print(f"[rx] gap: expected={self.expected_next} got={seq} missed={gap}")
            self.expected_next = seq + 1
        else:
            self.out_of_order += 1

        if sent_times is not None and seq in sent_times:
            tx_t0 = sent_times.pop(seq)
            self.matched += 1
            self.rtts_ms.append((now - tx_t0) * 1000.0)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="UDP send/receive sequence test tool.")
    parser.add_argument(
        "--mode",
        choices=("sender", "receiver", "both"),
        default="both",
        help="Run as sender, receiver, or both (default).",
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Target host for sender mode.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port.")
    parser.add_argument("--size", type=int, default=DEFAULT_SIZE, help="Packet size in bytes.")
    parser.add_argument("--rate", type=float, default=DEFAULT_RATE, help="Packets per second.")
    parser.add_argument("--duration", type=float, default=DEFAULT_DURATION, help="Send duration in seconds.")
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=DEFAULT_IDLE_TIMEOUT,
        help="Receiver stop timeout in seconds without any packet.",
    )
    args = parser.parse_args()

    if args.mode in ("sender", "both"):
        if args.size < 12:
            parser.error("--size must be >= 12 when sending (header is 12 bytes).")
        if args.rate <= 0:
            parser.error("--rate must be > 0.")
        if args.duration <= 0:
            parser.error("--duration must be > 0.")
    if args.idle_timeout <= 0:
        parser.error("--idle-timeout must be > 0.")
    return args


def create_socket(mode: str, port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if mode in ("receiver", "both"):
        sock.bind(("", port))
    sock.setblocking(False)
    return sock


def recv_all(sock: socket.socket, rx: ReceiverStats, sent_times: dict[int, float] | None) -> int:
    received_now = 0
    while True:
        try:
            data, _ = sock.recvfrom(65535)
        except BlockingIOError:
            break
        except InterruptedError:
            continue
        rx.on_packet(data=data, now=time.monotonic(), sent_times=sent_times)
        received_now += 1
    return received_now


def print_sender_stats(stats: SenderStats) -> None:
    runtime = max(0.0, stats.end_time - stats.start_time)
    actual_rate = (stats.sent / runtime) if runtime > 0.0 else 0.0
    print("[sender] stats")
    print(
        f"  sent={stats.sent} packet_size={stats.packet_size} payload_size={stats.payload_size} "
        f"target_rate={stats.target_rate_pps:.3f}/s actual_rate={actual_rate:.3f}/s "
        f"duration={runtime:.3f}s requested_duration={stats.requested_duration_s:.3f}s"
    )


def print_receiver_stats(stats: ReceiverStats, stop_reason: str) -> None:
    first_seq = stats.first_seq if stats.first_seq is not None else -1
    last_seq = stats.last_seq if stats.last_seq is not None else -1
    avg_bytes = (stats.total_bytes / stats.packet_count) if stats.packet_count else 0.0
    if stats.first_rx_time is not None and stats.last_rx_time is not None:
        runtime = stats.last_rx_time - stats.first_rx_time
    else:
        runtime = 0.0
    rx_rate = (stats.packet_count / runtime) if runtime > 0.0 else 0.0

    start_gap = first_seq if first_seq > 0 else 0
    assumed_missing = start_gap + stats.missing_gaps
    continuity_ok = (
        stats.packet_count > 0
        and first_seq == 0
        and stats.missing_gaps == 0
        and stats.out_of_order == 0
        and stats.duplicates == 0
        and stats.invalid_headers == 0
    )

    print(f"[receiver] stats (stop_reason={stop_reason})")
    print(
        f"  packets={stats.packet_count} unique={stats.unique_count} avg_bytes={avg_bytes:.1f} "
        f"rate={rx_rate:.3f}/s"
    )
    print(
        f"  seq_first={first_seq} seq_last={last_seq} expected_next={stats.expected_next} "
        f"missing_assumed={assumed_missing} gaps={stats.missing_gaps} "
        f"duplicates={stats.duplicates} out_of_order={stats.out_of_order} "
        f"invalid_headers={stats.invalid_headers}"
    )
    print(f"  sequence_0_to_N_without_loss={'yes' if continuity_ok else 'no'}")
    if stats.rtts_ms:
        rtts = sorted(stats.rtts_ms)
        p50 = rtts[len(rtts) // 2]
        p95 = rtts[min(len(rtts) - 1, int(len(rtts) * 0.95))]
        print(
            f"  matched_echoes={stats.matched} "
            f"rtt_ms(min/p50/p95/max)={rtts[0]:.2f}/{p50:.2f}/{p95:.2f}/{rtts[-1]:.2f}"
        )


def run_sender(sock: socket.socket, args: argparse.Namespace, keys: EscKeyWatcher) -> SenderStats:
    payload_len = args.size - 12
    stats = SenderStats(
        packet_size=args.size,
        payload_size=payload_len,
        target_rate_pps=args.rate,
        requested_duration_s=args.duration,
        start_time=time.monotonic(),
    )
    next_send = stats.start_time
    send_end = stats.start_time + args.duration
    seq = 0
    interval = 1.0 / args.rate

    while time.monotonic() < send_end:
        if keys.esc_pressed():
            print("[sender] ESC pressed, stopping sender.")
            break

        now = time.monotonic()
        sent_now = False
        while now >= next_send and now < send_end:
            tx_t0 = time.monotonic()
            packet = struct.pack("!Id", seq, tx_t0) + make_payload(seq, payload_len)
            sock.sendto(packet, (args.host, args.port))
            print(f"[tx] seq={seq} bytes={len(packet)} target={args.host}:{args.port}")
            stats.sent += 1
            seq += 1
            next_send += interval
            sent_now = True
            now = time.monotonic()

        if not sent_now:
            sleep_time = max(0.0, next_send - now)
            time.sleep(min(0.001, sleep_time))

    stats.end_time = time.monotonic()
    return stats


def run_receiver(
    sock: socket.socket, args: argparse.Namespace, keys: EscKeyWatcher
) -> tuple[ReceiverStats, str]:
    stats = ReceiverStats()
    start_wait = time.monotonic()
    stop_reason = ""

    while True:
        if keys.esc_pressed():
            stop_reason = "esc"
            break

        received_now = recv_all(sock, stats, sent_times=None)
        now = time.monotonic()
        ref = stats.last_rx_time if stats.last_rx_time is not None else start_wait
        if now - ref >= args.idle_timeout:
            stop_reason = f"idle_timeout_{args.idle_timeout:.1f}s"
            break

        if received_now == 0:
            time.sleep(0.001)

    return stats, stop_reason


def run_both(
    sock: socket.socket, args: argparse.Namespace, keys: EscKeyWatcher
) -> tuple[SenderStats, ReceiverStats, str]:
    payload_len = args.size - 12
    tx_stats = SenderStats(
        packet_size=args.size,
        payload_size=payload_len,
        target_rate_pps=args.rate,
        requested_duration_s=args.duration,
        start_time=time.monotonic(),
    )
    rx_stats = ReceiverStats()
    sent_times: dict[int, float] = {}

    seq = 0
    interval = 1.0 / args.rate
    next_send = tx_stats.start_time
    send_end = tx_stats.start_time + args.duration
    stop_reason = ""

    while time.monotonic() < send_end:
        if keys.esc_pressed():
            stop_reason = "esc"
            break

        now = time.monotonic()
        did_work = False

        while now >= next_send and now < send_end:
            tx_t0 = time.monotonic()
            packet = struct.pack("!Id", seq, tx_t0) + make_payload(seq, payload_len)
            sock.sendto(packet, (args.host, args.port))
            sent_times[seq] = tx_t0
            print(f"[tx] seq={seq} bytes={len(packet)} target={args.host}:{args.port}")
            tx_stats.sent += 1
            seq += 1
            next_send += interval
            did_work = True
            now = time.monotonic()

        if recv_all(sock, rx_stats, sent_times=sent_times) > 0:
            did_work = True

        if not did_work:
            time.sleep(0.001)

    tx_stats.end_time = time.monotonic()

    if not stop_reason:
        drain_start = time.monotonic()
        while True:
            if keys.esc_pressed():
                stop_reason = "esc"
                break

            received_now = recv_all(sock, rx_stats, sent_times=sent_times)
            now = time.monotonic()
            ref = rx_stats.last_rx_time if rx_stats.last_rx_time is not None else drain_start

            if tx_stats.sent > 0 and rx_stats.matched >= tx_stats.sent:
                stop_reason = "all_sent_frames_received"
                break
            if now - ref >= args.idle_timeout:
                stop_reason = f"idle_timeout_{args.idle_timeout:.1f}s"
                break
            if received_now == 0:
                time.sleep(0.001)

    return tx_stats, rx_stats, stop_reason


def main() -> None:
    args = parse_args()
    print(
        f"mode={args.mode} host={args.host} port={args.port} size={args.size} "
        f"rate={args.rate}/s duration={args.duration}s idle_timeout={args.idle_timeout}s"
    )
    if args.mode in ("sender", "both"):
        print("Press ESC to stop early.")
    elif args.mode == "receiver":
        print("Press ESC to stop receiver.")

    sock = create_socket(args.mode, args.port)

    with EscKeyWatcher() as keys:
        if args.mode == "sender":
            tx_stats = run_sender(sock, args, keys)
            print_sender_stats(tx_stats)
            return

        if args.mode == "receiver":
            rx_stats, stop_reason = run_receiver(sock, args, keys)
            print_receiver_stats(rx_stats, stop_reason)
            return

        tx_stats, rx_stats, stop_reason = run_both(sock, args, keys)
        print_sender_stats(tx_stats)
        print_receiver_stats(rx_stats, stop_reason)


if __name__ == "__main__":
    main()
