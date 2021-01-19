#include "../drivers/keyboard.h"
#include "../drivers/idt.h"
#include "shell.h"

extern void keyboard_handler_int();

void initialize(){
    for (int i = 0; i < IDT_SIZE; i++) {
        load_idt_entry(i, 0, 0, 0);
    }
    idt_init();
    load_idt_entry(0x21, (unsigned long) keyboard_handler_int, 0x08, 0x8e);
    kb_init();
    shell_init();
}

void kmain() {
    initialize();

    while(1) __asm__("hlt\n\t");
}
