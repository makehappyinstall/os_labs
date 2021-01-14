CC = gcc
ASM = nasm
LD = ld

all: os-image

run: clean all
	bochs -f b

kernel.o: kernel.c
	$(CC) -fno-pie -m32 -ffreestanding -c $< -o $@

keyb.o: keyb.c
	$(CC) -fno-pie -m32 -ffreestanding -c $< -o $@

idt_init.o: idt_init.c
	$(CC) -fno-pie -m32 -ffreestanding -c $< -o $@

user_utils.o: user_utils.c
	$(CC) -fno-pie -m32 -ffreestanding -c $< -o $@

kernel_entry.o: kernel_entry.asm
	$(ASM) $< -f elf32 -o $@

io_functions.o: io_functions.asm
	$(ASM) $< -f elf32 -o $@

bootstrap.bin: bootstrap.asm
	$(ASM) $< -f bin -o $@

kernel.bin: kernel_entry.o kernel.o keyb.o idt_init.o user_utils.o io_functions.o
	$(LD) -m elf_i386 -o $@ -Ttext 0x1000 $^ --oformat binary

os-image: bootstrap.bin kernel.bin
	cat $^ > $@

clean:
	rm *.bin *.o os-image
