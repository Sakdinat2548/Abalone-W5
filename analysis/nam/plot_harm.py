"""Throwaway plot: THD vs freq + THD vs input at Boost 10 (real ColorStage)."""
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

freq_rows = []
level_rows = []
mode = "freq"
with open(r"C:\Users\<you>\AppData\Local\Temp\opencode\harm.csv") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            if "level sweep" in line:
                mode = "level"
            continue
        if line.startswith("freqHz") or line.startswith("inDbFS"):
            continue
        parts = [float(x) for x in line.split(",")]
        (freq_rows if mode == "freq" else level_rows).append(parts)

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))

labels = ["in -30 dBFS", "in -20 dBFS", "in -10 dBFS", "in 0 dBFS"]
for c in range(4):
    ax1.semilogx([r[0] for r in freq_rows], [r[c + 1] for r in freq_rows],
                 marker="o", ms=3, label=labels[c] + " (color %+.0f dB)" % (-30 + 10 * c + 30))
ax1.set_xlim(10, 20000)
ax1.set_xlabel("Hz")
ax1.set_ylabel("THD %")
ax1.set_title("Boost 10: THD vs frequency (flat — memoryless stage)")
ax1.grid(True, which="both", alpha=0.4)
ax1.legend(fontsize=8)

ax2.semilogx([10.0 ** (r[0] / 20.0) * 100.0 for r in level_rows],
             [r[1] for r in level_rows], color="red", lw=1.6)
ax2.set_xlabel("input level (% of full scale, 1 kHz)")
ax2.set_ylabel("THD %")
ax2.set_title("Boost 10: THD vs input level @1 kHz")
ax2.grid(True, which="both", alpha=0.4)
for db, want in [(-20.0, 0.121), (-10.0, 0.795), (0.0, 6.26)]:
    ax2.plot([10.0 ** (db / 20.0) * 100.0], [want], "ko", ms=4)

fig.suptitle("Abalone W5 color harmonics @ Boost 10 (+30 dB into Color)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\harm.png", dpi=110)
print("wrote harm.png")
