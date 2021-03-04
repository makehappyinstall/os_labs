#!/bin/bash
sudo stap -x $(pidof lab1) stap_script.stp > stap_results.txt
