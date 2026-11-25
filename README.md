# nRF5340 BLE time sync

Master thesis project: clock synchronization between nRF5340 boards over a BLE ACL connection, and its use for synchronized IMU data collection.

The work has two phases:

1. **Sync accuracy measurement** (branch `main`): a central and N peripherals toggle a GPIO at a time chosen by the central. A Saleae logic analyzer captures the edges, and the error between boards is computed as a function of the connection interval.
2. **IMU data collection** (branch `dataset_collection`, in progress): two synced Sensei boards record floor vibrations from footsteps with the on-board LSM6DSV16BX accelerometer. The goal is to measure both the waveform and the propagation delay between the two boards.

## Repository layout

| Path | Content |
|---|---|
| `conn_time_sync/` | Zephyr / nRF Connect SDK firmware, derived from Nordic's `samples/bluetooth/conn_time_sync` (see `conn_time_sync/README.rst` for the original sample's theory) |
| `ble_sync_measurements/` | Python tools for the Saleae capture (`capture.py`) and the sync-error analysis (`analyze.py`, `std_vs_n.py`) |
| `imu_dataset/` | Python tool that downloads IMU recordings from the board RAM over SWD (work in progress) |
| `acquire_data.sh` | Automated sweep of the connection interval: build, flash, capture, analyze |

## Requirements

- nRF Connect SDK **v3.4.1** and its toolchain
- `nrfjprog` (nRF Command Line Tools) and a SEGGER J-Link
- Saleae Logic 2 (started with `--automation`) for the sync measurements
- Python 3 with the packages listed in each tool folder's `requirements.txt`

## Firmware

### How synchronization works

Both roles enable the SoftDevice Controller vendor-specific event *connection anchor point report*. The connection anchor points and their event counters form a time base shared by central and peripheral.

1. The central sends each peripheral a target time expressed in its own controller clock, together with the latest anchor point.
2. The peripheral converts the target time to its own clock using the anchor-point difference.
3. Each board then fires a hardware trigger (RTC + TIMER → PPI → GPIOTE) at that time, without CPU involvement.

### Build and flash

```bash
cd conn_time_sync

# nRF5340 DK (sync measurements)
CONN_INTERVAL_UNITS=8 west build -b nrf5340dk/nrf5340/cpuapp -p

# Sensei board (IMU data collection, branch dataset_collection)
west build -b nrf5340dk/nrf5340/cpuapp -p -- -DEXTRA_DTC_OVERLAY_FILE=boards/sensei_nrf5340_cpuapp.overlay

west flash -r nrfjprog --snr <SERIAL>    # list serials with: nrfjprog --ids
```

`CONN_INTERVAL_UNITS` is the connection interval in units of 1.25 ms (6 to 3200, default 8). It is read when the build is configured, so rebuild with `-p` (pristine) whenever you change it.

### Choosing the role

The same firmware runs on every board. After boot, send one character on the console:

- `p`: peripheral. Start the peripherals first.
- `c`: central.

On `main` the console is the UART at 115200 baud. On `dataset_collection` it is SEGGER RTT, which you open with J-Link RTT Viewer.

## Sync accuracy measurements

```bash
cd ble_sync_measurements/experiments
python3 ../capture.py --duration 600                      # writes output-<timestamp>/
python3 ../analyze.py output-YYYY-MM-DD_HH-MM-SS/ <N>     # N = CONN_INTERVAL_UNITS used for the run
cd .. && python3 std_vs_n.py                              # standard deviation vs connection interval
```

`ble_sync_measurements/conf.py` defines the analyzer channels and the sampling rate. `acquire_data.sh` automates the full sweep over N.

## IMU data collection (in progress)

**Hardware:** Sensei board (nRF5340 + GAP9). The LSM6DSV16BX is on I2C at 1 MHz, and its INT1 pin is wired to P0.23.

**Firmware** (`conn_time_sync/src/imu.c`): the IMU is configured with direct register access over I2C (no Zephyr sensor driver).

- Accelerometer settings: ±2 g, 7.68 kHz output data rate, gyroscope off.
- Samples are buffered in the IMU FIFO. Every 128 samples the IMU pulses INT1, and the nRF reads the FIFO in one I2C burst.
- The Z axis is stored as raw `int16` values in a RAM buffer, `rec_buf`, which holds 5 s of data.
- A recording starts a fixed time after the BLE connection is established. The central sends the start time over BLE and the peripheral converts it to its own clock. The start is scheduled in software with millisecond resolution, so the two recordings are only roughly aligned until per-sample hardware timestamps are added (see Status below).

**Download:** the recording is read from RAM over SWD with `nrfjprog --memrd`, which does not reset the board. The addresses of `rec_buf` and `saved_samples` are taken from `build/conn_time_sync/zephyr/zephyr.elf` and change with every build.

> Do **not** open J-Link RTT Viewer between a recording and its download. Connecting the viewer resets the board, and the reset clears `rec_buf`.

**Converting to physical units:** 0.061 mg/LSB at ±2 g, so 1 g ≈ 16393 LSB.

**Status**

- [x] IMU configuration, FIFO, INT1 interrupt, RAM recording (Z axis, 5 s), verified on hardware
- [ ] Automatic download script (`imu_dataset/download.py`)
- [ ] Hardware timestamp of each INT1 pulse, so samples can be placed on the shared time base
- [ ] Two-board synchronized recording and validation with the logic analyzer
- [ ] X and Y axes
