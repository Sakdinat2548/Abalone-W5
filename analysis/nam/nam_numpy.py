"""Throwaway: exact numpy inference for TONE3000-packed WaveNet .nam files.

Architecture (verified against nam source + Core walkthrough + exact weight
count 12146): single LayerArray, rechannel 1->C k1 no-bias, 23 ungated
LeakyReLU(0.01) layers [dilated conv C->C k,d bias + 1x1 mixer on input +
1x1 C->C bias + residual add, heads accumulate post-activation], head
rechannel C->1 k16 bias, x head_scale. No FiLM, no head1x1, no condition DSP.
"""
import json

import numpy as np


def leaky_relu(x, slope=0.01):
    return np.where(x >= 0, x, slope * x)


class NAME:
    def __init__(self, path, submodel=1):
        d = json.load(open(path))
        m = d["config"]["submodels"][submodel]["model"]
        L = m["config"]["layers"][0]
        self.C = L["channels"]
        self.dil = L["dilations"]
        self.kerns = L["kernel_sizes"]
        self.head_scale = m["config"].get("head_scale", 1.0)
        w = np.asarray(m["weights"], dtype=np.float64)
        p = 0

        def take(n):
            nonlocal p
            v = w[p:p + n]
            p += n
            return v

        # rechannel 1->C k1, no bias
        self.re_w = take(1 * self.C * 1).reshape(self.C, 1)
        self.layers = []
        for k in self.kerns:
            d_idx = len(self.layers)
            dd = self.dil[d_idx]
            conv_w = take(self.C * self.C * k).reshape(self.C, self.C, k)
            conv_b = take(self.C)
            mix_w = take(1 * self.C * 1).reshape(self.C, 1)
            l11_w = take(self.C * self.C * 1).reshape(self.C, self.C)
            l11_b = take(self.C)
            self.layers.append((dd, k, conv_w, conv_b, mix_w, l11_w, l11_b))
        # head rechannel C->1 k16, bias
        self.h_w = take(self.C * 1 * 16).reshape(1, self.C, 16)
        self.h_b = take(1)
        # trailing head_scale (must match config within export tolerance)
        hs = take(1)
        assert abs(float(hs[0]) - self.head_scale) < 1e-5, (hs[0], self.head_scale)
        assert p == len(w), (p, len(w))
        # receptive field: layers + head k16
        self.rf = 1 + sum((kk - 1) * dd for dd, kk in zip(self.dil, self.kerns)) + 15

    def _dilated_conv(self, x, W, b, d):
        # x: (Cin, N) -> (Cout, N-(K-1)*d); W: (Cout, Cin, K)
        Cout, Cin, K = W.shape
        N = x.shape[1]
        out_len = N - (K - 1) * d
        idx = np.arange(out_len)[None, :] + np.arange(K)[:, None] * d  # (K, out)
        gathered = x[:, idx]  # (Cin, K, out)
        y = np.tensordot(W, gathered, axes=([1, 2], [0, 1]))  # (Cout, out)
        return y + b[:, None]

    def forward(self, x):
        x = np.asarray(x, dtype=np.float64).reshape(1, -1)
        # causal pre-roll so output aligns with input length
        xp = np.concatenate([np.zeros((1, self.rf - 1)), x], axis=1)
        c = xp
        y = self.re_w.reshape(self.C, 1) @ xp  # (C, N+rf-1)
        heads = None
        for (dd, k, cw, cb, mw, lw, lb) in self.layers:
            z = self._dilated_conv(y, cw, cb, dd)
            mix = (mw.reshape(self.C, 1) @ c)[:, -z.shape[1]:]
            a = leaky_relu(z + mix)
            r = (lw @ a.reshape(self.C, -1)).reshape(a.shape) + lb[:, None]
            y = y[:, -r.shape[1]:] + r
            heads = a if heads is None else heads[:, -a.shape[1]:] + a
        out = self._dilated_conv(heads, self.h_w, self.h_b, 1)
        return (out[0, -(x.shape[1]):] * self.head_scale)


if __name__ == "__main__":
    import sys

    m = NAME(sys.argv[1])
    print("channels:", m.C, "rf:", m.rf, "head_scale:", m.head_scale)
    # smoke: 1 kHz -20 dBFS sine, report output RMS + peak
    n = 48000
    t = np.arange(n) / 48000.0
    x = 10 ** (-20.0 / 20.0) * np.sin(2 * np.pi * 1000.0 * t)
    y = m.forward(x)
    print("in rms %.5f -> out rms %.5f (gain %+.2f dB) peak %.4f" % (
        float(np.sqrt(np.mean(x ** 2))), float(np.sqrt(np.mean(y ** 2))),
        20 * np.log10(float(np.sqrt(np.mean(y ** 2)) / np.sqrt(np.mean(x ** 2)))),
        float(np.max(np.abs(y)))))
