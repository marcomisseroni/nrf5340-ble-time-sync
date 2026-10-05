# BLE Sync Measurements – Experiment Setup

## Hardware

### Components

- 3 boards:
  - **Peripheral 1**
  - **Peripheral 2**
  - **Central**
- 1 Saleae Logic analyzer (Logic Pro 8, or the previous analyzer)
- Jumper wires

### Wiring

1. **Common ground:** connect the GND pins of all three boards together.
2. **Logic analyzer ground:** connect a GND lead of the logic analyzer to the common ground.
3. **Signal channels:** use jumpers to connect three logic analyzer channels to pin **P1.06** on each board, following the table for your analyzer below.

The channels in the tables must match the ones in `conf.py`, which both `capture.py` and `analyze.py` read.

#### Saleae Logic Pro 8

| Board        | Pin   | Analyzer label | API index (D) |
|--------------|-------|----------------|---------------|
| Peripheral 1 | P1.06 | 1              | D1            |
| Peripheral 2 | P1.06 | 2              | D2            |
| Central      | P1.06 | 3              | D3            |

- The channel numbers printed on the Logic Pro 8 harness start from 0, so the label and the API index are the same.
- Every channel has its own signal and ground wire. Connecting one ground wire to the common ground is enough, but grounding the wire paired with each used channel gives cleaner edges at high sample rates.
- Connect the analyzer to a **USB 3** port. High sample rates (`sampling_rate` in `conf.py`) are only available over USB 3.
- Set the logic threshold to **1.2 V** in the Logic 2 device settings. `capture.py` does not set it, so the capture uses the value selected in the app.

#### Previous analyzer

| Board        | Pin   | Analyzer label (CH) | API index (D) |
|--------------|-------|---------------------|---------------|
| Peripheral 1 | P1.06 | CH4                 | D3            |
| Peripheral 2 | P1.06 | CH6                 | D5            |
| Central      | P1.06 | CH8                 | D7            |

- **CH** is the channel number printed on the logic analyzer's label (it starts from 1).
- **D** is the channel index, starting from 0. The Logic 2 app and its Python automation API use this index.
- With this analyzer, set `CENTRAL = 7`, `PERIPHERAL_1 = 3`, `PERIPHERAL_2 = 5` and `sampling_rate = 2_000_000` in `conf.py`.

## Software

### Prerequisites

- [Logic 2](https://www.saleae.com/downloads/) installed
- Python 3 with the automation package:

  ```bash
  pip install logic2-automation
  ```

### 1. Start Logic 2 with automation enabled

Start the Logic 2 AppImage with the `--automation` flag. This opens the scripting socket server on port `10430`, which `capture.py` connects to:

```bash
./Logic-2.x.x-linux-x64.AppImage --automation
```

Leave the app running while you capture data.

### 2. Configure the channels

Edit `conf.py` so that the channel of each board and the sampling rate match the analyzer you are using (see [Wiring](#wiring)).

### 3. Capture data

Run the capture script:

```bash
python capture.py
```

The script records the enabled channels for the configured duration. It saves the results in a new timestamped folder in the current directory:

```
output-YYYY-MM-DD_HH-MM-SS/
├── digital.csv           # raw GPIO transitions (timestamps + channel states)
└── example_capture.sal   # full capture, can be reopened in the Logic 2 GUI
```

### 4. Analyze the data

Run the analysis script on an output folder to compute the synchronization statistics:

```bash
python analyze.py output-YYYY-MM-DD_HH-MM-SS/
```
