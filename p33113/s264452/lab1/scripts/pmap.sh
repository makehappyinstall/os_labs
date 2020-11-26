#!/bin/bash
pid="$(ps -ef | grep "main" | grep -v grep | awk '{print $2}')"
sudo pmap -x $pid > pmap.output
