"""Throwaway plot: compressor-style transfer curves B1/B6/B10 (real DSP)."""
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

xs, b1, b6, b10 = [], [], [], []
with open(r"C:\Users\<you>\AppData\Local\Temp\opencode\dyn.csv") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("inDb"):
            continue
        p = [float(x) for x in line.split(",")]
        xs.append(p[0])
        b1.append(p[1])
        b6.append(p[2])
        b10.append(p[3])

fig, ax = plt.subplots(figsize=(7, 7))
ax.plot(xs, xs, color="#8b949e", lw=1, ls="--", label="linear")
ax.plot(xs, b1, color="#2dd4bf", lw=1.8, label="Boost 1 (+3 dB)")
ax.plot(xs, b6, color="#39d353", lw=1.8, label="Boost 6 (+18 dB)")
ax.plot(xs, b10, color="#ff7b00", lw=1.8, label="Boost 10 (+30 dB)")
ax.set_xlim(-60, 0)
ax.set_ylim(-60, 0)
ax.set_xticks(range(-60, 1, 10))
ax.set_yticks(range(-60, 1, 10))
ax.set_aspect("equal")
ax.set_xlabel("input peak (dBFS)")
ax.set_ylabel("output peak (dB, unity trim per curve)")
ax.set_title("Abalone W5 dynamics — transfer curves")
details = []
for name, yy, color in [("B1", b1, "#2dd4bf"), ("B6", b6, "#39d353"), ("B10", b10, "#ff7b00")]:
    gr0 = xs[-1] - yy[-1]  # gain reduction with 0 dBFS in
    knee = next((x for x, y in zip(xs, yy) if y - x <= -1.0), None)
    details.append("%-3s %+6.2f%s" % (
        name, yy[-1],
        (", knee ~%d" % knee) if knee is not None else ""))
    ax.plot([xs[-1]], [yy[-1]], "o", color=color, ms=5)
ax.text(0.03, 0.8333,
        "0 dB in:\n" + "\n".join(details),
        transform=ax.transAxes, fontsize=9, va="top", ha="left", family="monospace",
        bbox=dict(facecolor="white", alpha=0.85, edgecolor="#cccccc"))
ax.grid(True, alpha=0.4)
ax.legend(fontsize=9)
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\dyn.png", dpi=110)
print("wrote dyn.png")
