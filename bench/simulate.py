"""Deterministic offline fixtures for the acoustic USB stream."""
import argparse
from pathlib import Path
import struct
import zlib

MAGIC = b"ACOU"
VERSION = 1
START, DATA, END = 1, 2, 3
SYNTHETIC = 1
RUN_ID = 1
DEFAULT_RATE = 100_000
HEADER = struct.Struct("<4sBBBBIQIQII")
CRC = struct.Struct("<I")
TONE = (0, 7071, 10000, 7071, 0, -7071, -10000, -7071)
SAMPLES_PER_FRAME = 256
FRAME_COUNT = 100
CASES = ("tone", "silence", "clipping", "gap", "corruption", "restart")
GOLDEN = bytes.fromhex(
    "41434f5501020100040000000100000000000000010000000000000000000000a0860100000000007b0038fe95bf38a1"
)


def encode_frame(kind, run_id, sequence, sample_index, rate, samples=()):
    """Encode one fixture frame with a CRC over header and payload."""
    payload = struct.pack(f"<{len(samples)}h", *samples)
    header = HEADER.pack(
        MAGIC, VERSION, kind, SYNTHETIC, 0,
        len(payload), run_id, sequence, sample_index, rate, 0,
    )
    frame = header + payload
    return frame + CRC.pack(zlib.crc32(frame))


def fixture(case, rate=DEFAULT_RATE):
    """Yield a complete or intentionally damaged recording."""
    if case not in CASES:
        raise ValueError(f"unknown fixture: {case}")
    if not 0 < rate <= 0xFFFFFFFF:
        raise ValueError("sample rate must be a positive uint32")
    wave = TONE
    if case == "silence":
        wave = (0,)
    elif case == "clipping":
        wave = (32767, -32768)
    samples = wave * (SAMPLES_PER_FRAME // len(wave))
    yield encode_frame(START, RUN_ID, 0, 0, rate)
    for block in range(FRAME_COUNT):
        if case == "gap" and block == 1:
            continue
        if case == "restart" and block == 1:
            yield from restarted_run(rate, samples)
            return
        packet = encode_frame(
            DATA, RUN_ID, block + 1, block * SAMPLES_PER_FRAME, rate, samples,
        )
        if case == "corruption" and block == 1:
            packet = packet[:HEADER.size] + bytes([packet[HEADER.size] ^ 1]) + packet[HEADER.size + 1:]
        yield packet
    yield encode_frame(END, RUN_ID, FRAME_COUNT + 1, FRAME_COUNT * SAMPLES_PER_FRAME, rate)


def restarted_run(rate, samples):
    """Start a different run after intentionally leaving the first open."""
    yield encode_frame(START, RUN_ID + 1, 0, 0, rate)
    for block in range(FRAME_COUNT):
        yield encode_frame(
            DATA, RUN_ID + 1, block + 1, block * SAMPLES_PER_FRAME, rate, samples,
        )
    yield encode_frame(END, RUN_ID + 1, FRAME_COUNT + 1, FRAME_COUNT * SAMPLES_PER_FRAME, rate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=CASES)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE)
    args = parser.parse_args()
    if not 0 < args.rate <= 0xFFFFFFFF:
        parser.error("--rate must be a positive uint32")
    if encode_frame(DATA, RUN_ID, 1, 0, DEFAULT_RATE, (123, -456)) != GOLDEN:
        raise RuntimeError("protocol golden frame mismatch")
    # Exclusive creation protects an existing recording from overwrite.
    with args.output.open("xb") as output:
        for packet in fixture(args.case, args.rate):
            output.write(packet)


if __name__ == "__main__":
    main()
