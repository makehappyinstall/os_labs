//
// Created by sdfedorov on 24/11/2020.
//

#include "util.h"
#include "stdio.h"

void wait_for_input(const char* msg) {
    puts(msg);
    getchar();
}

long min(long a, long b) {
    return (a < b) ? a :  b;
}
