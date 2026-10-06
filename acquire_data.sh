#!/bin/bash

echo "-----------------------------------------------------------------------------"
echo "-----------------------------------------------------------------------------"
echo "***************************  STARTING SIMULATION  ***************************"
echo "-----------------------------------------------------------------------------"
echo "-----------------------------------------------------------------------------"

echo "***************************  PREPARING VARIABLES FOR WEST COMMAND  ***************************"

west() {
    TOOLCHAIN=/home/m4rch0/ncs/toolchains/8285d8ad56 \
    PATH="$TOOLCHAIN/bin:$TOOLCHAIN/usr/bin:$TOOLCHAIN/usr/local/bin:$TOOLCHAIN/opt/bin:$TOOLCHAIN/opt/zephyr-sdk/arm-zephyr-eabi/bin:$PATH" \
    LD_LIBRARY_PATH="$TOOLCHAIN/lib:$TOOLCHAIN/lib/x86_64-linux-gnu:$TOOLCHAIN/usr/local/lib:$LD_LIBRARY_PATH" \
    PYTHONHOME="$TOOLCHAIN/usr/local" \
    ZEPHYR_BASE=/home/m4rch0/ncs/v3.4.1/zephyr \
    command west "$@"
}

MAX_N=3200
N_SIM=10
INC=$(( MAX_N / N_SIM ))
DURATION=800

echo "*************************** STARTING LOGIC ANALYZER SOFTWARE  ***************************"

~/Downloads/Logic-2.4.46-linux-x64.AppImage --automation > /dev/null &
cd conn_time_sync

for (( i=INC; i<=MAX_N; i+=INC )); do
    echo "Simulatin with N = $i"
    echo "***************************  BUILDING  ***************************"
    west build -b nrf5340dk/nrf5340/cpuapp -p \
    -- -Dconn_time_sync_CONN_INTERVAL_UNITS=$i \
    > log.txt || exit 1
    IS_FIRST=1
    COUNTER=1
    echo "***************************  FLASHING FIRMWARE  ***************************"
    nrfjprog --ids | while read id; do
        west flash -r nrfjprog --snr $id > log.txt < /dev/null || exit 1
        sleep 3
        if [ $IS_FIRST -eq 1 ]; then
            CHAR="c"
            IS_FIRST=0
        else
            CHAR="p"
        fi
        stty -F /dev/ttyACM$COUNTER 115200 raw -echo
        echo $CHAR > /dev/ttyACM$COUNTER
        (( COUNTER+=2 ))
    done 
    echo "***************************  STARTING CAPTURE  ***************************"
    source ../ble_sync_measurements/venv/bin/activate
    cd ../ble_sync_measurements/experiments
    sleep 5
    python3 ../capture.py --duration $DURATION
    (( DURATION += INC ))
    OUT=$(ls -dt output-*/ | head -1)
    python3 ../analyze.py $OUT $i
    cd ../conn_time_sync
done

kill %1