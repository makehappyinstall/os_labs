#!/bin/bash
pid="$(ps -ef | grep "lab1" | grep -v grep | awk '{print $2}')"
top | grep "lab1"
sudo pmap -x $pid
sudo strace ./lab1
sudo stap -x $pid statistics
