import numpy as np
import argparse
import yaml
import pywt
import matplotlib.pyplot as plt

from scipy.interpolate import CubicSpline
from scipy import signal
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "imu_dataset" / "data"
SPEED_DIR = DATA_DIR / "speed"

LOW_CUTOFF = 150    # Low cutoff must account for the relative dim of wavelt and window, if the wavelet * scale is too big respect to the window the estimate is bad
HIGH_CUTOFF = 3000
N_SCALES = 100
V_MIN = 100

def init() -> argparse.Namespace:
    """
    Parses the arguments.
    """
    files = sorted(DATA_DIR.glob("*.npz"))

    if(len(files) < 2):
        raise RuntimeError("Not enough files, at least 2 files are required")

    if1 = files[2]
    if2 = files[3]
    parser = argparse.ArgumentParser(description="Compute the speed of the vibration waves in the ground.")
    parser.add_argument("--if1", default=if1, type=Path, help="Input file 1")
    parser.add_argument("--if2", default=if2, type=Path, help="Input file 2")
    parser.add_argument("--odir", default=SPEED_DIR, type=Path, help="Output directory")
    args = parser.parse_args()
    args.odir.mkdir(parents=True, exist_ok=True)
    return args

def read_distance(args: argparse.Namespace) -> float:
    if1 = args.if1
    if2 = args.if2
    odir = args.odir
    yaml1 = if1.with_suffix(".yaml")
    yaml2 = if2.with_suffix(".yaml")
    with open(yaml1, "r") as f:
        yaml_dict1 = yaml.safe_load(f)
    with open(yaml2, "r") as f:
        yaml_dict2 = yaml.safe_load(f)
    if((yaml_dict1["distance"] == "" or yaml_dict1["distance"] is None) and (yaml_dict2["distance"] == "" or yaml_dict2["distance"] is None)):
        raise RuntimeError("Distance between sensors not found in yaml files")
    if(yaml_dict1["distance"] != "" and yaml_dict1["distance"] is not None):
        distance = yaml_dict1["distance"]
    else:
        distance = yaml_dict2["distance"]
    return distance

def timestamp_interp(timestamp_vec: np.ndarray, sample_vec: np.ndarray) -> np.ndarray:
    """
    The board collects just 1 timestamp every 128 samples so to find the complete timestamp vector
    we can use interpolation.
    """
    idx_vec = np.zeros(len(timestamp_vec))
    for i in range(len(idx_vec)):
        idx_vec[i] = 128 * (i + 1) - 1

    idx_complete = np.arange(0, len(sample_vec), 1)
    
    # Correction for timestamps before the first timestamp recorded
    T = (timestamp_vec[1] - timestamp_vec[0]) / 128
    idx_before = np.arange(0, 127, 1)                                   # samples 0..126, before the first timestamp (sample 127)
    corr_timestamps = timestamp_vec[0] - (127 - idx_before) * T         # go back from ts[0] one period per sample
    ts = np.interp(idx_complete, idx_vec, timestamp_vec)
    ts[:len(corr_timestamps)] = corr_timestamps
    return ts

def compute_wavelet_t(fs: float, data: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """
    Calculation of the wavelet transform.
    """
    freq = np.geomspace(LOW_CUTOFF, HIGH_CUTOFF, N_SCALES)
    scales = pywt.central_frequency('mexh') * fs / freq
    coefs, f = pywt.cwt(data, scales, 'mexh', sampling_period=1/fs, method='fft', axis=-1)
    return coefs, f

def acc_interp(acc_z1: np.ndarray, acc_z2: np.ndarray, ts1: np.ndarray, ts2: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    To compute the cross-correlation the 2 signals must have the same sampling frequency so we extract 
    from acc_z1 all the common samples with acc_z2 and then we interpolate the samples of acc_z2 using as
    timestamps vector the timestamps of acc_z1. Since we need it for the cross-correlation we can interpolate
    acc_z2 before computing the wavelet transform so we have just 1 vector of scales.
    """
    acc_z1 = (acc_z1 - np.mean(acc_z1)) * 0.061 * 1e-3  # Subtract mean and convert in g
    acc_z2 = (acc_z2 - np.mean(acc_z2)) * 0.061 * 1e-3    
    start = max(ts1[0], ts2[0])
    end = min(ts1[-1], ts2[-1])
    start_idx = [i for i, x in enumerate(ts1) if x >= start][0]
    end_idx = [i for i, x in enumerate(ts1) if x <= end][-1]
    acc_z1 = acc_z1[start_idx:end_idx + 1]
    ts1 = ts1[start_idx:end_idx + 1]
    acc_z2_curve = CubicSpline(ts2, acc_z2)
    acc_z2 = acc_z2_curve(ts1)
    return acc_z1, acc_z2, ts1

def find_step(acc_z1: np.ndarray, acc_z2: np.ndarray, ts1: np.ndarray, fs: float) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Find a step in both recordings to compute the wavelet transform only on that window.
    """
    margin = int(0.1 * fs)
    max_idx = np.argmax(abs(acc_z1[margin:-margin])) + margin
    pre_margin = int(0.02 * fs)
    post_margin = int(0.04 * fs)
    window1 = acc_z1[max_idx - pre_margin : max_idx + post_margin + 1]
    window2 = acc_z2[max_idx - pre_margin : max_idx + post_margin + 1]
    ts_step = ts1[max_idx - pre_margin : max_idx + post_margin + 1]
    return window1, window2, ts_step

def plot_vel(window1: np.ndarray, window2: np.ndarray, ts_step: np.ndarray,
             freq: np.ndarray, vel_vec: np.ndarray) -> None:
    """
    Top: the two signals in the step window. Bottom: wave speed vs frequency.
    """
    t_ms = (ts_step - ts_step[0]) * 1e-6
    fig, (ax_sig, ax_vel) = plt.subplots(2, 1, figsize=(12, 8))

    ax_sig.plot(t_ms, window1 * 1e3, label="sensor 1 (if1)")
    ax_sig.plot(t_ms, window2 * 1e3, label="sensor 2 (if2, interpolated)")
    ax_sig.set_xlabel("time from window start [ms]")
    ax_sig.set_ylabel("acceleration Z [mg]")
    ax_sig.set_title("Step window")
    ax_sig.legend()
    ax_sig.grid(True)

    ax_vel.semilogx(freq, vel_vec, "o-")
    ax_vel.set_xlabel("frequency [Hz]")
    ax_vel.set_ylabel("speed [m/s]")
    ax_vel.set_title("Wave speed vs frequency")
    ax_vel.grid(True, which="both")

    plt.tight_layout()
    plt.show()

def print_vel(freq: np.ndarray, time_lags: np.ndarray, vel_vec: np.ndarray, distance: float) -> None:
    """
    Prints the wave speed found for every frequency as a table.
    """
    print(f"distance = {distance} m")
    print(f"{'freq [Hz]':>10} | {'delay [ms]':>10} | {'speed [m/s]':>11}")
    print(f"{'-' * 10}-+-{'-' * 10}-+-{'-' * 11}")
    for f, dt, v in zip(freq, time_lags, vel_vec):
        print(f"{f:>10.1f} | {dt * 1e3:>10.3f} | {v:>11.1f}")

def main() -> None:
    args = init()
    data1 = np.load(args.if1)
    data2 = np.load(args.if2)
    acc1 = data1["acc"] # shape is (samples, 3)
    ts1 = timestamp_interp(np.float64(data1["timestamp"]), acc1)
    acc2 = data2["acc"]
    ts2 = timestamp_interp(np.float64(data2["timestamp"]), acc2)
    acc_z1 = acc1[:, 2]
    acc_z2 = acc2[:, 2]
    acc_z1, acc_z2, ts1 = acc_interp(acc_z1, acc_z2, ts1, ts2)
    T = np.mean(np.diff(ts1))
    fs = 1e9 / T
    window1, window2, ts_step = find_step(acc_z1, acc_z2, ts1, fs)
    coefs_wv_w1, f1 = compute_wavelet_t(fs, window1)
    coefs_wv_w2, _ = compute_wavelet_t(fs, window2)

    time_lags = np.zeros(N_SCALES)
    distance = float(read_distance(args))
    max_lag = distance / V_MIN * fs

    for i in range(N_SCALES):
        cc = signal.correlate(coefs_wv_w1[i, :], coefs_wv_w2[i, :], mode="full")         
        corr_lags = signal.correlation_lags(len(coefs_wv_w1[i, :]), len(coefs_wv_w2[i, :]), mode="full")
        mask = abs(corr_lags) < max_lag
        cc = cc[mask]
        corr_lags = corr_lags[mask] 
        time_lags[i] = corr_lags[np.argmax(cc)] / fs

    if(np.any(time_lags == 0)):
        print("Error: time lags contains zeros, velocity cannot be computed (division by 0)")
        return
    vel_vec = distance / abs(time_lags)
    print_vel(f1, time_lags, vel_vec, distance)
    plot_vel(window1, window2, ts_step, f1, vel_vec)


if __name__ == "__main__":
    main()