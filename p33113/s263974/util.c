//
// Created by sdfedorov on 24/11/2020.
//

#include "util.h"
#include "stdio.h"

void wait_for_input(const char* msg) {
#ifdef INTERACTIVE
    puts(msg);
    getchar();
#endif
}

long min(long a, long b) {
    return (a < b) ? a :  b;
}
