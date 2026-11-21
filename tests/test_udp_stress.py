#!/usr/bin/env python3
"""Regression tests for UDP stress traffic pacing."""

import unittest
from unittest import mock

import udp_stress


class FakeClock:
    def __init__(self) -> None:
        self.now = 10.0
        self.sleeps: list[float] = []

    def monotonic(self) -> float:
        return self.now

    def sleep(self, duration: float) -> None:
        self.sleeps.append(duration)
        self.now += duration


class RecordingSocket:
    def __init__(self, clock: FakeClock) -> None:
        self.clock = clock
        self.send_times: list[float] = []

    def sendto(self, packet: bytes, target: tuple[str, int]) -> None:
        del packet, target
        self.send_times.append(self.clock.now)


class StartFramePacingTests(unittest.TestCase):
    def test_redundant_start_frames_use_configured_interval(self) -> None:
        clock = FakeClock()
        sock = RecordingSocket(clock)
        stats = udp_stress.SenderStats(computed_rate_pps=4.0, planned_frames=100)

        with (
            mock.patch.object(udp_stress.time, "monotonic", clock.monotonic),
            mock.patch.object(udp_stress.time, "sleep", clock.sleep),
        ):
            next_send = udp_stress.send_start(
                sock,
                ("192.0.2.1", 6969),
                total=100,
                stats=stats,
                count=3,
                interval=0.25,
                next_send=clock.now,
            )

        self.assertEqual([10.0, 10.25, 10.5], sock.send_times)
        self.assertEqual([0.25, 0.25], clock.sleeps)
        self.assertEqual(10.75, next_send)
        self.assertEqual(3, stats.start_sent)


if __name__ == "__main__":
    unittest.main()
