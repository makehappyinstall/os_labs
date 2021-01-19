//
// Created by sdfedorov on 14/01/2021.
//

#include "screen.h"
#include "../kernel/util.h"

char* vidptr = VIDEO_ADDRESS;
unsigned int current_loc;

void clear_screen(void){
    unsigned int i = 0;
    while (i < SCREEN_SIZE) {
        vidptr[i++] = 0;
        vidptr[i++] = DEFAULT_TEXT_COLOR;
    }
}

void print_newline(void){
    unsigned int line_size = BYTES_PER_CHAR * MAX_COLS;
    current_loc += (line_size - current_loc % (line_size));

    if (current_loc / (BYTES_PER_CHAR * MAX_COLS) >= MAX_ROWS) {
        // Shifting
        mem_cpy(vidptr, vidptr + BYTES_PER_CHAR * MAX_COLS,SCREEN_SIZE);
        current_loc -= BYTES_PER_CHAR * MAX_COLS;
    }
}

void print_char(char val, char color) {
    vidptr[current_loc++] = val;
    vidptr[current_loc++] = color;
}

void print(const char *str, char color){
    unsigned int i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            print_newline();
        } else {
            print_char(str[i], color);
        }
        i++;
    }
}

static char to_ascii_char(char c) {
    if (c >= 0xA && c <= 0xF) {
        return c + 0x37;
    } else if (c >= 0x0 && c <= 0x9){
        return c + 0x30;
    } else {
        return '$';
    }
}

void print_hex(unsigned char hex_val, char color) {
    print_char(to_ascii_char((char) ((hex_val >> 4) % 16)) , color);
    print_char(to_ascii_char((char) (hex_val % 16)), color);
}

void backspace(int times) {
    for (int i = 0; i < BYTES_PER_CHAR * times; i++) {
        vidptr[current_loc--] = 0;
    }
}

void set_cursor(int offset) {
    offset /= 2; // Covert from cell offset to char offset.
    // This is similar to get_cursor, only now we write
    // bytes to those internal device registers.
    write_port(REG_SCREEN_CTRL, 14);
    write_port(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    write_port(REG_SCREEN_CTRL, 15);
    write_port(REG_SCREEN_DATA, offset);
}