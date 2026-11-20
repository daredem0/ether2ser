"""Host integration checks with two emulated USB consoles; no boards required."""

import asyncio
import base64
import importlib.util
import json
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import tempfile
import threading
import time
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools/serial_console.py"
spec = importlib.util.spec_from_file_location("serial_console", SCRIPT)
console = importlib.util.module_from_spec(spec)
spec.loader.exec_module(console)


class FakeBoard:
    def __init__(self, directory, name):
        self.path = directory / name
        self.commands = []
        self.done = threading.Event()
        self.master, self.slave = pty.openpty()
        self.path.symlink_to(os.ttyname(self.slave))
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        pending = b""
        while not self.done.is_set():
            if not select.select([self.master], [], [], 0.05)[0]:
                continue
            try:
                pending += os.read(self.master, 1024)
                while b"\r" in pending:
                    command, pending = pending.split(b"\r", 1)
                    self.commands.append(command.decode())
                    if command == b"timeout":
                        continue
                    # The real CLI can emit intermediate prompts while echoing.
                    os.write(self.master, command + b"\r\nether2ser> ")
                    time.sleep(0.08)
                    os.write(self.master, b"\r\nresult:" + command + b"\r\nether2ser> ")
            except OSError:
                break

    def close(self):
        self.done.set()
        self.thread.join(1)
        os.close(self.master)
        os.close(self.slave)


class SharedConsoleTest(unittest.TestCase):
    def test_two_boards_shared_clients_and_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            state = root / "state"
            a, b = FakeBoard(root, "a"), FakeBoard(root, "b")
            proc = subprocess.Popen([sys.executable, str(SCRIPT), "--state-dir", str(state),
                                     "serve", "--device", f"a={a.path}", "--device", f"b={b.path}"],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            try:
                for _ in range(100):
                    if (state / "broker.sock").exists():
                        break
                    if proc.poll() is not None:
                        self.fail(proc.stderr.read().decode())
                    time.sleep(0.02)
                asyncio.run(self.exercise(state, a, b))
                _, stderr = proc.communicate(timeout=5)
                self.assertEqual(0, proc.returncode, stderr.decode())
                # JSONL preserves TX attribution and RX bytes for later analysis.
                events = [json.loads(line) for line in (state / "a.jsonl").read_text().splitlines()]
                self.assertTrue(any(e["kind"] == "tx" and e["data"]["source"] == "human" for e in events))
                self.assertTrue(any(e["kind"] == "rx" for e in events))
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.communicate(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.communicate(timeout=5)
                a.close()
                b.close()

    async def exercise(self, state, a, b):
        async def request(op, board="a", **kwargs):
            return await console.rpc(state, {"op": op, "board": board, **kwargs})

        for _ in range(50):
            status = await request("status")
            if all(board["connected"] for board in status["boards"].values()):
                break
            await asyncio.sleep(0.02)
        self.assertFalse((await request("send", command="status"))["ok"])
        for board in ("a", "b"):
            self.assertTrue((await request("sync", board))["ok"])
        reader, writer = await asyncio.open_unix_connection(str(state / "broker.sock"))
        writer.write(console.encode({"op": "watch", "board": "a"}))
        await writer.drain()
        await reader.readline()  # history
        results = await asyncio.gather(
            request("send", command="status", source="human"),
            request("send", command="net", source="agent"),
            request("send", "b", command="mem"))
        self.assertTrue(all(result["ok"] for result in results), results)
        self.assertIn("result:status", results[0]["output"])
        self.assertNotIn("result:net", results[0]["output"])
        self.assertIn("result:net", results[1]["output"])
        self.assertEqual(a.commands[-2:], ["status", "net"])
        self.assertEqual(b.commands[-1], "mem")
        seen = []
        while True:
            event = json.loads(await asyncio.wait_for(reader.readline(), 2))
            seen.append(event)
            if event["kind"] == "rx" and b"result:net" in base64.b64decode(event["data"]):
                break
        self.assertEqual(sum(e["kind"] == "tx" for e in seen), 2)
        self.assertFalse((await request("send", command="x" * 128))["ok"])
        self.assertFalse((await request("send", command="status\nreboot"))["ok"])
        failed = await request("send", command="timeout", timeout=0.5)
        self.assertFalse(failed["ok"])
        self.assertFalse((await request("send", command="net"))["ok"])
        self.assertTrue((await request("sync"))["ok"])
        self.assertTrue((await request("send", command="status"))["ok"])
        writer.close()
        await writer.wait_closed()
        # An unfinished human command stays in the client while agent input runs.
        terminal_master, terminal_slave = pty.openpty()
        human = subprocess.Popen([sys.executable, str(SCRIPT), "--state-dir", str(state),
                                  "client", "a"], stdin=terminal_slave, stdout=terminal_slave,
                                 stderr=terminal_slave)
        try:
            for _ in range(50):
                if select.select([terminal_master], [], [], 0)[0]:
                    if b"[input]" in os.read(terminal_master, 65536):
                        break
                await asyncio.sleep(0.02)
            os.write(terminal_master, b"stat")
            self.assertTrue((await request("send", command="net"))["ok"])
            os.write(terminal_master, b"us\r")
            for _ in range(100):
                if a.commands[-2:] == ["net", "status"]:
                    break
                await asyncio.sleep(0.02)
            self.assertEqual(a.commands[-2:], ["net", "status"])
            os.write(terminal_master, b"\x03")
            await asyncio.to_thread(human.wait, 3)
            self.assertEqual(human.returncode, 0)
        finally:
            if human.poll() is None:
                human.terminate()
                human.wait(timeout=3)
            os.close(terminal_master)
            os.close(terminal_slave)
        # Remove the target path, disconnect, then present a new PTY on the same path.
        a.path.unlink()
        a.close()
        for _ in range(50):
            if not (await request("status"))["boards"]["a"]["connected"]:
                break
            await asyncio.sleep(0.02)
        replacement = FakeBoard(a.path.parent, a.path.name)
        try:
            for _ in range(100):
                if (await request("status"))["boards"]["a"]["connected"]:
                    break
                await asyncio.sleep(0.02)
            self.assertFalse((await request("send", command="net"))["ok"])
            self.assertTrue((await request("sync"))["ok"])
            self.assertTrue((await request("send", command="net"))["ok"])
            watch_reader, watch_writer = await asyncio.open_unix_connection(
                str(state / "broker.sock"))
            watch_writer.write(console.encode({"op": "watch", "board": "a"}))
            await watch_writer.drain()
            await watch_reader.readline()
            self.assertTrue((await request("stop"))["ok"])
            self.assertEqual(b"", await asyncio.wait_for(watch_reader.read(), 2))
            watch_writer.close()
            await watch_writer.wait_closed()
        finally:
            replacement.close()
            # The outer fixture still owns a, but its descriptors were already closed.
            a.close = lambda: None


if __name__ == "__main__":
    unittest.main()
