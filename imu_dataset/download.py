import subprocess
import numpy as np
import os
import matplotlib.pyplot as plt

from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parent.parent
ELF_FILE_PATH = ROOT / "conn_time_sync" / "build" / "conn_time_sync" / "zephyr" / "zephyr.elf"
OUTPUT_DIR = ROOT / "imu_dataset" / "data"
NM_PATH = Path("/home/m4rch0/ncs/toolchains/8285d8ad56/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm")

SHOW_PLOTS = True

ODR_HZ = 7680               # must match ODR_HZ in conn_time_sync/src/imu.c (nominal, the real one differs per chip)
COUNTER_BDR = 128           # samples between two timestamps, must match COUNTER_BDR_REG2 in conn_time_sync/src/imu.c
SENSITIVITY_G_PER_LSB = 0.061e-3   # LSM6DSV16BX at +-2 g (datasheet: 0.061 mg/LSB)

def read_board_id() -> str:
    """
    Function that returns the id of the connected board.
    If a number of boards different from 1 is detected it raises an error.
    """
    out = subprocess.run(["nrfjprog", "--ids"], capture_output=True, text=True, check=True)
    lines = out.stdout.splitlines()
    if(len(lines) == 0):
        raise RuntimeError("No board detected")
    elif(len(lines) > 1):
        raise RuntimeError("More than one board detected")
    return lines[0]

def find_symbols() -> dict:
    """
    Finds the firmware variables rec_buf, saved_samples, rec_ts and saved_ts in the ELF file (nm -S).
    Returns a dict with the addresses of all four (keys *_addr) and the size in bytes of
    the two buffers (rec_buf_dim, rec_ts_dim).
    """
    out = subprocess.run([NM_PATH, "-S", ELF_FILE_PATH], capture_output=True, text=True, check=True).stdout.splitlines()
    rec_buf = None
    saved_samples = None
    rec_ts = None
    saved_ts = None
    for line in out:
        fields = line.split()
        if("rec_buf" == fields[-1]):
            rec_buf = fields
        elif("saved_samples" == fields[-1]):
            saved_samples = fields
        elif("rec_ts" == fields[-1]):
            rec_ts = fields
        elif("saved_ts" == fields[-1]):
            saved_ts = fields

    if(rec_buf is None):
        raise RuntimeError("Error: couldn't find rec_buf variable in the elf file")

    if(saved_samples is None):
        raise RuntimeError("Error: couldn't find saved_samples variable in the elf file")

    if(rec_ts is None):
        raise RuntimeError("Error: couldn't find rec_ts variable in the elf file")

    if(saved_ts is None):
        raise RuntimeError("Error: couldn't find saved_ts variable in the elf file")

    if(len(rec_buf) != 4):
        raise RuntimeError("Error: a field of rec_buf is missing")
    
    if(len(saved_samples) != 4):
        raise RuntimeError("Error: a field of saved_samples is missing")

    if(len(rec_ts) != 4):
        raise RuntimeError("Error: a field of rec_ts is missing")

    if(len(saved_ts) != 4):
        raise RuntimeError("Error: a field of saved_ts is missing")

    res = {
        "rec_buf_addr": int(rec_buf[0], 16),
        "rec_buf_dim": int(rec_buf[1], 16),
        "saved_samples_addr": int(saved_samples[0], 16),
        "rec_ts_addr": int(rec_ts[0], 16),
        "rec_ts_dim": int(rec_ts[1], 16),
        "saved_ts_addr": int(saved_ts[0], 16)
    }

    return res

def read_memory(board_id: str, addr: int, n_byte: int, width: int) -> list:
    """
    Read the RAM of the selected board at the address addr.
    n_byte is the total data read from memory in byte.
    width is how many bit are grouped in each word.
    """
    out = subprocess.run(["nrfjprog", "--snr", board_id, "--memrd", str(addr), "--w", str(width), "--n", str(n_byte)], capture_output=True, text=True, check=True).stdout
    words = []
    for line in out.splitlines():
        fields = line.split("|")[0]
        fields = fields.split()[1:]
        for field in fields:
            words.append(int(field, 16))
    return words

def correct_tick_err(buf_ts: np.ndarray) -> np.ndarray:
    """
    This procedure is necessary since it happens sometimes that the interrupt of the "ready samples" of
    the IMU on INT1 is raised when TIMER overflows and the PPI is incrementing RTC so the samples gets a
    wrong timestamp, off just 1 tick.
    """

    median = np.median(np.diff(buf_ts))
    if(buf_ts[1] - buf_ts[0] - median < -15000):
        buf_ts[0] -= 30518

    for i in range(len(buf_ts) - 1):
        if(buf_ts[i + 1] - buf_ts[i] - median > 15000):
            buf_ts[i + 1] -= 30518
    return buf_ts

def plots(data: np.ndarray, ts: np.ndarray) -> None:
    """
    Plot to check the data: Z acceleration over time, histogram of the values and timestamp residuals.
    data is the raw int16 array (LSB), converted to g only for plotting.
    ts are the timestamps in ns (central clock), one every COUNTER_BDR samples:
    timestamp k belongs to sample index (k + 1) * COUNTER_BDR - 1.
    The time axis comes from a linear fit of ts vs sample index, so it uses the real ODR of this IMU, not ODR_HZ.
    """
    z_g = data * SENSITIVITY_G_PER_LSB

    ts_idx = (np.arange(len(ts)) + 1) * COUNTER_BDR - 1     # sample index of each timestamp
    ts_rel = (ts - ts[0]).astype(np.float64)                # relative to the first ts, small values for the fit
    slope, intercept = np.polyfit(ts_idx, ts_rel, 1)         # slope = ns per sample
    odr_real = 1e9 / slope

    t = (np.arange(len(data)) * slope + intercept + ts[0]) / 1e9   # time of every sample, s, central clock
    residuals_us = (ts_rel - (ts_idx * slope + intercept)) / 1e3   # ts minus fit, us

    fig, (ax_time, ax_hist, ax_res) = plt.subplots(3, 1, figsize=(10, 10))

    ax_time.plot(t, z_g, linewidth=0.5)
    ax_time.set_xlabel("time [s] (central clock)")
    ax_time.set_ylabel("acc Z [g]")
    ax_time.set_title(f"{len(data)} samples, {t[-1] - t[0]:.3f} s, real ODR = {odr_real:.1f} Hz "
                      f"({(odr_real / ODR_HZ - 1) * 100:+.2f} % vs {ODR_HZ} Hz)")
    ax_time.grid(True)

    ax_hist.hist(z_g, bins=100)
    ax_hist.set_xlabel("acc Z [g]")
    ax_hist.set_ylabel("count")
    ax_hist.set_title(f"mean = {z_g.mean():.4f} g, std = {z_g.std() * 1e3:.2f} mg")
    ax_hist.grid(True)

    ax_res.plot(ts / 1e9, residuals_us, ".", markersize=3)
    ax_res.set_xlabel("time [s] (central clock)")
    ax_res.set_ylabel("ts - fit [us]")
    ax_res.set_title(f"{len(ts)} timestamps (after +1 RTC tick correction), residual std = {residuals_us.std():.2f} us")
    ax_res.grid(True)

    fig.tight_layout()
    plt.show()

def main():
    board_id = read_board_id()
    syms = find_symbols()
    saved_samples = read_memory(board_id, syms["saved_samples_addr"], 4, 32) # 4 byte is the dim of the var, equal to 32 bit, the type is: uint32_t

    if(len(saved_samples) != 1):
        raise RuntimeError(f"Error: unexpected length of saved_samples. Expected: 1, got: {len(saved_samples)}")
    if(saved_samples[0] == 0):
        raise RuntimeError("Error: no recording in RAM, the board has been reset")
    if(saved_samples[0] * 6 > syms["rec_buf_dim"]): # saved_samples is the number of samples, each sample's size is 2B * 3 axis
        raise RuntimeError(f"Error: dimensions do not match. Byte saved: {6 * saved_samples[0]}, rec_buf size: {syms["rec_buf_dim"]}")
    
    rec_buf = read_memory(board_id, syms["rec_buf_addr"], saved_samples[0] * 6, 16)   # 16 bit because rec_buf is int16_t

    if(len(rec_buf) != saved_samples[0] * 3): # 3: 1 for each axis
        raise RuntimeError(f"Error: unexpected length of rec_buf. Expected: {saved_samples[0] * 3}, got: {len(rec_buf)}")

    saved_ts = read_memory(board_id, syms["saved_ts_addr"], 4, 32)

    if(len(saved_ts) != 1):
        raise RuntimeError(f"Error: unexpected length of saved_ts. Expected: 1, got: {len(saved_ts)}")
    if(saved_ts[0] == 0):
        raise RuntimeError("Error: no recording in RAM, the board has been reset")
    if(saved_ts[0] * 8 > syms["rec_ts_dim"]): # saved_ts is the number of timestamps, each timestamp's size is 8B (uint64_t)
        raise RuntimeError(f"Error: dimensions do not match. Byte saved: {8 * saved_ts[0]}, rec_ts size: {syms["rec_ts_dim"]}")

    rec_ts_spl = read_memory(board_id, syms["rec_ts_addr"], saved_ts[0] * 8, 32)   # 32 bit words: each uint64_t timestamp is split into 2 words

    rec_ts = np.zeros(int(len(rec_ts_spl) / 2), dtype=np.uint64)
    for i in range(int(len(rec_ts))):
        rec_ts[i] = rec_ts_spl[2 * i] + (rec_ts_spl[2 * i + 1] << 32)

    if(len(rec_ts) != saved_ts[0]):
        raise RuntimeError(f"Error: unexpected length of rec_ts. Expected: {saved_ts[0]}, got: {len(rec_ts)}")
    
    rec_buf = np.array(rec_buf, dtype=np.uint16).view(np.int16).reshape(-1, 3)
    
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    dt = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    filename_npz = f"{dt}_{board_id}.npz"
    filename_txt = f"{dt}_{board_id}.txt"
    path = OUTPUT_DIR / filename_npz
    txt_path = OUTPUT_DIR / filename_txt
    txt_path.touch()
    np.savez(path, acc=rec_buf, timestamp=correct_tick_err(rec_ts))

    if(SHOW_PLOTS):
        plots(rec_buf[:, 2], rec_ts)

if __name__ == "__main__":
    main()