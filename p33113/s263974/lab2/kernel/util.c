//
// Created by sdfedorov on 14/01/2021.
//

// TODO
#include "../drivers/screen.h"
#include "util.h"
#include "base.h"

void mem_cpy(char* dest, char* orig, unsigned long size) {
    unsigned long iter;
    for (iter = 0; iter < size; iter++) dest[iter] = orig[iter];
}

void str_cpy(char* dest, char* str) {
    unsigned long iter = 0;
    while (str[iter] != '\0') {
        dest[iter] = str[iter];
        iter++;
    }
}

char str_eq(char* a_str, char* b_str) {
    char equal = 1;
    unsigned long i = 0;
    do {
        if (a_str[i] != b_str[i]) {
            equal = 0;
            break;
        }
        i++;
    } while (a_str[i] != '\0' || b_str[i] != '\0');
    return equal;
}

#define MAX_SEARCH_ITER 80

char* next_str(char* str) {
    int iter;
    for (iter = 0; iter < MAX_SEARCH_ITER; iter++) {
        if (str[iter] == '\0') {
            if (str[iter + 1] == '\0') {
                return NULL;
            } else return (str + iter + 1);
        }
    }
    return NULL;
}

int str_to_int (char *str){
    int temp = 0;
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

    if ((sign = i) < 0) /* записываем знак */
        i = -i; /* делаем i положительным числом */

    do {
        *--p = '0' + (i % 10);
    } while ((i /= 10) > 0);

    if (sign < 0)
        *--p = '-';

    return p;
}