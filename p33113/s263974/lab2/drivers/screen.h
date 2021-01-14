//
// Created by sdfedorov on 14/01/2021.
//

#ifndef OPERATING_SYSTEMS_ITMO_2020_SCREEN_H
#define OPERATING_SYSTEMS_ITMO_2020_SCREEN_H

#define REG_SCREEN_CTRL 0x3D4
#define REG_SCREEN_DATA 0x3D5

#define VIDEO_ADDRESS (char*) 0xB8000
#define MAX_COLS 80
#define MAX_ROWS 25
#define BYTES_PER_CHAR 2
#define SCREEN_SIZE MAX_ROWS * MAX_COLS * BYTES_PER_CHAR

#define DEFAULT_TEXT_COLOR 0x07
#define DEFAULT_MSG_COLOR 0x02

#define print_d(A) print(A, DEFAULT_TEXT_COLOR)
#define print_hex_d(A) print_hex(A, DEFAULT_TEXT_COLOR)
#define print_char_d(A) print_char(A, DEFAULT_TEXT_COLOR)

char *vidptr;
unsigned int current_loc;

void clear_screen(void);
void print_newline(void);
void print(const char *str, char color);
void print_hex(unsigned char hex_val, char color);
void print_char(char val, char color);
void backspace(int times);

void set_cursor(int offset);

#endif //OPERATING_SYSTEMS_ITMO_2020_SCREEN_H
