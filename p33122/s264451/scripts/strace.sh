#!/bin/bash
make lab1.o
strace -e trace=memory ./lab1
