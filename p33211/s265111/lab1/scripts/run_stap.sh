#!/bin/bash
sudo stap script.stp -x `pgrep -x main` -o stapResult.txt