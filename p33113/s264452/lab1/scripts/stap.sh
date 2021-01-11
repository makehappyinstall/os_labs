#!/bin/bash

pid="$(ps -ef | grep "main" | grep -v grep | awk '{print $2}')"
sudo stap -x $pid scripts/stap_script.stp > stap.output 

