"""Evaluate one assembled mono PCM run. NumPy is the only dependency."""
from dataclasses import dataclass
import math

import numpy as np


@dataclass(frozen=True)
class Recipe:
    model: str
    revision: str
    rate_hz: int
    minimum_samples: int
    band_hz: tuple[float, float]
    max_rms_fs: float
    max_band_rms_fs: float


# Illustrative software fixture limits in digital full-scale units.
DEMO = Recipe("fixture", "demo-1", 100000, 20000, (11000, 14000), 0.25, 0.25)


def evaluate(samples, *, complete, status_flags, source, rate_hz, model, recipe=DEMO):
    """Source and status flags follow protocol v1. Pass only verified PCM here.

    The receiver supplies complete=True only after validating the whole run,
    including END, integrity, sequence, sample count and session identity.
    """
    result = dict(verdict="INVALID", reason="", model=model, source=source,
                  recipe=recipe.revision, features={}, gates=[], suspected_part=None)

    def finish(verdict, reason):
        result.update(verdict=verdict, reason=reason)
        return result

    if complete is not True or status_flags != 0 or source not in (1, 2):
        return finish("INVALID", "Acquisition integrity")
    pcm = np.asarray(samples)
    if pcm.ndim != 1 or pcm.dtype.kind != "i" or pcm.dtype.itemsize != 2:
        return finish("INVALID", "Expected signed 16-bit mono PCM")
    if rate_hz <= 0 or not pcm.size or np.any((pcm == -32768) | (pcm == 32767)):
        return finish("INVALID", "Empty, clipped or invalid-rate recording")
    x = pcm.astype(np.float64) / 32768.0
    x -= np.mean(x)
    rms = float(np.sqrt(np.mean(x * x)))
    if rms == 0:
        return finish("INVALID", "Constant signal")
    result["features"]["rms_fs"] = rms
    if model != recipe.model or rate_hz != recipe.rate_hz:
        return finish("HOLD", "No matching model and rate recipe")
    lo, hi = recipe.band_hz
    if (not recipe.revision or recipe.minimum_samples < 4
            or not 0 <= lo < hi <= rate_hz / 2
            or not all(math.isfinite(v) and v > 0 for v in
                       (recipe.max_rms_fs, recipe.max_band_rms_fs))):
        return finish("HOLD", "Invalid recipe")
    if x.size < recipe.minimum_samples:
        return finish("INVALID", "Short recording")
    window = np.hanning(x.size)
    power = np.abs(np.fft.rfft(x * window)) ** 2 / (x.size * np.dot(window, window))
    power[1:(-1 if x.size % 2 == 0 else None)] *= 2
    freq = np.fft.rfftfreq(x.size, 1.0 / rate_hz)
    band = (freq >= lo) & (freq < hi)
    if not np.any(band):
        return finish("HOLD", "No FFT bins in recipe band")
    band_rms = float(np.sqrt(np.sum(power[band])))
    result["features"].update(band_rms_fs=band_rms, peak_hz=float(freq[np.argmax(power)]))
    for name, measured, limit in (("rms_fs", rms, recipe.max_rms_fs),
                                  ("band_rms_fs", band_rms, recipe.max_band_rms_fs)):
        result["gates"].append(dict(metric=name, measured=measured, limit=limit,
                                    exceeded=measured > limit))
    if source == 2:
        return finish("HOLD", "Live-unit limits await reference-unit validation")
    if any(g["exceeded"] for g in result["gates"]):
        return finish("FAIL", "Synthetic limit exceeded")
    return finish("PASS", "Synthetic limits satisfied")
