#!/bin/bash
sudo stap -x $(pgrep lab1) stap_script.stp &> stap_out
