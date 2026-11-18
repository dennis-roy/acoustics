import argparse
import json
from pathlib import Path
import tempfile
import unittest
import wave

from bench.receive import record, runs
from bench.simulate import CASES, fixture


class ReplayTest(unittest.TestCase):
    def test_every_fixture(self):
        expected = {
            "tone": ["PASS"], "silence": ["INVALID"], "clipping": ["INVALID"],
            "gap": ["INVALID"], "corruption": ["INVALID"], "restart": ["INVALID", "PASS"],
        }
        for case in CASES:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as tmp:
                dest = Path(tmp)
                data = b"".join(fixture(case))
                chunks = (data[i:i + 7] for i in range(0, len(data), 7))
                args = argparse.Namespace(output=dest, device=None, model="fixture")
                self.assertEqual(record(chunks, args), expected[case])
                folder = dest / f"run-{len(expected[case]):04d}"
                meta = json.loads((folder / "result.json").read_text())
                with wave.open(str(folder / "audio.wav")) as waveform:
                    self.assertEqual(waveform.getnframes(), meta["samples"])
                    self.assertEqual(waveform.getframerate(), meta["rate_hz"])

    def test_missing_end_invalidates_prefix(self):
        frames = list(fixture("tone"))
        rs = list(runs(frames[:-1]))
        self.assertFalse(rs[0].complete)
        self.assertEqual(len(rs[0].pcm), 51200)
