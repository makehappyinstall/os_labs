#include "keyboard_map.h"

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES
#define VIDEO_START 0xb8000

#define REG_SCREEN_CTRL 0x3D4
#define REG_SCREEN_DATA 0x3D5

static int buf_pointer = 0;
static char input_buffer[256];

static int current_loc = 0;

static char *const vidptr = (char *) 0xb8000;

void handle_command(char* buf_ptr);

void set_cursor(int offset) {
    offset /= 2; 

    write_port(REG_SCREEN_CTRL, 14);
    write_port(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    write_port(REG_SCREEN_CTRL, 15);
    write_port(REG_SCREEN_DATA, offset);
}

int get_cursor_pos() {
    return current_loc;
}

void kb_init(void) {
    unsigned char curmask_master = read_port(0x21);

    write_port(0x21, curmask_master & 0xFD);
}

void clear_screen(void) {
    unsigned int i = 0;
    while (i < SCREENSIZE) {
        vidptr[i++] = ' ';
        vidptr[i++] = 0x02;
    }
}

void print_newline(void) {
    unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
    current_loc = current_loc + (line_size - current_loc % (line_size));
}


void print(const char *str) {
    unsigned int i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            print_newline();
            i++;
        } else {
            vidptr[current_loc++] = str[i++];
            vidptr[current_loc++] = 0x02;
        }
    }
}

void keyboard_handler(void) {
    const char *kcmd = ">>> ";
    signed char keycode;

    keycode = read_port(0x60);
    if (keycode >= 0 && keyboard_map[keycode]) {
        switch (keyboard_map[keycode]) {
            case '\n':
                input_buffer[buf_pointer] = '\0';
                buf_pointer = 0;
                print_newline();
                handle_command(input_buffer);
                print(kcmd);
                break;
            case '\b':
                if (current_loc % COLUMNS_IN_LINE > 6 * 2) {
                    vidptr[--current_loc] = 0x00;
                    vidptr[--current_loc] = 0x00;
                    buf_pointer--;
                }
                break;
            default:
                input_buffer[buf_pointer++] = keyboard_map[keycode];
                vidptr[current_loc++] = keyboard_map[keycode];
                vidptr[current_loc++] = 0x02;
        }
    }

    if (current_loc > SCREENSIZE) {
        clear_screen();
        current_loc = 0;
        print(kcmd);
    }

    set_cursor(current_loc);

    write_port(0x20, 0x20);
}