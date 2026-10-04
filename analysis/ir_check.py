# Abalone W5 - U5-inspired clean bass DI.
# Copyright (C) 2026 Sakdinat2548.
# SPDX-License-Identifier: AGPL-3.0-or-later

"""IR validation harness for Abalone W5 v1 (Task 8).

Compares real Avalon U5 impulse responses (Tone3000, gitignored local dir)
against our DSP chain (ToneBank tone + HighCut on/off) and against the manual
chart/CSV oracle. Shape agreement only: every curve is normalized at 1 kHz to
remove capture-gain unknowns.

Dependencies: Python 3 stdlib + numpy ONLY (no scipy; FFT via numpy.fft).
WAV read via a manual RIFF parser (stdlib `wave` rejects float32 files on
read, so `wave` is write-only here for the self-test round-trip, with the
format tag patched to 3 afterwards); the reader supports PCM 8/16/24/32-bit
int and float32.

Python-port verification: the RBJ / HighCut math below is transcribed verbatim
from src/dsp/ToneBank.h and src/dsp/HighCut.h. Equivalence is EXECUTED, not
asserted: --verify-port (run on every invocation as part of the self-test
section) recomputes the six per-tone either-oracle worst deltas
(min(|port-header|, |port-CSV|) over the 60 eye-read header points, same
algorithm as tests/ToneBankTest.cpp checkHeaderOracle) and requires each
to match the recorded C++ reference within 0.05 dB (Task-24 numbers):
  T1 0.16 / T2 0.12 / T3 0.07 / T4 0.10 / T5 0.05 / T6 0.21 dB.
(The references are small because the fit tracks the digitized
chart — CSV binding, header/IR advisory; see IR_VALIDATION.md. The check
verifies port fidelity to C++, not fit quality.)

Usage:
  python analysis/ir_check.py                  # self-test + real IRs if present
  python analysis/ir_check.py --self-test-only  # harness sanity only, no files
  python analysis/ir_check.py --verify-port     # port-vs-C++ check only
  python analysis/ir_check.py --ir-dir <dir> --zoom-dir <dir>

Exit code: 0 = self-test green (and every real IR within gate, if any);
1 = self-test failure (includes port-verification failure); 2 = a real IR
failed the +/-1 dB gate (measure, don't tune here -- tuning is a follow-up
decision) or files were present but NONE could be compared (all skipped).
Absent local files are NOT a failure: they print BLOCKED-ON-FILES / pending
instructions and exit 0.
"""

import argparse
import csv
import math
import os
import re
import struct
import sys
import tempfile
import wave

try:
    import numpy as np
except ImportError:
    sys.exit("ir_check.py requires numpy (python -c \"import numpy\")")

# ----------------------------------------------------------------------------
# Oracle gates (from spec / AGENTS.md).
# ----------------------------------------------------------------------------
GATE_DB = 1.0          # shape gate, 40 Hz - 15 kHz
GATE_LO_HZ = 40.0
GATE_HI_HZ = 15000.0
NORM_HZ = 1000.0       # capture-gain normalization point
SELFTEST_TOL_DB = 0.05  # end-to-end (time-domain synth IR -> FFT -> analytic)

# ----------------------------------------------------------------------------
# Section 1: Python port of src/dsp/ToneBank.h + src/dsp/HighCut.h.
# (tone, stage) -> (type, f0 Hz, Q, gain dB); HP stages have no gain.
# Verbatim from the header comment table (Task 24: T1x6/T2x6/T3x5/T4x6/T5x5/
# T6x6 — tight fit to the digitized gray, see ToneBank.h).
# ----------------------------------------------------------------------------
TONE_PARAMS = {
    10: ("hp", 5.0, 0.40395, 0.0),
    11: ("pk", 49.0866, 0.35951, 1.8346),
    12: ("pk", 1048.8736, 0.16364, -13.7137),
    13: ("pk", 1734.5392, 0.21392, 7.1515),
    14: ("hs", 11981.7927, 1.06858, 1.311),
    15: ("pk", 284.8489, 0.89252, 1.335),
    20: ("pk", 644.5752, 0.31635, -9.5942),
    21: ("pk", 715.0513, 1.6408, -12.8167),
    22: ("ls", 12.4907, 0.91201, -1.5117),
    23: ("pk", 34.9406, 0.50216, 1.5057),
    24: ("pk", 3159.9499, 0.61821, -0.7183),
    25: ("hs", 610.9243, 1.6477, 1.3745),
    30: ("pk", 1139.8615, 0.15462, -3.7423),
    31: ("pk", 18852.0009, 0.17095, 0.4067),
    32: ("ls", 12.9996, 0.78994, -3.6748),
    33: ("pk", 40.4319, 0.64122, 0.8181),
    34: ("pk", 677.6882, 1.72978, -0.2364),
    40: ("ls", 23.3315, 0.66438, -2.6291),
    41: ("pk", 8815.7501, 1.19986, -2.0138),
    42: ("hs", 12.6487, 1.02216, 1.5772),
    43: ("pk", 5139.997, 0.66627, -4.9216),
    44: ("hs", 14083.1316, 0.57743, 1.3207),
    45: ("pk", 9.9251, 3.37706, -0.4468),
    50: ("ls", 16.5931, 0.66482, -4.0),
    51: ("ls", 61.4232, 0.42485, -19.2452),
    52: ("hs", 166.6066, 0.51491, 2.5516),
    53: ("pk", 732.2752, 1.09303, 0.3299),
    54: ("pk", 11.0426, 3.60242, -0.6),
    60: ("ls", 17.7493, 0.65975, -3.972),
    61: ("ls", 63.4268, 0.43413, -18.9831),
    62: ("hs", 198.2506, 0.53229, 2.7376),
    63: ("hs", 14729.0504, 0.51821, -5.6445),
    64: ("pk", 20141.4081, 1.42257, -0.5909),
    65: ("pk", 11.1211, 2.63754, -0.7999),
}


def num_sections(tone):
    """Section budget T1x6/T2x6/T3x5/T4x6/T5x5/T6x6 — mirrors ToneBank::numSections."""
    return {1: 6, 2: 6, 3: 5, 4: 6, 5: 5, 6: 6}[tone]


def rbj_cook(typ, f0, q, gain_db, fs):
    """RBJ cookbook coefficients (double), shelf alpha = sin(w0)/(2Q)."""
    w0 = 2.0 * math.pi * f0 / fs
    cw, sw = math.cos(w0), math.sin(w0)
    alpha = sw / (2.0 * q)
    A = 10.0 ** (gain_db / 40.0)
    if typ == "pk":
        b0, b1, b2 = 1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A
        a0, a1, a2 = 1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A
    elif typ == "ls":
        sq = 2.0 * math.sqrt(A) * alpha
        b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq)
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw)
        b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq)
        a0 = (A + 1.0) + (A - 1.0) * cw + sq
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw)
        a2 = (A + 1.0) + (A - 1.0) * cw - sq
    elif typ == "hs":
        sq = 2.0 * math.sqrt(A) * alpha
        b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq)
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw)
        b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq)
        a0 = (A + 1.0) - (A - 1.0) * cw + sq
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw)
        a2 = (A + 1.0) - (A - 1.0) * cw - sq
    elif typ == "hp":
        b0, b1, b2 = (1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5
        a0, a1, a2 = 1.0 + alpha, -2.0 * cw, 1.0 - alpha
    else:
        raise ValueError("unknown section type %r" % typ)
    return b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0


def tone_magnitude_at(tone, freq_hz, fs=48000.0):
    """Exact theoretical cascade response in dB; mirrors ToneBank::magnitudeAt."""
    if tone == 0 or not freq_hz > 0.0:
        return 0.0
    w = 2.0 * math.pi * freq_hz / fs
    cos_w, sin_w = math.cos(w), math.sin(w)
    real, imag = 1.0, 0.0
    for s in range(num_sections (tone)):
        b0, b1, b2, a1, a2 = rbj_cook(*TONE_PARAMS[tone * 10 + s], fs)
        cos2 = 2.0 * cos_w * cos_w - 1.0
        bz_r = b0 + b1 * cos_w + b2 * cos2
        bz_i = -(b1 * sin_w + b2 * 2.0 * cos_w * sin_w)
        az_r = 1.0 + a1 * cos_w + a2 * cos2
        az_i = -(a1 * sin_w + a2 * 2.0 * cos_w * sin_w)
        denom = az_r * az_r + az_i * az_i
        h_r = (bz_r * az_r + bz_i * az_i) / denom
        h_i = (bz_i * az_r - bz_r * az_i) / denom
        real, imag = real * h_r - imag * h_i, real * h_i + imag * h_r
    return 20.0 * math.log10(math.hypot(real, imag))


def highcut_coeff(fs):
    """Exact -3 dB @ 8 kHz one-pole coefficient; mirrors HighCut::setSampleRate."""
    w = 2.0 * math.pi * 8000.0 / fs
    t = 2.0 - math.cos(w)
    return 1.0 - (t - math.sqrt(t * t - 1.0))


def highcut_magnitude_at(freq_hz, fs=48000.0):
    """|H(e^jw)| of y += a*(x-y) in dB."""
    a = highcut_coeff(fs)
    b = 1.0 - a
    w = 2.0 * math.pi * freq_hz / fs
    mag = a / math.sqrt(1.0 + b * b - 2.0 * b * math.cos(w))
    return 20.0 * math.log10(mag)


def chain_db(tone, freq_hz, fs=48000.0, highcut=False):
    """Full ToneBank + HighCut chain response in dB (other stages are flat)."""
    out = tone_magnitude_at(tone, freq_hz, fs)
    if highcut:
        out += highcut_magnitude_at(freq_hz, fs)
    return out


# ----------------------------------------------------------------------------
# Section 1b: self-proving Python-port verification (no build coupling).
# Recomputes the six per-tone either-oracle worst deltas
# (min(|port-header|, |port-CSV|) over the 60 eye-read header points -- the
# same algorithm as tests/ToneBankTest.cpp checkHeaderOracle) and requires
# each to match the recorded C++ reference within PORT_VERIFY_TOL_DB.
# Reference values: C++ checkHeaderOracle output at 48 kHz, 2-decimal
# (Task-24 numbers).
# ----------------------------------------------------------------------------
CXX_EITHER_REF_DB = {1: 0.16, 2: 0.12, 3: 0.07, 4: 0.10, 5: 0.05, 6: 0.21}
PORT_VERIFY_TOL_DB = 0.05


def load_header_targets(path):
    """Parse the eye-read header oracle (analysis/tone_targets.h).

    Returns [(tone, freq_hz, db)]; single source of truth, no transcription.
    """
    with open(path) as fh:
        txt = fh.read()
    pts = re.findall(r"\{(\d),\s*([\d.]+)f,\s*([+-]?[\d.]+)f\}", txt)
    out = [(int(t), float(f), float(db)) for t, f, db in pts]
    if len(out) < 60:
        raise ValueError("expected >=60 header targets in %s, got %d"
                         % (path, len(out)))
    return out


def verify_port(csv_path, header_path, fs=48000.0):
    """Check the Python port against the C++ reference. Returns (ok, lines)."""
    chart_freqs, chart = load_chart_csv(csv_path)
    targets = load_header_targets(header_path)
    ok = True
    lines = []
    for tone in range(1, 7):
        worst = 0.0
        for t, f, db in targets:
            if t != tone:
                continue
            m = tone_magnitude_at(tone, f, fs)
            csvv = float(log_interp(f, chart_freqs, chart[tone]))
            d = dHeader = abs(m - db)
            dCsv = abs(m - csvv)
            dEither = dHeader if dHeader < dCsv else dCsv
            if dEither > worst:
                worst = dEither
        diff = abs(worst - CXX_EITHER_REF_DB[tone])
        passed = diff <= PORT_VERIFY_TOL_DB
        ok = ok and passed
        lines.append("  [%-4s] tone %d port either-oracle worst=%.4f dB "
                     "(C++ ref %.2f, diff %.4f, tol %.2f)"
                     % ("PASS" if passed else "FAIL", tone, worst,
                        CXX_EITHER_REF_DB[tone], diff, PORT_VERIFY_TOL_DB))
    return ok, lines


# ----------------------------------------------------------------------------
# Section 2: WAV I/O (stdlib only) + IR -> magnitude response (numpy FFT).
# ----------------------------------------------------------------------------
def read_wav_mono(path):
    """Read a WAV file, return (float64 mono samples, sample rate).

    Manual RIFF parser (stdlib `wave` rejects float32 files on read): supports
    format tags 1 (PCM 8/16/24/32-bit int) and 3 (float32), any channel count
    (mixed to mono). Unknown chunks are skipped.
    """
    with open(path, "rb") as fh:
        blob = fh.read()
    if blob[0:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file: %s" % path)
    tag, nch, fs, width = None, None, None, None
    raw = None
    pos = 12
    while pos + 8 <= len(blob):
        cid, size = blob[pos:pos + 4], struct.unpack("<I", blob[pos + 4:pos + 8])[0]
        body = blob[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, nch = struct.unpack("<HH", body[0:4])
            fs = struct.unpack("<I", body[4:8])[0]
            width = struct.unpack("<H", body[14:16])[0] // 8
        elif cid == b"data":
            raw = body
        pos += 8 + size + (size & 1)  # chunks are word-aligned
    if tag is None or raw is None:
        raise ValueError("WAV missing fmt/data chunk: %s" % path)
    assert nch is not None and fs is not None and width is not None
    if tag == 3 and width == 4:
        data = np.frombuffer(raw, dtype=np.float32).astype(np.float64)
    elif tag == 1 and width == 1:  # 8-bit unsigned
        data = np.frombuffer(raw, dtype=np.uint8).astype(np.float64)
        data = (data - 128.0) / 128.0
    elif tag == 1 and width == 2:
        data = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    elif tag == 1 and width == 3:  # 24-bit little-endian
        arr = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        ints = arr[:, 0] | (arr[:, 1] << 8) | (arr[:, 2] << 16)
        ints = np.where(ints >= 1 << 23, ints - (1 << 24), ints)
        data = ints.astype(np.float64) / float(1 << 23)
    elif tag == 1 and width == 4:
        data = (np.frombuffer(raw, dtype=np.int32).astype(np.float64)
                / float(1 << 31))
    else:
        raise ValueError("unsupported WAV format tag=%s width=%s in %s"
                         % (tag, width, path))
    if nch > 1:
        data = data.reshape(-1, nch).mean(axis=1)
    return data, float(fs)


def write_wav_mono(path, data, fs):
    """Write float64 mono data as 32-bit float WAV (self-test round-trip).

    stdlib `wave` always stamps format tag 1 (PCM), so patch bytes 20-21 to
    tag 3 (IEEE float) afterwards -- otherwise the reader (correctly) treats
    the samples as int32 PCM.
    """
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(4)
        wf.setframerate(int(fs))
        wf.writeframes(np.asarray(data, dtype=np.float32).tobytes())
    with open(path, "r+b") as fh:
        fh.seek(20)
        fh.write(struct.pack("<H", 3))


def log_interp(x, xp, fp):
    """Log-frequency linear interpolation (x scalar or array)."""
    return np.interp(np.log(x), np.log(xp), fp)


def ir_response(ir, fs, target_freqs):
    """FFT magnitude of an IR in dB, normalized to 0 dB at 1 kHz.

    No window: a decayed IR needs none, and any window would bias the deep
    T2 notch. Returns response sampled (log-interp) at target_freqs.
    """
    ir = np.asarray(ir, dtype=np.float64)
    n = 1
    while n < len(ir):
        n *= 2
    spec = np.fft.rfft(ir, n=n)
    freqs = np.fft.rfftfreq(n, d=1.0 / fs)
    mag = np.abs(spec)
    mag = np.maximum(mag, 1e-30)  # floor before log (kills log(0) at DC nulls)
    db = 20.0 * np.log10(mag)
    db = db - float(log_interp(NORM_HZ, freqs[1:], db[1:]))  # skip DC bin
    return log_interp(np.asarray(target_freqs), freqs[1:], db[1:])


def load_chart_csv(path):
    """Load the digitized manual-chart oracle; returns (freqs, {tone: dbs})."""
    freqs, curves = [], {t: [] for t in range(1, 7)}
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            freqs.append(float(row["freq_hz"]))
            for t in range(1, 7):
                curves[t].append(float(row["curve%d_db" % t]))
    return np.array(freqs), {t: np.array(v) for t, v in curves.items()}


def norm_at_1k(freqs, db):
    """Normalize a curve to 0 dB at 1 kHz (log-interp)."""
    return np.asarray(db) - float(log_interp(NORM_HZ, freqs, db))


def summarize(freqs, delta):
    """(max, mean) of |delta| over the gated band + full-band max for the log."""
    in_gate = (freqs >= GATE_LO_HZ) & (freqs <= GATE_HI_HZ)
    gated = np.abs(delta[in_gate])
    return float(gated.max()), float(gated.mean()), float(np.abs(delta).max())


# ----------------------------------------------------------------------------
# Section 3: synthetic self-test (end-to-end harness sanity, no files needed).
# A unit impulse is filtered through a float32 time-domain copy of the chain
# (Transposed DFII biquads + one-pole highcut, as in the C++), round-tripped
# through a real WAV file, FFT'd, and compared to the analytic chain_db.
# ----------------------------------------------------------------------------
def dfii_filter(x, b0, b1, b2, a1, a2):
    """Transposed Direct Form II, float32 state (mirrors ToneBank::processSample)."""
    y = np.empty_like(x, dtype=np.float32)
    z1 = np.float32(0.0)
    z2 = np.float32(0.0)
    for i, xi in enumerate(x):
        out = np.float32(b0) * xi + z1
        z1 = np.float32(b1) * xi - np.float32(a1) * out + z2
        z2 = np.float32(b2) * xi - np.float32(a2) * out
        if abs(float(z1)) < 1.0e-15:
            z1 = np.float32(0.0)
        if abs(float(z2)) < 1.0e-15:
            z2 = np.float32(0.0)
        y[i] = out
    return y.astype(np.float64)


def synth_ir(tone, fs, highcut, seconds=1.5):
    """Synthetic IR: impulse through the time-domain chain (float32 states)."""
    n = int(fs * seconds)
    x = np.zeros(n, dtype=np.float32)
    x[0] = np.float32(1.0)
    if tone != 0:
        for s in range(num_sections (tone)):
            b0, b1, b2, a1, a2 = rbj_cook(*TONE_PARAMS[tone * 10 + s], fs)
            x = dfii_filter(x, b0, b1, b2, a1, a2).astype(np.float32)
    if highcut:
        a = highcut_coeff(fs)
        y = np.empty_like(x)
        state = np.float32(0.0)
        for i, xi in enumerate(x):
            state = state + np.float32(a) * (xi - state)
            y[i] = state
        x = y
    return x.astype(np.float64)


def run_self_test(grid_freqs):
    """End-to-end harness check. Returns (ok, lines)."""
    lines = []
    ok = True
    cases = [(0, False, "flat bypass (tone 0, highcut off)"),
             (3, False, "tone 3, highcut off"),
             (3, True, "tone 3, highcut on"),
             (2, False, "tone 2 notch, highcut off")]
    for tone, hc, label in cases:
        fs = 48000.0
        ir = synth_ir(tone, fs, hc)
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "synth.wav")
            write_wav_mono(p, ir, fs)
            back, fs_back = read_wav_mono(p)
        assert fs_back == fs, "WAV round-trip changed sample rate"
        meas = ir_response(back, fs, grid_freqs)
        ref = np.array([chain_db(tone, f, fs, hc) for f in grid_freqs])
        ref = ref - float(log_interp(NORM_HZ, grid_freqs, ref))
        worst, mean, _ = summarize(grid_freqs, meas - ref)
        passed = worst <= SELFTEST_TOL_DB
        ok = ok and passed
        lines.append("  [%-4s] %-32s max=%.4f dB mean=%.4f dB (tol %.2f)"
                     % ("PASS" if passed else "FAIL", label, worst, mean,
                        SELFTEST_TOL_DB))
    # Gain-invariance: scaling the IR must not move the normalized response.
    ir = synth_ir(3, 48000.0, False)
    r1 = ir_response(ir, 48000.0, grid_freqs)
    r2 = ir_response(ir * 3.7, 48000.0, grid_freqs)
    ginv = float(np.abs(r1 - r2).max())
    passed = ginv < 1e-9
    ok = ok and passed
    lines.append("  [%-4s] gain-invariance (x3.7)          max=%.2e dB"
                 % ("PASS" if passed else "FAIL", ginv))
    return ok, lines


# ----------------------------------------------------------------------------
# Section 4: filename parsing + per-IR comparison.
# Accepted names (case-insensitive), e.g.:
#   tone3_highcut_off_48k.wav / TONE2_HIGHCUT_ON_44k1.wav / tone0.wav
#   AVALON_TONE1.wav / AVALON_TONE1_HIGHCUT.wav (real Tone3000 names:
#   highcut iff _HIGHCUT appears, else off)
# Anything unparseable is reported and skipped (never guessed silently).
# ----------------------------------------------------------------------------
IR_NAME_RE = re.compile(r"tone\s*([0-6]).*?(off|on)", re.IGNORECASE)
TONE_FALLBACK_RE = re.compile(r"tone\s*([0-6])", re.IGNORECASE)
HIGHCUT_RE = re.compile(r"highcut", re.IGNORECASE)


def parse_ir_name(name):
    m = IR_NAME_RE.search(name)
    if m:
        return int(m.group(1)), m.group(2).lower() == "on"
    # Real Tone3000 names carry no on/off token
    # (AVALON_TONE1.wav / AVALON_TONE1_HIGHCUT.wav): highcut iff
    # _HIGHCUT appears, else off.
    m2 = TONE_FALLBACK_RE.search(name)
    if not m2:
        return None
    return int(m2.group(1)), bool(HIGHCUT_RE.search(name))


def check_ir_dir(ir_dir, grid_freqs, chart_freqs, chart):
    """Compare each IR WAV against chain + chart.

    Returns (n_total, n_compared, n_fail, lines). Unparseable or unreadable
    files are SKIP lines -- never guessed, never fatal here; the caller
    treats "files present but none compared" as a loud nonzero exit so an
    all-SKIP run can never look green.
    """
    wavs = sorted(f for f in os.listdir(ir_dir)
                  if f.lower().endswith(".wav"))
    if not wavs:
        return 0, 0, 0, []
    lines = []
    n_compared = 0
    n_fail = 0
    for f in wavs:
        parsed = parse_ir_name(f)
        if parsed is None:
            lines.append("  SKIP %s: cannot parse tone/highcut from name" % f)
            continue
        tone, hc = parsed
        try:
            ir, fs = read_wav_mono(os.path.join(ir_dir, f))
        except (ValueError, struct.error, OSError) as e:
            lines.append("  SKIP %s: unreadable (%s)" % (f, e))
            continue
        n_compared += 1
        meas = ir_response(ir, fs, grid_freqs)
        ref = np.array([chain_db(tone, fr, fs, hc) for fr in grid_freqs])
        ref = ref - float(log_interp(NORM_HZ, grid_freqs, ref))
        # Chart oracle: flat zeros for bypass (the manual chart has no highcut
        # dimension, so add our highcut curve for highcut-on files to keep the
        # vs-chart column meaningful instead of measuring the filter itself).
        cht = (np.zeros_like(grid_freqs) if tone == 0
               else log_interp(grid_freqs, chart_freqs,
                               norm_at_1k(chart_freqs, chart[tone])))
        if hc:
            cht = cht + np.array([highcut_magnitude_at(fr, fs)
                                  for fr in grid_freqs])
            cht = cht - float(log_interp(NORM_HZ, grid_freqs, cht))
        w_ours, m_ours, _ = summarize(grid_freqs, meas - ref)
        w_cht, m_cht, _ = summarize(grid_freqs, meas - cht)
        verdict = "PASS" if w_ours <= GATE_DB else "FAIL"
        if verdict == "FAIL":
            n_fail += 1
        lines.append("  [%s] %s (tone %d, highcut %s, %.0f Hz): "
                     "vs-ours max=%.2f mean=%.2f | vs-chart max=%.2f mean=%.2f" %
                     (verdict, f, tone, "on" if hc else "off", fs,
                      w_ours, m_ours, w_cht, m_cht))
        # 15-20 kHz axis-caveat region: reported separately, NEVER gated.
        hi = grid_freqs > GATE_HI_HZ
        lines.append("         15-20k (report only): vs-ours max=%.2f mean=%.2f"
                     % (float(np.abs(meas[hi] - ref[hi]).max()),
                        float(np.abs(meas[hi] - ref[hi]).mean())))
    return len(wavs), n_compared, n_fail, lines


# ----------------------------------------------------------------------------
# Section 5: driver.
# ----------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser(description="Abalone W5 IR validation harness")
    ap.add_argument("--self-test-only", action="store_true")
    ap.add_argument("--verify-port", action="store_true",
                    help="run only the Python-port-vs-C++ check, then exit")
    ap.add_argument("--ir-dir", default=os.path.join(HERE, "ir_local"))
    ap.add_argument("--zoom-dir", default=os.path.join(HERE, "zoom_ref"))
    ap.add_argument("--csv", default=os.path.join(HERE,
                                                  "u5_tone_curves_digitized.csv"))
    ap.add_argument("--header-targets", default=os.path.join(HERE,
                                                             "tone_targets.h"))
    args = ap.parse_args()

    chart_freqs, chart = load_chart_csv(args.csv)
    grid = chart_freqs[(chart_freqs >= GATE_LO_HZ)]  # 40 Hz..20 kHz grid

    if args.verify_port:
        print("== ir_check.py port verification (Python port vs C++ ref) ==")
        ok, lines = verify_port(args.csv, args.header_targets)
        print("\n".join(lines))
        print("port verification: %s" % ("GREEN" if ok else "FAILED"))
        return 0 if ok else 1

    print("== ir_check.py port verification (Python port vs C++ ref) ==")
    port_ok, port_lines = verify_port(args.csv, args.header_targets)
    print("\n".join(port_lines))
    if not port_ok:
        print("PORT VERIFICATION FAILED -- port disagrees with C++, "
              "fix before trusting any comparison.")
        return 1
    print("port verification: GREEN")

    print("== ir_check.py self-test (synthetic IRs, no files needed) ==")
    ok, lines = run_self_test(grid)
    print("\n".join(lines))
    if not ok:
        print("SELF-TEST FAILED -- harness is untrustworthy, fix before use.")
        return 1
    print("self-test: GREEN")

    if args.self_test_only:
        return 0

    print("\n== IR leg (Tone3000 U5 IRs) ==")
    if not os.path.isdir(args.ir_dir) or not any(
            f.lower().endswith(".wav") for f in os.listdir(args.ir_dir)):
        print("BLOCKED-ON-FILES: no IR WAVs at %s" % args.ir_dir)
        print("To complete: download the Avalon U5 set "
              "(14 files, TONE0-6 x HIGHCUT on/off) from "
              "https://www.tone3000.com/tones/avalon_u5-36172 with your "
              "Tone3000 account (T3K license -- NEVER commit the WAVs), "
              "drop them in that dir, and re-run this script.")
        ir_fail = 0
        ir_ran = False
    else:
        n, n_compared, ir_fail, lines = check_ir_dir(args.ir_dir, grid,
                                                     chart_freqs, chart)
        print("\n".join(lines))
        print("IR leg: %d file(s), %d compared, %d FAIL vs +/-%.0f dB gate "
              "(40 Hz-15 kHz)" % (n, n_compared, ir_fail, GATE_DB))
        if n_compared == 0:
            print("WARNING: %d WAV file(s) present but ALL SKIPPED -- "
                  "zero IRs actually compared. This is NOT green: fix "
                  "filenames (tone 0-6 + on/off, or AVALON_TONE<n>[_HIGHCUT]) "
                  "or repair the unreadable files and re-run." % n)
            return 2
        ir_ran = True

    print("\n== Zoom leg (ear A/B captures) ==")
    zoom_ok = (os.path.isdir(args.zoom_dir) and any(
        f.lower().endswith(".wav") for f in os.listdir(args.zoom_dir)))
    if not zoom_ok:
        print("PENDING (user-side): no captures at %s -- ear A/B cannot be "
              "automated anyway." % args.zoom_dir)
        print("To complete: record the same riff DI plus Tones 2/3/4 through "
              "the plugin at matched output level, 48 kHz WAV, names like "
              "riff_di.wav / riff_tone2.wav / riff_tone3.wav / riff_tone4.wav, "
              "then level-match (RMS over the riff body) and spectral-compare "
              "vs the chain render; see IR_VALIDATION.md for the method.")
    else:
        print("captures present -- level-match + spectral-compare per "
              "IR_VALIDATION.md (manual step, not automated by this script).")

    if ir_ran and ir_fail:
        print("\nRESULT: %d IR(s) outside the gate -- MEASURED, do not tune "
              "here (re-tune is a follow-up decision)." % ir_fail)
        return 2
    print("\nRESULT: harness green%s."
          % (", IR leg pending files" if not ir_ran else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
