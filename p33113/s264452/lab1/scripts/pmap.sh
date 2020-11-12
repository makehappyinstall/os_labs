#!/bin/bash
pid="$(ps -ef | grep "main.o" | grep -v grep | awk '{print $2}')"
sudo pmap -x $pid > pmap.output
