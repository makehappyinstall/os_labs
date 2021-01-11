#!/bin/bash
make main.o
strace -e trace=memory ./main
