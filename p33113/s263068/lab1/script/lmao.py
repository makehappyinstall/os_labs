import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

read = []
write = []
open_calls = []
close = []
cpu_time = []
ms_exec = []
ms = 0

with open("output.txt", 'r') as file:
    lines = file.readlines()
    for line in lines:
        try:
            read_v, write_v, open_v, close_v, cpu_v = line.split('\t')
            read.append(int(read_v))
            write.append(int(write_v))
            open_calls.append(int(open_v))
            close.append(int(close_v))
            cpu_time.append(int(cpu_v))
            ms_exec.append(ms)
            ms += 100
        except ValueError:
            continue

plt.plot(ms_exec, read, label='read')

plt.plot(ms_exec, write, label='write')

plt.plot(ms_exec, open_calls, label='open')

plt.plot(ms_exec, close, label='close')

plt.legend()
plt.ylabel("Sys calls")
plt.xlabel("Time in ms")
plt.yscale('log')
plt.title("System calls through time")

plt.savefig("calls.svg")
