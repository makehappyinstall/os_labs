//
// Created by sdfedorov on 14/01/2021.
//

#include "../kernel/shell.h"
#include "keyboard.h"
#include "keyboard_map.h"

extern unsigned char read_port(int port);
extern void write_port(unsigned short port, unsigned char data);


void keyboard_handler(void) {
    signed char keycode;

    if (read_port(0x64) & 0x01) {
        keycode = read_port(0x60);
        if (keycode >= 0) {
            receive_char(keyboard_map[keycode]);
        }
    }
    /* Send End of Interrupt (EOI) to master PIC */
    write_port(0x20, 0x20);
}

void kb_init(void) {
    /* This is a very basic keyboard initialization. The assumption is we have a
     * PS/2 keyboard and it is already in a propr state. This may not be the case
     * on real hardware. We simply enable the keyboard interupt */

    /* Get current master PIC interrupt mask */
    unsigned char curmask_master = read_port (0x21);

    /* 0xFD is 11111101 - enables only IRQ1 (keyboard) on master pic
       by clearing bit 1. bit is clear for enabled and bit is set for disabled */
    write_port(0x21, curmask_master & 0xFD);
}