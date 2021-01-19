//
// Created by sdfedorov on 14/01/2021.
//

#include "shell.h"
#include "util.h"
#include "base.h"
#include "../drivers/screen.h"

#define MSG_LEN 9
#define MAX_ARGS 5
#define MAX_LEN MAX_COLS - MSG_LEN
#define COMMANDS_COUNT 9

static struct command cmds[COMMANDS_COUNT] = {
    (struct command) { 0, 1, "echo" },
    (struct command) { 1, 1, "yes" },
    (struct command) { 2, 2, "catm" },
    (struct command) { 3, 2, "memdump" },
    (struct command) { 4, 2, "wmem" },
    (struct command) { 5, 3, "add" },
    (struct command) { 6, 0, "clear" },
    (struct command) { 7, 0, "exit" },
    (struct command) { 8, 0, "help" },
};

static char cli_msg[MSG_LEN] = "~console>";
static char cur_row[MAX_LEN];
static int cur_row_pos;
char* cmd_args[MAX_ARGS];

static void flush_the_row() {
    for (int i = 0; i < MAX_LEN; i++) cur_row[i] = 0;
    cur_row_pos = 0;
}

// Commands

void noop() {
    print_d("\nnoop - use 'help' in case you are lost.");
}

void echo() {
    print_newline();
    print_d(cmd_args[1]);
}

void yes() {
    int yes_rep = MAX_ROWS - 1;
    if (cmd_args[1] == NULL) {
        for (int i = 0; i<yes_rep; i++) print_d("\nyes");
    } else {
        for (int i = 0; i<yes_rep; i++) {
            print_newline();
            print_d(cmd_args[1]);
        }
    }
}

void catm() {
    int size = str_to_int(cmd_args[2]);
    char* addr = (char*) str_to_int(cmd_args[1]);

    int c;
    for(c = 0; c < size; c++) {
        if (c % (MAX_COLS-5) == 0) print_newline();
        print_char_d(addr[c]);
    }
}

void memdump() {
    int size = str_to_int(cmd_args[2]);
    char* addr = (char*) str_to_int(cmd_args[1]);

    int c;
    for(c = 0; c < size; c++) {
        if (c % (MAX_COLS / 5) == 0) print_newline();
        print_hex(addr[c], 0x04);
        print_char_d(' ');
    }
}

void wmem() {
    char* data = cmd_args[2];
    char* addr = (char*) str_to_int(cmd_args[1]);

    int size = 0;
    for (int i = 0;;i++) if (data[i] != '\0') size++; else break;

    int c;
    for(c = 0; c < size; c++) *(addr + c) = data[c];

    print_d("\n: Written ");
    print_d(int_to_str(size));
    print_d(" bytes -> ");
    print_d(cmd_args[1]);
}

void add() {
    char* addr_a = (char*) str_to_int(cmd_args[1]);
    char* addr_b = (char*) str_to_int(cmd_args[2]);

    char result = *addr_a + *addr_b;

    if (cmd_args[3] != NULL) {
        char* res_addr = (char*) str_to_int(cmd_args[3]);
        *(res_addr) = result;
    }

    print_d("RESULT: ");
    print_hex_d(result);
}

void clear() {
    clear_screen();
    current_loc = 0;
}

void exit_f() {
    print_d("\nIMAGE THAT THIS THING IS IN SHUTDOWN process!");
}

void help() {
    char* help_list[COMMANDS_COUNT + 2] = {
            "\nHELP:",
            "\necho (data) - outputs provided data",
            "\nyes [data] - fills the whole screen area with provided data",
            "\ncatm (addr) (size) - outputs mem region as symbols",
            "\nmemdump (addr) (size) - outputs mem region as HEX-vals",
            "\nwmem (addr) (data) - write data to the mem region",
            "\nadd (addr_1) (addr_2) [res_addr] - sums two bytes of data",
            "\nclear - flushes the screen data",
            "\nexit - shutdowns the machine",
            "\nhelp - show help",
            "\n(addr) in DECIMAL, (size) in DECIMAL, (data) in CHARACTERS"
    };

    for (int i = 0; i < COMMANDS_COUNT + 2; i++)
        print_d(help_list[i]);
}

void execute_command() {
    for (int i = 0; i < MAX_LEN; i++) {
        if (cur_row[i] == ' ') cur_row[i] = '\0';
    }

    int cmd_num = -1;
    for (int i = 0; i < COMMANDS_COUNT; i++) {
        if (str_eq(cmds[i].spell, cur_row)) {
            cmd_num = cmds[i].cmd_id;
            break;
        }
    }

    if (cmd_num == -1) {
        noop();
    } else {
        char* arg_jump = cur_row;
        for (int i = 0; i < MAX_ARGS; i++) cmd_args[i] = NULL;
        for (int i = 0; i <= cmds[cmd_num].arg_num; i++) {
            cmd_args[i] = arg_jump;
            arg_jump = next_str(arg_jump);
        }

        switch(cmd_num) {
            case 0: { echo(); break; }
            case 1: { yes(); break; }
            case 2: { catm(); break; }
            case 3: { memdump(); break; }
            case 4: { wmem(); break; }
            case 5: { add(); break; }
            case 6: { clear(); break; }
            case 7: { exit_f(); break; }
            case 8: { help(); break; }
        }
    }
}

void shell_init() {
    clear_screen();
    flush_the_row();
    print(cli_msg, DEFAULT_MSG_COLOR);
    set_cursor(current_loc);
}

void receive_char(char new_char) {
    switch (new_char) {
        case '\n': {
            execute_command();
            flush_the_row();
            print_newline();
            print(cli_msg, DEFAULT_MSG_COLOR);
            set_cursor(current_loc);
            break;
        };
        case 0x08: {
            if (cur_row_pos > 0) {
                set_cursor(current_loc);
                backspace(1);
                cur_row[cur_row_pos--] = 0;
            }
            break;
        }
        default: {
            if (cur_row_pos < MAX_LEN){
                cur_row[cur_row_pos++] = new_char;
                print_char_d(new_char);
                set_cursor(current_loc);
            }
            break;
        }
    }
}
