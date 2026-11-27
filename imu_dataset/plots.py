import argparse
import matplotlib.pyplot as plt
import numpy as np

from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent / "data"

COUNTER_BDR = 128                  # samples between two timestamps, must match COUNTER_BDR_REG2 in conn_time_sync/src/imu.c
SENSITIVITY_G_PER_LSB = 0.061e-3   # LSM6DSV16BX at +-2 g (datasheet: 0.061 mg/LSB)
AXES = ["X", "Y", "Z"]             # column order of acc in the .npz (rec_buf stores X, Y, Z)
SPEC_NPERSEG = 256                 # samples per spectrogram segment: ~34 ms long, ~29 Hz resolution
SPEC_STEP = 64                     # spectrogram hop: a new column every ~8.5 ms (75 % overlap)


def list_pairs() -> list:
    """
    Returns the .npz files in DATA_DIR sorted by name (= by download time), grouped in pairs:
    pair 0 = files 0 and 1, pair 1 = files 2 and 3, and so on.
    It assumes the two boards of one recording are downloaded one after the other.
    """
    files = sorted(DATA_DIR.glob("*.npz"))
    return [(files[i], files[i + 1]) for i in range(0, len(files) - 1, 2)]


def load(path: Path) -> tuple:
    """
    Loads one recording. Returns (t, acc_g):
    t is the time of every sample in s (central clock), from a linear fit of the timestamps vs sample index,
    so it uses the real ODR of that IMU. Timestamp k belongs to sample index (k + 1) * COUNTER_BDR - 1.
    acc_g is an (N, 3) array in g.
    """
    d = np.load(path)
    acc_g = d["acc"].astype(np.float64) * SENSITIVITY_G_PER_LSB
    ts = d["timestamp"].astype(np.int64)    # ns, already corrected for the +1 RTC tick error by download.py

    ts_idx = (np.arange(len(ts)) + 1) * COUNTER_BDR - 1
    ts_rel = (ts - ts[0]).astype(np.float64)               # relative to ts[0]: small values, no float64 precision loss
    slope, intercept = np.polyfit(ts_idx, ts_rel, 1)        # slope = ns per sample

    t = (np.arange(len(acc_g)) * slope + intercept + ts[0]) / 1e9
    return t, acc_g


def spectrogram(x: np.ndarray, fs: float, nperseg: int, step: int) -> tuple:
    """
    Cuts x (mean removed) into segments of nperseg samples, one every step samples, applies a Hann window
    to each one and computes its one-sided power spectral density.
    Returns (centers, f, psd): centers = index of the central sample of each segment,
    f = frequencies in Hz, psd = 2D array (segments x frequencies) in unit of x ^2 / Hz.
    """
    x = x - x.mean()
    w = np.hanning(nperseg)
    starts = np.arange(0, len(x) - nperseg + 1, step)
    psd = np.array([np.abs(np.fft.rfft(w * x[s:s + nperseg])) ** 2 for s in starts])
    psd /= fs * np.sum(w ** 2)      # normalization to unit^2 / Hz
    psd[:, 1:-1] *= 2               # one-sided spectrum: fold the negative frequencies (not DC and Nyquist)
    f = np.fft.rfftfreq(nperseg, 1 / fs)
    return starts + nperseg // 2, f, psd


def cross_corr(a: np.ndarray, b: np.ndarray) -> tuple:
    """
    Normalized cross-correlation of two signals with the same length and the same sample times.
    Returns (lags, r): r[k] = sum_i a[i + k] * b[i] / (N * std_a * std_b), lags from -(N-1) to N-1 samples.
    A peak at a positive lag means a is delayed with respect to b.
    Done with the FFT (zero-padded to avoid the circular wrap), because np.correlate is O(N^2).
    """
    a = a - a.mean()
    b = b - b.mean()
    n = len(a)
    nfft = 1 << (2 * n - 1).bit_length()
    r = np.fft.irfft(np.fft.rfft(a, nfft) * np.conj(np.fft.rfft(b, nfft)), nfft)
    r = np.concatenate((r[-(n - 1):], r[:n])) / (n * a.std() * b.std())
    lags = np.arange(-(n - 1), n)
    return lags, r


def main():
    pairs = list_pairs()

    parser = argparse.ArgumentParser(description="Plot the acceleration of a pair of recordings and their cross-correlation.")
    parser.add_argument("index", type=int, help=f"index of the pair of files to plot (0 .. {len(pairs) - 1})")
    args = parser.parse_args()

    if not (0 <= args.index < len(pairs)):
        raise SystemExit(f"Error: index {args.index} out of range, there are {len(pairs)} pairs in {DATA_DIR}")

    path_a, path_b = pairs[args.index]
    print(f"A: {path_a.name}\nB: {path_b.name}")
    t_a, acc_a = load(path_a)
    t_b, acc_b = load(path_b)

    # Acceleration: one figure per axis, A and B together on the same central-clock time axis.
    # t0 = first sample of the earlier recording, so the x axis starts near 0.
    t0 = min(t_a[0], t_b[0])
    fig, axs = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    for i, ax in enumerate(axs):
        ax.plot(t_a - t0, acc_a[:, i], linewidth=0.5, label=f"A: {path_a.stem}")
        ax.plot(t_b - t0, acc_b[:, i], linewidth=0.5, label=f"B: {path_b.stem}", alpha=0.8)
        ax.set_ylabel(f"acc {AXES[i]} [g]")
        ax.grid(True)
        ax.legend(loc="upper right")
    axs[-1].set_xlabel("time [s] (central clock, from the first sample)")
    fig.suptitle(f"Pair {args.index}: start offset B - A = {(t_b[0] - t_a[0]) * 1e6:.1f} us")
    fig.tight_layout()

    # Spectrogram: rows = axes X, Y, Z, columns = A, B. Colour = ASD in dB re 1 ug/sqrt(Hz).
    # Same colour scale in all 6 panels, so A and B and the three axes can be compared directly.
    # Each file uses its own real sample rate, so the frequency axes are correct even if the ODRs differ.
    specs = []
    for i in range(3):
        for t, acc in ((t_a, acc_a), (t_b, acc_b)):
            centers, f, psd = spectrogram(acc[:, i], 1 / np.mean(np.diff(t)), SPEC_NPERSEG, SPEC_STEP)
            specs.append((t[centers] - t0, f, 10 * np.log10(psd / 1e-12)))   # 1e-12 g^2/Hz = (1 ug)^2/Hz
    all_db = np.concatenate([s[2].ravel() for s in specs])
    vmin, vmax = np.percentile(all_db, 5), np.percentile(all_db, 99.9)

    fig, axs = plt.subplots(3, 2, figsize=(14, 9), sharex=True, sharey=True)
    for k, (tc, f, db) in enumerate(specs):
        ax = axs[k // 2, k % 2]
        mesh = ax.pcolormesh(tc, f, db.T, shading="nearest", vmin=vmin, vmax=vmax, cmap="viridis")
        ax.set_title(f"{'A' if k % 2 == 0 else 'B'} {AXES[k // 2]}: {(path_a if k % 2 == 0 else path_b).stem}", fontsize=9)
        if k % 2 == 0:
            ax.set_ylabel("frequency [Hz]")
    for ax in axs[-1]:
        ax.set_xlabel("time [s] (central clock, from the first sample)")
    fig.colorbar(mesh, ax=axs, label="ASD [dB re 1 ug/sqrt(Hz)]")
    fig.suptitle(f"Pair {args.index}: spectrogram ({SPEC_NPERSEG} samples per segment, hop {SPEC_STEP}, Hann)")

    # Cross-correlation: the two IMUs have different ODRs, so both signals are first linearly
    # interpolated onto one uniform time grid that covers only the time interval present in both recordings.
    t_start = max(t_a[0], t_b[0])
    t_end = min(t_a[-1], t_b[-1])
    if t_end <= t_start:
        raise SystemExit("Error: the two recordings do not overlap in time")
    dt = (np.mean(np.diff(t_a)) + np.mean(np.diff(t_b))) / 2
    grid = np.arange(t_start, t_end, dt)

    fig, axs = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    for i, ax in enumerate(axs):
        a = np.interp(grid, t_a, acc_a[:, i])
        b = np.interp(grid, t_b, acc_b[:, i])
        lags, r = cross_corr(a, b)
        lag_ms = lags * dt * 1e3
        k = np.argmax(np.abs(r))
        ax.plot(lag_ms, r, linewidth=0.7)
        ax.axvline(lag_ms[k], color="r", linestyle="--", linewidth=0.8)
        ax.set_ylabel(f"xcorr {AXES[i]}")
        ax.set_title(f"{AXES[i]}: peak r = {r[k]:.3f} at lag {lag_ms[k]:.3f} ms ({lags[k]} samples)")
        ax.grid(True)
        print(f"{AXES[i]}: peak r = {r[k]:.3f} at lag {lag_ms[k]:.3f} ms")
    axs[-1].set_xlabel("lag [ms] (positive = A delayed with respect to B)")
    fig.suptitle(f"Pair {args.index}: cross-correlation A vs B, {len(grid)} samples, "
                 f"grid step {dt * 1e6:.2f} us, overlap {t_end - t_start:.3f} s")
    fig.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()
