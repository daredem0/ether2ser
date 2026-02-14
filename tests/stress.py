import socket, time, struct, statistics, argparse

HOST = "192.168.29.20"
PORT = 6969
# Max is 1472, otherwise we get fragmentation
SIZE = 1472         # payload size
RATE = 1  # packets per second
DURATION = 10      # seconds

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.0)
sock.bind(("", 6969))

interval = 1.0 / RATE
end = time.monotonic() + DURATION
seq = 0
sent = 0
recv_total = 0
recv_matched = 0
rtts = []
sent_times = {}
last_sent_seq = None
last_id_received = False

payload_len = max(0, SIZE - 12)

def make_payload(seq, nbytes):
    # Deterministic changing pattern: depends on packet sequence and byte index.
    return bytes((((seq * 31) + (i * 17) + (seq >> 8)) & 0xFF) for i in range(nbytes))

def try_recv():
    global recv_total, recv_matched, last_id_received
    while True:
        try:
            data, _ = sock.recvfrom(2048)
        except BlockingIOError:
            return
        if len(data) < 12:
            continue
        s, t0 = struct.unpack("!Id", data[:12])
        recv_total += 1
        print(f"recv_id={s} last_sent_id={last_sent_seq}")
        if s in sent_times:
            recv_matched += 1
            rtts.append((time.monotonic() - t0) * 1000.0)
            del sent_times[s]
            if last_sent_seq is not None and s == last_sent_seq:
                last_id_received = True

next_send = time.monotonic()
while time.monotonic() < end:
    now = time.monotonic()
    if now >= next_send:
        pkt = struct.pack("!Id", seq, now) + make_payload(seq, payload_len)
        sock.sendto(pkt, (HOST, PORT))
        sent_times[seq] = now
        last_sent_seq = seq
        seq += 1
        sent += 1
        next_send += interval
    try_recv()

# Drain a bit
drain_until = time.monotonic() + 60.0
while time.monotonic() < drain_until:
    try_recv()
    if last_id_received:
        break

loss = sent - recv_matched
print(f"sent={sent} recv={recv_matched} loss={loss} loss%={loss*100.0/sent:.2f}")
print(f"recv_total={recv_total}")
if rtts:
    rtts.sort()
    print(f"rtt ms: min={rtts[0]:.2f} p50={rtts[len(rtts)//2]:.2f} "
          f"p95={rtts[int(len(rtts)*0.95)]:.2f} max={rtts[-1]:.2f}")
