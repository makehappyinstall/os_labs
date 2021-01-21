const char *ECHO_COMMAND = "echo\0";
const char *WRITE_MEMORY_COMMAND = "mw\0";
const char *READ_MEMORY_COMMAND = "mr\0";
const char *HELP_COMMAND = "help\0";
const char *CLEAR_COMMAND = "clear\0";

void print(const char *str);
void clear_screen();

char *base_pointer = 0x00000000;

void print_newline(void);


int get_first_pos(char* str, char ch) {
    int i = 0;
    while (str[i] != ch) {
        i++;
    }
    return i;
}

void buf_cpy(char *from, char *to, int start, int end) {
    int current = start;
    int i = 0;
    while (current < end) {
        to[i++] = from[current++];
    }
    to[i] = '\0';
}

int compare_str(const char *str1, const char *str2) {
    int i = 0;
    while (str1[i] != '\0') {
        if (str1[i] != str2[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

unsigned int str_to_int (char *str){
    unsigned int temp = 0;
    int i = 0;
    while (str[i] >= 0x30 && str[i] <= 0x39) {
        temp = temp + (str[i] & 0x0F);
        temp = temp * 10;
        i++;
    }
    temp = temp / 10;
    return (temp);
}

char* int_to_str(int i) {
    static char buf[255];
    char *p = buf + 255;

    int sign;

    if ((sign = i) < 0) 
        i = -i; 

    do {
        *--p = '0' + (i % 10);
    } while ((i /= 10) > 0);

    if (sign < 0)
        *--p = '-';

    return p;
}


void echo_cmd(char *buf_ptr) {
    print(buf_ptr);
    print_newline();
}

char* write_to_mem(char* ptr, char *str) {
    unsigned int address = str_to_int(ptr);
    unsigned int value = str_to_int(str);
    base_pointer[address] = value;
    unsigned int result = base_pointer[address];
    return int_to_str(result);
}

char* read_from_mem(char* ptr){
    unsigned int i = 0;
    unsigned int address = str_to_int(ptr);
    unsigned int result = base_pointer[address];
    return int_to_str(result);
}

void help_cmd(){
    char *msg = "echo: format: echo msg \n   mw: format: mw adress msg \n   mr: format: mr adress \n   help: \n";
    print(msg);
}

void handle_command(char *buf_ptr) {
    char *command[32];
    buf_cpy(buf_ptr, command, 0, 5);
    int i = 0;
    int end = get_first_pos(buf_ptr, '\0');
    if (compare_str(ECHO_COMMAND, command)) {
        char arg[256];
        buf_cpy(buf_ptr, arg, 5, end);
        echo_cmd(arg);
    } else if (compare_str(WRITE_MEMORY_COMMAND, command)) {
        char arg[256];
        buf_cpy(buf_ptr, arg, 5, end);
        int delimiter = get_first_pos(arg, ' ');
        char arg1[32], arg2[32];
        buf_cpy(arg, arg1, 0, delimiter);
        buf_cpy(arg, arg2, delimiter + 1, end);
        char *result = write_to_mem(arg1, arg2);
        echo_cmd(result);
    } else if (compare_str(READ_MEMORY_COMMAND, command)) {
        char arg[32];
        buf_cpy(buf_ptr, arg, 5, end);
        char *result = read_from_mem(arg);
        echo_cmd(result);
    } else if (compare_str(HELP_COMMAND, command)) {
        help_cmd();
    } else if (compare_str(CLEAR_COMMAND, command)) {
        clear_screen();
    }
     else {
        print("Command not found\n");
    }
}