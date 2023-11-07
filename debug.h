//
// Created by ryan on 11/7/23.
//

#ifndef LAB1_DEBUG_H
#define LAB1_DEBUG_H

#include <stdio.h>
#include "processor.h"

void print_registers(cpu_state *state) {
    for (int i = 0; i < 8; i++) {
        printf("R%-2d: %-10d ", i, state->register_file[i]);
    }
    printf("\n");
    for (int i = 0; i < 8; i++) {
        printf("R%-2d: %-10d ", i + 8, state->register_file[i + 8]);
    }
    printf("\n");
}

void print_memory(cpu_state *state) {
    for (int i = 0; i < MAX_WORDS_OF_DATA; i += 10) {
        printf("%4d ", i);
        for (int j = 0; j < 20; j++) {
            printf("%-4d ", state->data_memory[j]);
        }
        printf("\n");
    }
}

#endif //LAB1_DEBUG_H
