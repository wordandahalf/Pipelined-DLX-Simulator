#ifndef LAB1_DEBUG_H
#define LAB1_DEBUG_H

#include <stdio.h>
#include "processor.h"

void print_registers(int *register_file) {
    for (int i = 0; i < 8; i++) {
        printf("R%-2d: %-10d ", i, register_file[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; i++) {
        printf("R%-2d: %-10d ", i + 8, register_file[i + 8]);
    }
    printf("\n");
}

void print_memory(int *data_memory) {
    for (int i = 0; i < MAX_WORDS_OF_DATA; i += 20) {
        printf("%4d ", i);
        for (int j = 0; j < 20; j++) {
            printf("%-4d ", data_memory[i + j]);
        }
        printf("\n");
    }
}

#endif //LAB1_DEBUG_H
