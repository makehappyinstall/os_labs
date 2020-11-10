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

red_patch = mpatches.Patch(color='red', label='The red data')
plt.legend(handles=[red_patch])

plt.plot(ms_exec, read)
plt.ylabel("Sys calls")
plt.xlabel("Time in ms")
plt.title("System calls through time")

plt.plot(ms_exec, write)

plt.plot(ms_exec, open_calls)

plt.plot(ms_exec, close)
plt.savefig("calls.svg")
