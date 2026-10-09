"""Throwaway: null-test user Reaper renders vs numpy NAM forwards."""
import struct
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
import numpy as np
from nam_numpy import NAME


def read_float_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[0:4] == b"RIFF" and data[8:12] == b"WAVE"
    pos = 12
    fmt = None
    audio = None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, sr, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            assert tag == 3 and bits == 32, (tag, bits)
            fmt = (ch, sr)
        elif cid == b"data":
            audio = np.frombuffer(body, dtype=np.float32)
        pos += 8 + size + (size & 1)
    ch, sr = fmt
    return audio.reshape(-1, ch)[:, 0].astype(np.float64), sr


N = 48000
x = 10 ** (-10.0 / 20.0) * np.sin(2 * np.pi * 440.0 * np.arange(N) / 48000.0)
files = {
    "B3": ("REAPER Media/nam_boost3_440m10.wav",
           r"C:\Users\<you>\Downloads\Avalon U5 Boost3 MicOut No EQ.nam"),
    "B7": ("REAPER Media/nam_boost7_440m10.wav",
           r"C:\Users\<you>\Downloads\Avalon U5 Boost7(Unity) MicOut No EQ.nam"),
    "DI": ("REAPER Media/nam_di_440m10.wav",
           r"C:\Users\<you>\Downloads\Avalon U5 DI Preamplifier.nam"),
}
for tag, (rel, path) in files.items():
    a, sr = read_float_wav("C:/Users/<you>/Documents/" + rel)
    print("%s: sr %d len %.2fs peak %+.1f dBFS" % (
        tag, sr, len(a) / sr, 20 * np.log10(np.max(np.abs(a)))))
    m = NAME(path)
    y = m.forward(x)
    n = min(len(a), len(y))
    a, y = a[:n], y[:n]
    seg = slice(0, 48000)
    za, zy = a[seg] - a[seg].mean(), y[seg] - y[seg].mean()
    lag = int(np.argmax(np.correlate(za, zy, mode="full"))) - (len(za) - 1)
    g = float(np.sqrt(np.mean(zy ** 2)) / np.sqrt(np.mean(za ** 2)))
    r = zy - np.roll(za, lag) * g
    t = slice(24000, 48000)
    print("  lag %+d gain %+.3f dB null %.1f dBFS (%.1f rel out)" % (
        lag, 20 * np.log10(g), 20 * np.log10(float(np.sqrt(np.mean(r[t] ** 2)))),
        20 * np.log10(float(np.sqrt(np.mean(r[t] ** 2)) / np.sqrt(np.mean(zy[t] ** 2))))))
