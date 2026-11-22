#!/usr/bin/env python3
"""Shared Linux USB CDC consoles for humans and command-line automation.

Only the broker opens the serial ports. Clients exchange newline-delimited JSON
over a private Unix socket. No third-party Python packages are required.
"""

import argparse
import asyncio
import base64
import contextlib
import fcntl
import json
import os
from pathlib import Path
import select
import signal
import re
import subprocess
import sys
import termios
import time
import tty

PROMPT = b"ether2ser> "
QUIET = 0.25  # Firmware may emit prompts during character echo and log draining.
RESPONSE_END = re.compile(
    rb"(?m)^@response [0-9]+ end (?P<status>ok|accepted|error [a-z_]+)\r?$")
DEFAULT_STATE = Path(f"/tmp/ether2ser-console-{os.getuid()}")


def encode(value):
    return (json.dumps(value) + "\n").encode()


class Board:
    def __init__(self, name, path, state):
        self.name, self.path = name, path
        self.fd = None
        self.lock = asyncio.Lock()
        self.listeners = set()
        self.changed = asyncio.Event()
        self.response = None
        self.last_rx = 0.0
        self.needs_sync = True
        self.history = bytearray()
        self.log = (state / f"{name}.jsonl").open("a", buffering=1)

    def publish(self, kind, data):
        event = {"time": time.time(), "board": self.name, "kind": kind, "data": data}
        self.log.write(json.dumps(event) + "\n")
        packet = encode(event)
        for writer in tuple(self.listeners):
            if writer.is_closing() or writer.transport.get_write_buffer_size() > 262144:
                writer.close()
                self.listeners.discard(writer)
            else:
                writer.write(packet)

    def disconnect(self, reason):
        if self.fd is not None:
            asyncio.get_running_loop().remove_reader(self.fd)
            os.close(self.fd)
            self.fd = None
        self.needs_sync = True
        self.changed.set()
        self.publish("connection", reason)

    def read_ready(self):
        try:
            data = os.read(self.fd, 65536)
            if not data:
                self.disconnect("disconnected")
                return
        except BlockingIOError:
            return
        except OSError as error:
            self.disconnect(str(error))
            return
        self.last_rx = time.monotonic()
        self.history.extend(data)
        del self.history[:-16384]
        if self.response is not None:
            self.response.extend(data)
            # Bound responses even if a target streams logs indefinitely.
            if len(self.response) > 1048576:
                del self.response[:-1048576]
        self.publish("rx", base64.b64encode(data).decode())
        self.changed.set()

    async def maintain(self):
        last_error = None
        while True:
            if self.fd is None:
                fd = None
                try:
                    fd = os.open(self.path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    fcntl.ioctl(fd, termios.TIOCEXCL)
                    tty.setraw(fd)
                    attrs = termios.tcgetattr(fd)
                    attrs[2] |= termios.CLOCAL | termios.CREAD
                    attrs[2] &= ~(termios.CRTSCTS | termios.HUPCL)
                    attrs[4] = attrs[5] = termios.B115200
                    termios.tcsetattr(fd, termios.TCSANOW, attrs)
                    self.fd = fd
                    asyncio.get_running_loop().add_reader(fd, self.read_ready)
                    self.publish("connection", f"connected: {self.path}; sync before sending")
                    last_error = None
                except OSError as error:
                    if fd is not None:
                        os.close(fd)
                    if str(error) != last_error:
                        self.publish("connection", str(error))
                        last_error = str(error)
            await asyncio.sleep(0.5)

    async def command(self, command, source, timeout, sync=False, framed=True):
        if not isinstance(command, str) or any(ord(c) < 32 or ord(c) > 126 for c in command):
            raise ValueError("commands must contain only printable ASCII on one line")
        limit = 126 if framed and not sync else 127
        if len(command) > limit:
            raise ValueError(f"firmware command limit is {limit} bytes for this client")
        if not 0.5 <= timeout <= 120:
            raise ValueError("timeout must be between 0.5 and 120 seconds")
        async with self.lock:
            if self.fd is None:
                raise ValueError("device disconnected; broker will retry the USB path")
            if self.needs_sync and not sync:
                raise ValueError(f"console needs synchronization: run sync {self.name}")
            self.response = bytearray()
            self.publish("tx", {"source": source, "command": command})
            deadline = time.monotonic() + timeout
            try:
                # A blank line for sync completes any old partial CLI input.
                prefix = "@" if framed and not sync else ""
                data = (prefix + command + "\r").encode("ascii")
                while data:
                    try:
                        count = os.write(self.fd, data)
                        data = data[count:]
                    except BlockingIOError:
                        if time.monotonic() >= deadline:
                            raise TimeoutError("serial write timed out")
                        await asyncio.sleep(0.01)
                while True:
                    if self.fd is None:
                        raise OSError("device disconnected during command")
                    now = time.monotonic()
                    if not sync and framed:
                        match = RESPONSE_END.search(self.response)
                        if match is not None:
                            self.needs_sync = False
                            output = self.response.decode(errors="replace")
                            status = match.group("status").decode()
                            if status.startswith("error "):
                                return {"ok": False, "error": status[6:], "output": output}
                            return {"ok": True, "output": output}
                    elif PROMPT in self.response and now - self.last_rx >= QUIET:
                        self.needs_sync = False
                        return {"ok": True, "output": self.response.decode(errors="replace")}
                    if now >= deadline:
                        expected = "quiet firmware prompt" if sync else "firmware response terminator"
                        raise TimeoutError(f"no {expected} before timeout; command was not retried")
                    self.changed.clear()
                    with contextlib.suppress(asyncio.TimeoutError):
                        await asyncio.wait_for(self.changed.wait(), min(QUIET, deadline - now))
            except (OSError, TimeoutError) as error:
                self.needs_sync = True
                self.publish("error", str(error))
                return {"ok": False, "error": str(error),
                        "output": self.response.decode(errors="replace")}
            finally:
                self.response = None

    def close(self):
        if self.fd is not None:
            asyncio.get_running_loop().remove_reader(self.fd)
            with contextlib.suppress(OSError):
                fcntl.ioctl(self.fd, termios.TIOCNXCL)
            os.close(self.fd)
            self.fd = None
        for writer in self.listeners:
            writer.close()
        self.log.close()


async def serve(state, devices):
    state.mkdir(mode=0o700, parents=True, exist_ok=True)
    if state.is_symlink() or state.stat().st_uid != os.getuid():
        raise ValueError("state directory must be owned by this user and not a symlink")
    state.chmod(0o700)
    with (state / "broker.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        sock = state / "broker.sock"
        sock.unlink(missing_ok=True)
        boards = {name: Board(name, path, state) for name, path in devices.items()}
        stopping = asyncio.Event()
        client_tasks = set()
        client_writers = set()
        loop = asyncio.get_running_loop()
        for sig in (signal.SIGTERM, signal.SIGINT):
            loop.add_signal_handler(sig, stopping.set)

        async def handle(reader, writer):
            task = asyncio.current_task()
            client_tasks.add(task)
            client_writers.add(writer)
            board = None
            stop_requested = False
            try:
                request = json.loads(await asyncio.wait_for(reader.readline(), 5))
                op = request["op"]
                if op == "status":
                    result = {"ok": True, "boards": {
                        n: {"path": b.path, "connected": b.fd is not None,
                            "needs_sync": b.needs_sync} for n, b in boards.items()}}
                elif op == "stop":
                    result = {"ok": True}
                    stop_requested = True
                else:
                    board = boards[request["board"]]
                    if op == "watch":
                        writer.write(encode({"kind": "rx", "data": base64.b64encode(board.history).decode()}))
                        board.listeners.add(writer)
                        await writer.drain()
                        await reader.read()
                        return
                    if op not in ("send", "sync"):
                        raise ValueError("unknown operation")
                    result = await board.command(
                        request.get("command", ""), request.get("source", "agent"),
                        float(request.get("timeout", 8)), op == "sync",
                        bool(request.get("framed", True)))
                writer.write(encode(result))
                await writer.drain()
                if stop_requested:
                    client_tasks.discard(task)
                    stopping.set()
            except (ValueError, KeyError, asyncio.TimeoutError) as error:
                writer.write(encode({"ok": False, "error": str(error)}))
                with contextlib.suppress(ConnectionError):
                    await writer.drain()
            except (ConnectionError, BrokenPipeError):
                pass
            finally:
                if board is not None:
                    board.listeners.discard(writer)
                client_writers.discard(writer)
                writer.close()
                client_tasks.discard(task)
        tasks = [asyncio.create_task(board.maintain()) for board in boards.values()]
        server = await asyncio.start_unix_server(handle, path=str(sock))
        sock.chmod(0o600)
        try:
            async with server:
                await stopping.wait()
                for writer in tuple(client_writers):
                    writer.transport.abort()
                active_clients = tuple(client_tasks)
                for task in active_clients:
                    task.cancel()
                await asyncio.gather(*active_clients, return_exceptions=True)
        finally:
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            for board in boards.values():
                board.close()
            sock.unlink(missing_ok=True)


async def rpc(state, request):
    reader, writer = await asyncio.open_unix_connection(str(state / "broker.sock"), limit=2097152)
    try:
        writer.write(encode(request))
        await writer.drain()
        data = await reader.readline()
        if not data:
            raise ValueError("broker closed the connection")
        return json.loads(data)
    finally:
        writer.close()
        await writer.wait_closed()


def client(state, board):
    """A line-editing terminal client; partial human input never reaches serial."""
    import socket
    if not sys.stdin.isatty():
        raise ValueError("client requires a terminal; use send for automation")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(str(state / "broker.sock"))
        sock.sendall(encode({"op": "watch", "board": board}))
        old = termios.tcgetattr(sys.stdin)
        line = bytearray()
        pending = b""
        output_pending = ""
        jobs = set()
        # Run RPCs in a thread so reading both consoles remains responsive.
        from concurrent.futures import ThreadPoolExecutor
        with ThreadPoolExecutor(max_workers=1) as pool:
            def submit(command):
                result = asyncio.run(rpc(state, {"op": "sync" if not command else "send",
                                                "board": board, "command": command,
                                                "source": "human", "framed": False}))
                return result

            print(f"{board}: Enter sends a complete line; empty Enter syncs; Ctrl-C/D exits.\r")
            try:
                tty.setraw(sys.stdin)
                while True:
                    sys.stdout.write("\r\033[2K[input] " + line.decode())
                    sys.stdout.flush()
                    ready, _, _ = select.select([sock, sys.stdin], [], [], 0.1)
                    for job in tuple(jobs):
                        if job.done():
                            jobs.remove(job)
                            try:
                                result = job.result()
                                if not result["ok"]:
                                    print("\r\033[2K[error] " + result["error"] + "\r")
                            except Exception as error:
                                print(f"\r\033[2K[error] {error}\r")
                    if sock in ready:
                        data = sock.recv(65536)
                        if not data:
                            break
                        pending += data
                        while b"\n" in pending:
                            raw, pending = pending.split(b"\n", 1)
                            event = json.loads(raw)
                            sys.stdout.write("\r\033[2K")
                            if event["kind"] == "rx":
                                output_pending += base64.b64decode(event["data"]).decode(errors="replace")
                                while "\n" in output_pending:
                                    text, output_pending = output_pending.split("\n", 1)
                                    sys.stdout.write(text.rstrip("\r") + "\r\n")
                                if output_pending.endswith(PROMPT.decode()) or len(output_pending) > 16384:
                                    sys.stdout.write(output_pending + "\r\n")
                                    output_pending = ""
                            else:
                                sys.stdout.write(f"\r\n[{event['kind']}] {event['data']}\r\n")
                            sys.stdout.flush()
                    if sys.stdin in ready:
                        char = os.read(sys.stdin.fileno(), 1)
                        if char in (b"\x03", b"\x04", b""):
                            break
                        if char in (b"\r", b"\n"):
                            if len(jobs) >= 8:
                                print("\r\033[2K[error] too many pending commands\r")
                                continue
                            jobs.add(pool.submit(submit, line.decode()))
                            line.clear()
                        elif char in (b"\x7f", b"\x08"):
                            del line[-1:]
                        elif 32 <= char[0] <= 126 and len(line) < 127:
                            line.extend(char)
            finally:
                termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)
                print()


def device_map(specs):
    result = {}
    for spec in specs:
        name, path = spec.split("=", 1)
        if not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in name):
            raise ValueError("device names must use lowercase letters, digits, - or _")
        if name in result or os.path.realpath(path) in map(os.path.realpath, result.values()):
            raise ValueError("duplicate board name or device")
        result[name] = os.path.abspath(path)
    return result


def launch(args):
    devices = device_map(args.device)
    state = args.state_dir
    try:
        status = asyncio.run(rpc(state, {"op": "status"}))
    except (OSError, ValueError):
        state.mkdir(mode=0o700, parents=True, exist_ok=True)
        with (state / "broker.log").open("a") as log:
            proc = subprocess.Popen([sys.executable, str(Path(__file__).resolve()),
                                     "--state-dir", str(state), "serve", *sum(
                                         (["--device", f"{n}={p}"] for n, p in devices.items()), [])],
                                    stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                    start_new_session=True)
        for _ in range(50):
            if proc.poll() is not None:
                raise ValueError(f"broker failed; inspect {state / 'broker.log'}")
            try:
                status = asyncio.run(rpc(state, {"op": "status"}))
                break
            except (OSError, ValueError):
                time.sleep(0.1)
        else:
            raise ValueError(f"broker startup timed out; inspect {state / 'broker.log'}")
    actual = {n: b["path"] for n, b in status["boards"].items()}
    if actual != devices:
        raise ValueError("existing broker has different devices; stop it or choose another --state-dir")
    python, script = sys.executable, str(Path(__file__).resolve())
    panes = []
    for name in devices:
        values = [script, "--state-dir", str(state), "client", name]
        panes.append(f'    pane name={json.dumps(name)} command={json.dumps(python)} {{\n'
                     '        args ' + ' '.join(map(json.dumps, values)) + '\n    }')
    layout = state / "console.kdl"
    layout.write_text('layout {\n  pane split_direction="vertical" {\n' +
                      '\n'.join(panes) + '\n  }\n}\n')
    command = ["zellij", "--layout", str(layout), "attach", "--create", args.session]
    env = dict(os.environ)
    for name in ("ZELLIJ", "ZELLIJ_SESSION_NAME", "ZELLIJ_PANE_ID"):
        env.pop(name, None)
    with (state / "terminal.log").open("a") as log:
        terminal = subprocess.Popen(["alacritty", "--hold", "--title", "ether2ser consoles", "-e", *command],
                                    env=env, stdout=log, stderr=log, start_new_session=True)
    time.sleep(0.5)
    if terminal.poll() is not None:
        raise ValueError(f"terminal exited; inspect {state / 'terminal.log'}")
    print(f"Opened consoles. Broker/logs: {state}\nStop broker: {python} {script} --state-dir {state} stop")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-dir", type=Path, default=DEFAULT_STATE)
    subs = parser.add_subparsers(dest="op", required=True)
    for op in ("serve", "launch"):
        sub = subs.add_parser(op)
        sub.add_argument("--device", action="append", required=True, metavar="NAME=PATH")
        if op == "launch":
            sub.add_argument("--session", default="ether2ser")
    sub = subs.add_parser("client")
    sub.add_argument("board")
    for op in ("send", "sync"):
        sub = subs.add_parser(op)
        sub.add_argument("board")
        if op == "send":
            sub.add_argument("command", help="one quoted firmware command")
        sub.add_argument("--timeout", type=float, default=8)
        sub.add_argument("--json", action="store_true")
    subs.add_parser("status")
    subs.add_parser("stop")
    args = parser.parse_args()
    args.state_dir = args.state_dir.absolute()
    try:
        if args.op == "serve":
            asyncio.run(serve(args.state_dir, device_map(args.device)))
        elif args.op == "launch":
            launch(args)
        elif args.op == "client":
            client(args.state_dir, args.board)
        else:
            request = {"op": args.op}
            if args.op in ("send", "sync"):
                request.update(board=args.board, command=getattr(args, "command", ""), timeout=args.timeout)
            result = asyncio.run(rpc(args.state_dir, request))
            if args.op == "status" or getattr(args, "json", False):
                print(json.dumps(result, indent=2))
            else:
                print(result.get("output", ""), end="")
                if not result["ok"]:
                    print(result["error"], file=sys.stderr)
            return 0 if result["ok"] else 1
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
