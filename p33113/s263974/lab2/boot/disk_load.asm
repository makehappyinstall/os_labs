; load dh sector to es:bx from drive dl
disk_load:
	push dx

	mov ah, 0x02	; BIOS read sec func
	mov al, dh	; read dh sector
	mov ch, 0x00	; read cylinder 0
	mov dh, 0x00	; select head 0
	mov cl, 0x02	; start reading from second sector after boot sector

	int 0x13

	jc disk_error

	pop dx
	cmp dh, al
	jne disk_error
	ret

disk_error:
	mov bx, DISK_ERROR_MSG
	call print_string
	jmp $

DISK_ERROR_MSG db "Disk read error(",0
