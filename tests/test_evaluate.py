import unittest
import numpy as np
from bench.evaluate import DEMO, evaluate
from bench.simulate import TONE


class EvaluationTest(unittest.TestCase):
    def test_fixture_verdicts(self):
        tone = np.tile(np.array(TONE, dtype=np.int16), 3200)
        meta = dict(complete=True, status_flags=0, source=1, rate_hz=100000, model=DEMO.model)
        for samples, changes, expected in (
            (tone, {}, "PASS"), (tone * 2, {}, "FAIL"),
            (tone * 0, {}, "INVALID"), (tone[:100], {}, "INVALID"),
            (tone, {"complete": False}, "INVALID"), (tone, {"status_flags": 1}, "INVALID"),
            (tone, {"source": 2}, "HOLD"), (tone, {"model": "unknown"}, "HOLD"),
        ):
            with self.subTest(expected=expected, changes=changes):
                self.assertEqual(evaluate(samples, **(meta | changes))["verdict"], expected)
        features = evaluate(tone, **meta)["features"]
        self.assertAlmostEqual(features["peak_hz"], 12500)
        self.assertAlmostEqual(features["band_rms_fs"], features["rms_fs"], places=5)
        tone[100] = 32767
        self.assertEqual(evaluate(tone, **meta)["verdict"], "INVALID")
