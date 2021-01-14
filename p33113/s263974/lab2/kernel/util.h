//
// Created by sdfedorov on 14/01/2021.
//

#ifndef OPERATING_SYSTEMS_ITMO_2020_UTIL_H
#define OPERATING_SYSTEMS_ITMO_2020_UTIL_H

void mem_cpy(char* dest, char* orig, unsigned long size);
void str_cpy(char* dest, char* str);
char str_eq(char* a_str, char* b_str);
char* next_str(char* str);

int str_to_int (char *str);
char* int_to_str(int i);

#endif //OPERATING_SYSTEMS_ITMO_2020_UTIL_H
