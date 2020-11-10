 #! /bin/bash

 PROCESS="./lab1.out"
 pid=$(pidof $PROCESS) 
 echo "$PROCESS process is found. PID=$pid"
 sudo stap -x $pid ./statistics.stp > statistics.txt