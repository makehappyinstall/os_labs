 #! /bin/bash

 PROCESS="./lab1.out"
 PID=$(pidof $PROCESS)

 if [ $PID != '' ]
  then 
    echo "$PROCESS process is found. PID=$PID"
    sudo stap -x $PID ./statistics.stp > statistics.txt
  else
    echo "$PROCESS process is not found"
 fi
 