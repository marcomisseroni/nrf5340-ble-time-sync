import subprocess
import numpy as np

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF_FILE_PATH = ROOT / "conn_time_sync" / "build" / "conn_time_sync" / "zephyr" / "zephyr.elf"
OUTPUT_DIR = ROOT / "imu_dataset" / "data"
NM_PATH = Path("/home/m4rch0/ncs/toolchains/8285d8ad56/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm")

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

def find_symbols() -> tuple[int, int, int]:
    """
    Finds the memory location of rec_buf and saved_samples and the dimension of rec_buf, variables used in the firmware.
    The order is: rec_buf add, rec_buf dim, saved_samples add.
    """
    out = subprocess.run([NM_PATH, "-S", ELF_FILE_PATH], capture_output=True, text=True, check=True).stdout.splitlines()
    rec_buf = None
    saved_samples = None
    for line in out:
        fields = line.split()
        if("rec_buf" == fields[-1]):
            rec_buf = fields
        elif("saved_samples" == fields[-1]):
            saved_samples = fields

    if(rec_buf is None):
        raise RuntimeError("Error: couldn't find rec_buf variable in the elf file")

    if(saved_samples is None):
        raise RuntimeError("Error: couldn't find saved_samples variable in the elf file")

    if(len(rec_buf) != 4):
        raise RuntimeError("Error: a field of rec_buf is missing")
    
    if(len(saved_samples) != 4):
        raise RuntimeError("Error: a field of saved_samples is missing")

    return int(rec_buf[0], 16), int(rec_buf[1], 16), int(saved_samples[0], 16)

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

def main():
    board_id = read_board_id()
    rec_buf_add, rec_buf_dim, saved_samples_add = find_symbols()
    saved_samples = read_memory(board_id, saved_samples_add, 4, 32) # 4 byte is the dim of the var, equal to 32 bit, the type is: uint32_t

    if(len(saved_samples) != 1):
        raise RuntimeError(f"Error: unexpected length of saved_samples. Expected: 1, got: {len(saved_samples)}")
    if(saved_samples[0] == 0):
        raise RuntimeError("Error: no recording in RAM, the board has been reset")
    if(saved_samples[0] * 2 > rec_buf_dim): # saved_samples is the number of samples, each sample's size is 2B
        raise RuntimeError(f"Error: dimensions do not match. Byte saved: {2 * saved_samples[0]}, rec_buf size: {rec_buf_dim}")
    
    rec_buf = read_memory(board_id, rec_buf_add, saved_samples[0] * 2, 16)   # 16 bit because rec_buf is int16_t

    if(len(rec_buf) != saved_samples[0]):
        raise RuntimeError(f"Error: unexpected length of rec_buf. Expected: {saved_samples[0]}, got: {len(rec_buf)}")
    
    rec_buf = np.array(rec_buf, dtype=np.uint16).view(np.int16)


if __name__ == "__main__":
    main()