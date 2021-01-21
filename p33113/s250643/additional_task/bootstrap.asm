bits 16
org 0x7C00

KERNEL_OFFSET equ 0x1000 ; offset to reach main func in kernel.c
boot:
	mov [BOOT_DRIVE], dl ; store boot drive in DL

	mov bp, 0x9000
	mov sp, bp

	call load_kernel

	call switch_to_32

	jmp $

%include "print_string.asm"
%include "disk_load.asm"
%include "gdt.asm"
%include "switch_to_32.asm"

bits 16

load_kernel:
	mov bx, KERNEL_OFFSET ; destination
	mov dh, 32 ; 2 sectors of drive
	mov dl, [BOOT_DRIVE]
	call disk_load

	ret

bits 32

BEGIN_PM:
	call KERNEL_OFFSET

	jmp $

BOOT_DRIVE	db 0

times 510-($-$$) db 0
dw 0xAA55
