"""Record one USB request or replay a stream v1 file on the CM5."""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import time
import wave
import zlib

import numpy as np

from bench.evaluate import DEMO, evaluate
from bench.simulate import CRC, DATA, END, HEADER, MAGIC, START, VERSION


@dataclass
class Run:
    identifier: int
    source: int
    rate: int
    pcm: bytearray
    next_seq: int = 1
    flags: int = 0
    complete: bool = False
    fault: str = ""


def frames(chunks):
    """Yield decoded frames and None for malformed byte spans."""
    buf = bytearray()
    for chunk in chunks:
        buf.extend(chunk)
        while len(buf) >= len(MAGIC):
            start = buf.find(MAGIC)
            if start == -1:
                del buf[:-3]
                yield None
                break
            if start:
                del buf[:start]
                yield None
            if len(buf) < HEADER.size:
                break
            _, version, kind, source, reserved, size, run, seq, index, rate, flags = (
                HEADER.unpack_from(buf)
            )
            if (version != VERSION or source not in (1, 2) or reserved
                    or not run or not 0 < rate <= 0x7FFFFFFF or flags & ~7 or kind not in (START, DATA, END)
                    or (kind == DATA and (not 2 <= size <= 4096 or size % 2))
                    or (kind != DATA and size)):
                del buf[0]
                yield None
                continue
            end = HEADER.size + size
            if len(buf) < end + CRC.size:
                break
            if zlib.crc32(buf[:end]) != CRC.unpack_from(buf, end)[0]:
                del buf[0]
                yield None
                continue
            payload = bytes(buf[HEADER.size:end])
            del buf[:end + CRC.size]
            yield kind, source, run, seq, index, rate, flags, payload
    if buf:
        yield None


def runs(chunks, max_samples=1_000_000):
    active = None
    last_ids = set()
    for frame in frames(chunks):
        if frame is None:
            if active and not active.fault:
                active.fault = "Malformed frame or CRC"
            continue
        kind, source, identifier, seq, index, rate, flags, payload = frame
        if kind == START:
            if active:
                active.fault = active.fault or "Restart before END"
                yield active
            active = Run(identifier, source, rate, bytearray(), flags=flags)
            if seq or index or identifier in last_ids:
                active.fault = "Invalid START"
            last_ids.add(identifier)
            continue
        if active is None:
            continue
        active.flags |= flags
        if (source, identifier, rate, seq, index) != (
                active.source, active.identifier, active.rate, active.next_seq, len(active.pcm) // 2):
            active.fault = active.fault or "Run identity, sequence or sample index mismatch"
        active.next_seq += 1
        if kind == END:
            active.complete = bool(active.pcm and not active.fault and not active.flags)
            yield active
            active = None
        elif not active.fault:
            if len(active.pcm) + len(payload) > max_samples * 2:
                active.fault = "Run exceeds sample limit"
            else:
                active.pcm.extend(payload)
    if active:
        active.fault = active.fault or "Incomplete run at EOF or deadline"
        yield active


def store(output, run, model):
    result = evaluate(np.frombuffer(run.pcm, dtype="<i2"), complete=run.complete,
                      status_flags=run.flags, source=run.source, rate_hz=run.rate, model=model)
    result.update(run_id=run.identifier, rate_hz=run.rate, samples=len(run.pcm) // 2,
                  stream_fault=run.fault, status_flags=run.flags)
    # The JSON is the completion marker. WAV contains only the validated prefix.
    with wave.open(str(output / "audio.wav"), "wb") as audio:
        audio.setparams((1, 2, run.rate, 0, "NONE", "not compressed"))
        audio.writeframes(run.pcm)
    temp = output / "result.tmp"
    temp.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temp.rename(output / "result.json")
    return result


def serial_chunks(device, idle_timeout, run_timeout):
    import serial

    with serial.Serial(device, 115200, timeout=0.1, write_timeout=1) as port:
        port.reset_input_buffer()
        port.write(b"R")
        deadline = time.monotonic() + run_timeout
        last = time.monotonic()
        while time.monotonic() < deadline and time.monotonic() - last < idle_timeout:
            try:
                data = port.read(min(port.in_waiting or 1, 4096))
            except serial.SerialException:
                break
            if data:
                last = time.monotonic()
                yield data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--replay", type=Path)
    source.add_argument("--device")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--model", default=DEMO.model)
    parser.add_argument("--idle-timeout", type=float, default=2.0)
    parser.add_argument("--run-timeout", type=float, default=5.0)
    args = parser.parse_args()
    if not 0.1 <= args.idle_timeout <= args.run_timeout <= 60.0:
        parser.error("timeouts must satisfy 0.1 <= idle <= run <= 60 seconds")
    args.output.mkdir()
    if args.replay:
        with args.replay.open("rb") as stream:
            results = record(iter(lambda: stream.read(4096), b""), args)
    else:
        chunks = serial_chunks(args.device, args.idle_timeout, args.run_timeout)
        try:
            results = record(chunks, args)
        finally:
            chunks.close()
    return 0 if results and "INVALID" not in results else 1


def record(chunks, args):
    verdicts = []
    for count, run in enumerate(runs(chunks), 1):
        folder = args.output / f"run-{count:04d}"
        folder.mkdir()
        result = store(folder, run, args.model)
        print(json.dumps(result, allow_nan=False))
        verdicts.append(result["verdict"])
        if args.device:
            break
    if not verdicts:
        result = dict(verdict="INVALID", reason="No valid START received")
        (args.output / "result.json").write_text(json.dumps(result) + "\n", encoding="utf-8")
        print(json.dumps(result))
    return verdicts


if __name__ == "__main__":
    raise SystemExit(main())
