#! /bin/bash

PROCESS_NAME="os_lab_1"

pid=$(pidof $PROCESS_NAME)

if [[ $pid != '' ]]; then
    echo "$PROCESS_NAME process is found. PID=$pid"
    sudo ./script.stp -x $pid
else
    echo "$PROCESS_NAME process is not found"
fi
