bits 16
switch_to_32:
	cli
	lgdt [gdt_descriptor] ; load global descriptor table

	mov eax, cr0
	or eax, 1
	mov cr0, eax

	jmp CODE_SEG:init_pm

bits 32
init_pm:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	mov ebp, 0x90000
	mov esp, ebp

	call BEGIN_PM
