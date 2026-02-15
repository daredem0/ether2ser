#!/usr/bin/env python3
"""Send or receive monochrome images over UDP with simple chunking."""

import argparse
import os
import socket
import struct
import sys
import time
from io import BytesIO
from typing import Dict, Optional, Tuple

from PIL import Image

DEFAULT_HOST = "192.168.29.20"
DEFAULT_PORT = 6969
DEFAULT_SIZE = 1472  # Max safe payload to avoid IP fragmentation.
DEFAULT_RATE = 1.0
DEFAULT_IDLE_TIMEOUT = 10.0
DEFAULT_OUTPUT = "received_image.png"
MAX_DIMENSION = 256

# Header: seq(uint32), total_chunks(uint32), flags(uint8)
HEADER_STRUCT = struct.Struct("!IIB")
HEADER_SIZE = HEADER_STRUCT.size
FLAG_LAST = 0x01


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="UDP image sender/receiver with chunking.")
    parser.add_argument(
        "--mode",
        choices=("sender", "receiver"),
        default="sender",
        help="Run as sender or receiver.",
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Target host for sender mode.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port.")
    parser.add_argument(
        "--size",
        type=int,
        default=DEFAULT_SIZE,
        help="Total datagram size including header.",
    )
    parser.add_argument(
        "--rate",
        type=float,
        default=DEFAULT_RATE,
        help="Send rate in packets per second.",
    )
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=DEFAULT_IDLE_TIMEOUT,
        help="Receiver stop timeout in seconds without any packet.",
    )
    parser.add_argument(
        "--output",
        default=DEFAULT_OUTPUT,
        help="Output path for reconstructed image in receiver mode.",
    )
    parser.add_argument(
        "--broadcast",
        action="store_true",
        help="Enable SO_BROADCAST for sender (use with broadcast target).",
    )
    parser.add_argument(
        "image",
        nargs="?",
        help="PNG/JPG to send (required in sender mode).",
    )

    args = parser.parse_args()
    if args.size <= HEADER_SIZE:
        parser.error(f"--size must be > header ({HEADER_SIZE} bytes)")
    if args.mode == "sender" and not args.image:
        parser.error("image path is required in sender mode")
    if args.mode == "sender" and args.rate <= 0:
        parser.error("--rate must be > 0.")
    if args.idle_timeout <= 0:
        parser.error("--idle-timeout must be > 0.")
    return args


def create_socket(mode: str, port: int, broadcast: bool) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if broadcast:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    if mode == "receiver":
        sock.bind(("", port))
    sock.setblocking(False)
    return sock


def load_image_bytes(path: str) -> Tuple[bytes, Tuple[int, int]]:
    with Image.open(path) as img:
        img = img.convert("L")
        img.thumbnail((MAX_DIMENSION, MAX_DIMENSION), Image.LANCZOS)
        out = BytesIO()
        img.save(out, format="PNG", optimize=True)
        return out.getvalue(), img.size


def iter_chunks(payload: bytes, payload_size: int):
    total_chunks = (len(payload) + payload_size - 1) // payload_size
    if total_chunks == 0:
        total_chunks = 1
    for seq in range(total_chunks):
        start = seq * payload_size
        end = start + payload_size
        chunk = payload[start:end]
        flags = FLAG_LAST if seq == total_chunks - 1 else 0
        header = HEADER_STRUCT.pack(seq, total_chunks, flags)
        yield header + chunk, seq, total_chunks, flags


def send_image(sock: socket.socket, args: argparse.Namespace) -> None:
    payload_size = args.size - HEADER_SIZE
    data, dims = load_image_bytes(args.image)
    interval = 1.0 / args.rate
    next_send = time.monotonic()
    print(
        f"[sender] prepared {args.image} -> PNG {dims[0]}x{dims[1]} "
        f"({len(data)} bytes), chunk_payload={payload_size} bytes, rate={args.rate:.3f}/s"
    )
    for packet, seq, total, flags in iter_chunks(data, payload_size):
        now = time.monotonic()
        if now < next_send:
            time.sleep(next_send - now)
        else:
            next_send = now
        sock.sendto(packet, (args.host, args.port))
        tail = " (last)" if flags & FLAG_LAST else ""
        print(f"[tx] seq={seq}/{total-1} bytes={len(packet)} target={args.host}:{args.port}{tail}")
        next_send += interval
    print("[sender] done.")


def receive_image(sock: socket.socket, args: argparse.Namespace) -> Optional[str]:
    chunks: Dict[int, bytes] = {}
    expected_total: Optional[int] = None
    last_rx = time.monotonic()

    print(f"[receiver] listening on UDP *:{args.port} (timeout {args.idle_timeout}s)")
    while True:
        try:
            data, addr = sock.recvfrom(65535)
        except BlockingIOError:
            if time.monotonic() - last_rx >= args.idle_timeout:
                print("[receiver] idle timeout reached, stopping.")
                break
            time.sleep(0.001)
            continue

        last_rx = time.monotonic()
        if len(data) < HEADER_SIZE:
            print(f"[rx] dropped too-small datagram from {addr} (len={len(data)})")
            continue

        seq, total, flags = HEADER_STRUCT.unpack(data[:HEADER_SIZE])
        payload = data[HEADER_SIZE:]

        if expected_total is None:
            expected_total = total
            print(f"[receiver] expecting {expected_total} chunks")
        elif expected_total != total:
            print(f"[rx] warning: total_chunks changed {expected_total} -> {total}")
            expected_total = total

        if seq not in chunks:
            chunks[seq] = payload
            print(f"[rx] seq={seq}/{total-1} bytes={len(payload)} from {addr}")
        else:
            print(f"[rx] duplicate seq={seq} from {addr}")

        if flags & FLAG_LAST:
            expected_total = total

        if expected_total is not None and len(chunks) >= expected_total:
            print("[receiver] received all chunks.")
            break

    if not expected_total:
        print("[receiver] nothing received.")
        return None

    missing = [i for i in range(expected_total) if i not in chunks]
    if missing:
        print(f"[receiver] missing chunks: {missing}")
        return None

    ordered = b"".join(chunks[i] for i in range(expected_total))
    try:
        Image.open(BytesIO(ordered)).save(args.output, format="PNG")
    except Exception as exc:  # noqa: BLE001
        print(f"[receiver] failed to decode image: {exc}")
        return None

    print(f"[receiver] saved image to {args.output} ({len(ordered)} bytes)")
    return args.output


def main() -> None:
    args = parse_args()
    if args.mode == "sender":
        print(
            f"mode=sender target={args.host}:{args.port} size={args.size} rate={args.rate}/s "
            f"broadcast={'yes' if args.broadcast else 'no'}"
        )
    else:
        print(f"mode=receiver listen=0.0.0.0:{args.port} idle_timeout={args.idle_timeout}s")

    sock = create_socket(args.mode, args.port, args.broadcast)
    try:
        if args.mode == "sender":
            send_image(sock, args)
        else:
            receive_image(sock, args)
    finally:
        sock.close()


if __name__ == "__main__":
    main()
