//
// Created by sdfedorov on 14/01/2021.
//

#ifndef OPERATING_SYSTEMS_ITMO_2020_IDT_H
#define OPERATING_SYSTEMS_ITMO_2020_IDT_H

#define PIC_1_CTRL 0x20
#define PIC_2_CTRL 0xA0
#define PIC_1_DATA 0x21
#define PIC_2_DATA 0xA1
#define IDT_SIZE 256

void load_idt_entry(int isr_number, unsigned long base, short int selector, unsigned char flags);
void idt_init();

#endif //OPERATING_SYSTEMS_ITMO_2020_IDT_H
