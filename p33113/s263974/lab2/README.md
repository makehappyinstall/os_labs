## Lab #2

* bootstrap.asm - final version of bootloader
* disk_load.asm - load bytes from secondary mem (hard disk) using BIOS int
* gdt.asm - defining Global Descriptor Table (base flat layout) and it's descriptor
* io_functions.asm - keyboard interupts 
* kernel.c - main file with Higher-Level Clang kernel part
* kernel_entry.asm - calling the kernel.c
* keyboard_map.h - keyboard keys mapping (CODE -> CHAR)
* Makefile - compiling and running VM
* print_string.asm - printing ASCII null-terminated string using BIOS
* print_string_pm.asm - printing ASCII null-terminated string using VGA (protected mode)
* switch_to_32.asm - switching from real to protected mode.
