# Changes from the original sample

**Base:** Nordic sample `nrf/samples/bluetooth/conn_time_sync` from nRF Connect SDK **v3.4.1**
(`~/ncs/v3.4.1/nrf/samples/bluetooth/conn_time_sync`).
**Target:** `nrf5340dk/nrf5340/cpuapp`, 1 central + 2 peripherals.

The configuration files (`prj.conf`, `boards/*`, `Kconfig.sysbuild`, `sysbuild/`), `CMakeLists.txt`,
`main.c`, `central.c` and `timed_led_toggle.c` are **identical to the original**.
The modified files are:

| File | Changes |
|---|---|
| `src/controller_time_nrf53_app.c` | 1, 2, 3, 4, 5 |
| `src/peripheral.c` | 3 |
| `include/conn_time_sync.h` | 3 |

To regenerate the full diff:
```sh
diff -ru -x build ~/ncs/v3.4.1/nrf/samples/bluetooth/conn_time_sync conn_time_sync
```

---

## 1. 16 MHz TIMER (62.5 ns trigger resolution)
*2026-09-25, `controller_time_nrf53_app.c`, `timer_config()`*

On the nRF5340 the LED trigger combines RTC0 (30.52 µs tick) with TIMER0. TIMER0 is cleared on
every RTC tick and provides the fine part of the time.

| | Original | Modified |
|---|---|---|
| TIMER0 frequency | 1 MHz (1 µs) | **16 MHz (62.5 ns)** |
| TIMER0 bit width | 8 bits | **16 bits** (one tick = ~488 counts, does not fit in 8 bits) |

**Reason:** sub-µs resolution, so that fractional bias corrections can be applied.

## 2. Trigger computation in ns
*2026-09-25, `controller_time_nrf53_app.c`, `controller_time_trigger_set()`*

- `timer_val` is now expressed in **16 MHz counts** instead of µs (type `uint16_t`, was `uint8_t`).
  Without this change, after change 1, the trigger fired up to ~28 µs early.
- The **RTC tick start time is computed exactly in ns**. The original truncated it to the µs
  (`rtc_ticks_to_us()`), which added up to ~1 µs of error depending on where the trigger fell within the tick.
- The whole computation (RTC overflow, tick, counts) starts from the timestamp in ns.
- The fine part is computed from the **remainder after the overflow** instead of the full timestamp.
  In the original this only worked by accident: 512·10⁶ is a multiple of 256, so the truncation to
  `uint8_t` cancelled the overflows. With 16 bits this is no longer true, and after the first RTC
  overflow (~8.5 min) the computation would have been wrong.
- Removed `us_to_rtc_ticks()`, which is no longer used.

## 3. Peripheral bias correction (offset in ns)
*2026-09-25, `controller_time_nrf53_app.c`, `peripheral.c`, `conn_time_sync.h`*

- New API `controller_time_trigger_offset_ns_set(int32_t offset_ns)`: a constant offset in ns,
  added to every trigger inside `controller_time_trigger_set()`. A positive value delays the trigger.
- `peripheral.c`: `#define PERIPHERAL_TRIGGER_CORRECTION_NS`, applied in `peripheral_start()`.
  The central does not call it, so its offset stays 0.
- Current value: **1800 ns** (first attempt: 1450 ns).

**Reason:** with the original sample the peripherals fired on average ~1.45 µs **before** the central
(measurement of 2026-09-24). The offset has to be applied inside `controller_time_trigger_set()`,
because along the chain `peripheral.c` → `timed_led_toggle_trigger_at()` the time travels as
integer µs and the fractional part would be lost.

**Limitations:**
- The effective resolution is 62.5 ns, with truncation.
- The function only exists in the nRF53 backend: builds for nRF52/nRF54 no longer link.

## 4. TIMER compare margin from the tick edges
*2026-09-25, `controller_time_nrf53_app.c`, `controller_time_trigger_set()`*

| | Original (1 MHz) | First version (16 MHz) | Current (16 MHz) |
|---|---|---|---|
| `timer_val` limits | 1 … 30 µs | 1 … 487 counts | **8 … 480 counts** (0.5 … 30.0 µs) |
| Margin to the next tick | ~0.52 µs | ~0.08 µs | **~0.52 µs** |

**Problem:** with limits 1…487 an outlier of **−29.5 µs** appeared, i.e. one RTC tick early
(1 event out of 362, measurement of 2026-09-25 16:43). TIMER0 is cleared by the RTC TICK event,
while the PPI group is enabled by the COMPARE event: these are two different DPPI channels. If the
group is enabled just before the clear, a compare value close to the end of the tick matches in the
**previous tick**. At 16 MHz the margin had dropped to ~80 ns, the same order as the
synchronization jitter.

**Fix:** restored a ~0.5 µs margin from both edges. The cost is that triggers within ~0.5 µs of an
edge are shifted by at most 0.5 µs (the original shifted them by up to 1 µs).

**Result:** no outliers in the following 800 s captures.

## 5. HFXO always on in the application core
*2026-09-25, `controller_time_nrf53_app.c`, `hfxo_start()` in `controller_time_init()`*

- At startup the HF clock is requested through `z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF)`
  and the request is never released. TIMER0 therefore counts with the **32 MHz crystal (HFXO)**
  instead of the internal RC oscillator (HFINT).
- Enabled or disabled with `#define USE_HFXO 1/0`.

**Reason:** check whether the bias difference between the two peripherals (see below) comes from
the HFINT tolerance, which differs from chip to chip.
**Result:** the bias difference between the two peripherals is **halved** (from ~0.27 µs to
~0.14 µs, capture 09-25 18:34). The dispersion is unchanged (~0.67 µs). HFINT explains about half
of the difference, and the residual has another cause. To be confirmed with back-to-back
`USE_HFXO 0/1` captures, because the HFINT frequency also depends on temperature.

---

## Measurements

Setup: see `ble_sync_measurements/instructions.md`. Saleae at 2 MS/s (0.5 µs resolution), 800 s captures.
The error is the peripheral edge time minus the central edge time (positive = peripheral late).

| Capture | Firmware | P1−C mean | P2−C mean | P2−P1 | Notes |
|---|---|---|---|---|---|
| 09-24 21:16 | original | −1.45 µs | −1.44 µs | 0.01 µs | std ~0.9 µs |
| 09-25 16:43 | changes 1–3, offset 1450 ns, limits 1…487 | −0.56 µs | −0.37 µs | — | 100 s, **1 outlier −29.5 µs** on P2 |
| 09-25 17:17 | changes 1–4, offset 1450 ns | −0.54 µs | −0.25 µs | 0.29 µs | std ~0.68 µs, no outliers |
| 09-25 17:39 | changes 1–4, offset 1800 ns | −0.26 µs | +0.03 µs | 0.29 µs | std ~0.68 µs, no outliers |
| 09-25 18:03 | as 17:39, **P1/P2 channels swapped** | +0.12 µs | −0.15 µs | −0.28 µs | first 635 s; then P2 misses a toggle |
| 09-25 18:34 | changes 1–5 (**HFXO**), offset 1800 ns, channels as 18:03 | +0.03 µs | −0.11 µs | −0.14 µs | std ~0.67 µs, max \|Px−C\| 2.0 µs, no outliers |

The firmware of each capture was reconstructed from the modification times of the files.

**Observations:**
- Going from a 1450 ns to an 1800 ns offset, the means shift by ~0.29 µs, consistent with the
  +350 ns applied (given the 62.5 ns resolution).
- **Bias difference between the peripherals (~0.28 µs):** constant over time and across captures.
  With the channels swapped it **changes sign**, so it follows the **physical board** and not the
  Saleae channel. A common offset can only zero the mean of the two boards. With the HFXO
  (change 5) the difference drops to ~0.14 µs, so about half of it came from the HFINT tolerance.
  Candidate cause of the residual: a ppm difference between the 32.768 kHz crystals (to be checked
  by changing the connection interval). It is unclear why with the original firmware (09-24) the
  difference was ~0.

---

## Known issues (not fixed)

- **Toggle instead of set/clear.** The GPIOTE is configured with `POLARITY_TOGGLE` and the
  `led_value` field of the packet is ignored (`timed_led_toggle.c`). If a trigger is missed, the
  board stays in antiphase forever. This happened in the 09-25 18:03 capture at t = 635.3 s on one
  peripheral. Cause of the miss not determined.
- **`conn_interval_us` is `uint16_t`** in `peripheral.c`: fine at 10 ms, but truncated above ~65 ms.
- **Portability:** the anchor points come from vendor-specific HCI events of the Nordic SoftDevice
  Controller, so the code does not work with other BLE controllers.
- **`analyze.py`:** `dropna()` drops an event for **all** pairs if even one peripheral is missing.
  In the 18:03 capture the last 165 s were also dropped for P1−C.
