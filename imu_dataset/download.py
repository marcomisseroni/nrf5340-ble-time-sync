import subprocess

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF_FILE_PATH = ROOT / "conn_time_sync" / "build" / "conn_time_sync" / "zephyr" / "zephyr.elf"
OUTPUT_DIR = ROOT / "imu_dataset" / "data"

def read_board_id() -> subprocess.CompletedProcess:
    out = subprocess.run(["nrfjprog", "--ids"], capture_output=True, text=True, check=True)
    # extract board id
    return out

def find_symbols():
    pass
    
def main():
    board_id = read_board_id()

if __name__ == "__main__":
    main()