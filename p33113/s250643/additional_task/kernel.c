void keyboard_handler_int();
void kb_init(void);
void clear_screen(void);
void print(const char *str);
void idt_init();
void load_idt_entry(int isr_number, unsigned long base, short int selector, unsigned char flags);

void kernel_c() {
    const char *WELCOME_MSG = "Welcome to the club, buddy!\n";
    const char *ECHO = ">>> ";
    idt_init();
    load_idt_entry(0x21, (unsigned long) keyboard_handler_int, 0x08, 0x8e);
    kb_init();
    clear_screen();
    print(WELCOME_MSG);
    print(ECHO);
    set_cursor(get_cursor_pos());
    while(1) __asm__("hlt\n\t");
}
