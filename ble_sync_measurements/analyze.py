import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import argparse
import itertools
import conf
from scipy import stats

from pathlib import Path

# Parse the arguments: just the directory containing the data is required
parser = argparse.ArgumentParser(description="Analyze the data recorded using the logic analyzer")
parser.add_argument("dir", type=Path, help="Directory containing the data to analyze")
parser.add_argument("n", type=int, help="Number of unit of the connection interval (CI = n * 1.25ms)")
args = parser.parse_args()

df = pd.read_csv(args.dir / "digital.csv")

# Find all edges (rising and falling): the GPIO toggles every period instead
# of pulsing, so consecutive events land on opposite edges when a board's
# toggle phase is offset from the others, e.g. after an independent reset
channels = [c for c in df.columns if c.startswith("Channel")]
for ch in channels:
    df[f"{ch} edge"] = (df[ch].diff().abs() == 1).astype(int)

# Channel of each board (API index, see instructions.md). The central is the
# time reference; the peripherals are every other recorded channel listed in
# conf.py, so the script adapts to however many boards (2, 3, 4...) were
# captured instead of assuming a fixed count
CENTRAL = f"Channel {conf.CENTRAL}"
if CENTRAL not in channels:
    raise SystemExit(f"Central channel {CENTRAL!r} not recorded (found: {channels})")

configured_peripherals = [f"Channel {n}" for n in conf.digital_channels if n != conf.CENTRAL]

PERIPHERALS = {}  # name (P1, P2, ...) -> channel column, in conf.py order
for i, ch in enumerate(configured_peripherals, start=1):
    if ch not in channels:
        print(f"Warning: {ch} not recorded, skipping")
        continue
    if df[f"{ch} edge"].sum() == 0:
        print(f"Warning: {ch} has no edges, skipping")
        continue
    PERIPHERALS[f"P{i}"] = ch

if not PERIPHERALS:
    raise SystemExit("No peripheral channel with edges found")

# Max distance between edges of the same event: must be much smaller than
# half the period of the signal (edges now occur every half period, since
# rising and falling edges are both matched), otherwise edges of different
# events get matched
MATCH_TOLERANCE_S = 10e-3

def edge_times(ch):
    return df.loc[df[f"{ch} edge"] == 1, ["Time [s]"]].rename(columns={"Time [s]": ch})

# For each edge of the central (rising or falling), take the nearest edge of
# each peripheral, regardless of its direction; drop the events where a
# peripheral has no edge within tolerance
edges = edge_times(CENTRAL)
for ch in PERIPHERALS.values():
    edges = pd.merge_asof(edges, edge_times(ch), left_on=CENTRAL, right_on=ch,
                          direction="nearest", tolerance=MATCH_TOLERANCE_S)
total_events = len(edges)
edges = edges.dropna().reset_index(drop=True)

# Sync error of every peripheral against the central, plus every pair of
# peripherals against each other, in microseconds
dt_us = pd.DataFrame({
    **{f"{name}-C": (edges[ch] - edges[CENTRAL]) * 1e6 for name, ch in PERIPHERALS.items()},
    **{f"{n1}-{n2}": (edges[ch1] - edges[ch2]) * 1e6
       for (n1, ch1), (n2, ch2) in itertools.combinations(PERIPHERALS.items(), 2)},
})

statistics = pd.DataFrame({
    "mean": dt_us.mean(),
    "median": dt_us.median(),
    "std": dt_us.std(),
    "min": dt_us.min(),
    "max": dt_us.max(),
    "rms": np.sqrt((dt_us ** 2).mean()),
    "|dt| p95": dt_us.abs().quantile(0.95),
    "|dt| p99": dt_us.abs().quantile(0.99),
    "|dt| max": dt_us.abs().max(),
})

print(f"Total events:   {total_events} (edges of the central)")
print(f"Matched events: {len(edges)} ({len(edges) / total_events:.1%})")
print("Sync errors [us]:")
print(statistics.to_string(float_format=lambda x: f"{x:8.3f}"))

# Save the std of this run in the summary used to plot std vs connection
# interval, one row per run. std_pc is the mean of the stds of the
# peripheral-central pairs, std_pp the mean of the peripheral-peripheral ones
# (NaN if there are none). Re-analyzing a directory replaces its row
EXPERIMENTS_DIR = Path(__file__).parent / "experiments"
SUMMARY_PATH = EXPERIMENTS_DIR / "std_summary.csv"
stds = statistics["std"]
is_pc = stds.index.str.endswith("-C")
try:
    run_dir = str(args.dir.resolve().relative_to(EXPERIMENTS_DIR.resolve()))
except ValueError:
    run_dir = str(args.dir)
row = pd.DataFrame([{
    "dir": run_dir,
    "n": args.n,
    "ci_ms": args.n * 1.25,
    "std_pc": stds[is_pc].mean(),
    "std_pp": stds[~is_pc].mean(),
}])
if SUMMARY_PATH.exists():
    old = pd.read_csv(SUMMARY_PATH)
    row = pd.concat([old[old["dir"] != run_dir], row], ignore_index=True)
row.sort_values("n").to_csv(SUMMARY_PATH, index=False)
print(f"Std summary updated in {SUMMARY_PATH}")

# Histogram of the sync errors, one panel per pair with a shared x axis.
# The timestamps are quantized by the sampling period, so the bins are centered
# on multiples of it: every bar is exactly one possible measured value
#SAMPLE_PERIOD_US = 1 / (conf.sampling_rate) * 10 ** 6  # 2 MS/s, keep in sync with digital_sample_rate in capture.py
#lo = np.floor(dt_us.min().min() / SAMPLE_PERIOD_US) * SAMPLE_PERIOD_US
#hi = np.ceil(dt_us.max().max() / SAMPLE_PERIOD_US) * SAMPLE_PERIOD_US
#bins = np.arange(lo - SAMPLE_PERIOD_US / 2, hi + SAMPLE_PERIOD_US, SAMPLE_PERIOD_US)

fig, axes = plt.subplots(len(dt_us.columns), 1, sharex=True, squeeze=False, figsize=(7, 3 * len(dt_us.columns)))
axes = axes[:, 0]
for ax, pair in zip(axes, dt_us.columns):
    ax.hist(dt_us[pair], bins=16, color="#2a78d6", edgecolor="white", linewidth=0.1)
    ax.axvline(0, color="#52514e", linewidth=1, linestyle="--")
    ax.set_title(f"{pair}   mean {statistics.loc[pair, 'mean']:.2f} µs, std {statistics.loc[pair, 'std']:.2f} µs",
                 loc="left", fontsize=10)
    ax.set_ylabel("Events")
    ax.yaxis.get_major_locator().set_params(integer=True)
    ax.grid(axis="y", color="#e5e5e3", linewidth=0.8)
    ax.set_axisbelow(True)
    ax.spines[["top", "right"]].set_visible(False)
axes[-1].set_xlabel("Sync error: edge time difference [µs]")
fig.tight_layout()

hist_path = args.dir / "sync_error_histogram.png"
fig.savefig(hist_path, dpi=200)
print(f"Histogram saved to {hist_path}")

# Sync error over time, one panel per pair with shared axes.
# The x axis is the time of the central edge, i.e. when the event happened
t_event = edges[CENTRAL]

fig, axes = plt.subplots(len(dt_us.columns), 1, sharex=True, sharey=True, squeeze=False, figsize=(9, 3 * len(dt_us.columns)))
axes = axes[:, 0]
for ax, pair in zip(axes, dt_us.columns):
    ax.plot(t_event, dt_us[pair], color="#2a78d6", linewidth=0.8, marker="o", markersize=3)
    ax.axhline(0, color="#52514e", linewidth=1, linestyle="--")
    ax.axhline(statistics.loc[pair, "mean"], color="#2a78d6", linewidth=1, linestyle=":")
    ax.set_title(f"{pair}   mean {statistics.loc[pair, 'mean']:.2f} µs, std {statistics.loc[pair, 'std']:.2f} µs",
                 loc="left", fontsize=10)
    ax.set_ylabel("Error [µs]")
    ax.grid(axis="y", color="#e5e5e3", linewidth=0.8)
    ax.set_axisbelow(True)
    ax.spines[["top", "right"]].set_visible(False)
axes[-1].set_xlabel("Capture time [s]")
fig.tight_layout()

time_path = args.dir / "sync_error_over_time.png"
fig.savefig(time_path, dpi=200)
print(f"Time plot saved to {time_path}")


# QQ plot against the normal distribution, one panel per pair. Each pair is
# standardized on its own (the columns have different mean and std), and the
# theoretical quantiles use the plotting positions (i - 0.5) / n, so the
# sample size of the pair is the one that counts
fig, axes = plt.subplots(1, len(dt_us.columns), squeeze=False,
                         figsize=(5 * len(dt_us.columns), 5))
axes = axes[0]
for ax, pair in zip(axes, dt_us.columns):
    sample = dt_us[pair].to_numpy()
    n = len(sample)
    sample_quantiles = np.sort((sample - sample.mean()) / sample.std(ddof=1))
    theoretical_quantiles = stats.norm.ppf((np.arange(1, n + 1) - 0.5) / n)
    ax.scatter(theoretical_quantiles, sample_quantiles, color="#2a78d6", s=18, alpha=0.7,
               edgecolors="white", linewidth=0.3)
    lim = max(np.abs(theoretical_quantiles).max(), np.abs(sample_quantiles).max()) * 1.05
    ax.plot([-lim, lim], [-lim, lim], color="#d6452a", linestyle="--", linewidth=1.5, alpha=0.8,
            label="Normal")
    ax.set_xlim(-lim, lim)
    ax.set_ylim(-lim, lim)
    ax.set_aspect("equal")
    ax.set_title(f"{pair}   n = {n}", loc="left", fontsize=10)
    ax.set_xlabel("Theoretical quantiles")
    ax.set_ylabel("Sample quantiles (standardized)")
    ax.legend(frameon=False)
    ax.grid(color="#e5e5e3", linewidth=0.8)
    ax.set_axisbelow(True)
    ax.spines[["top", "right"]].set_visible(False)
fig.tight_layout()

qq_path = args.dir / "sync_error_qq_plot.png"
fig.savefig(qq_path, dpi=200)
print(f"QQ plot saved to {qq_path}")
