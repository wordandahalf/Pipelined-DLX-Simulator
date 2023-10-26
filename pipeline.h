//
// Created by Ryan Jones on 10/23/23.
//

#ifndef LAB1_PIPELINE_H
#define LAB1_PIPELINE_H

#include <stdbool.h>
#include "globals.h"

typedef enum {
    none, memory, writeback
} forwarding;

// State structures for each of the pipeline stages
// TODO: throw struct defns in cpu_state

typedef struct {
    int PC, PCBranch;
    bool PCSrc, StallF;
} fetch_buffer;

typedef struct {
    int PCPlus4D;
    struct instruction InstD;
    bool StallD;
} decode_buffer;

typedef struct {
    int A, B;
    struct instruction InstE;

    forwarding ForwardAE, ForwardBE;
} execute_buffer;

typedef struct {
    int ALUOut, WriteData;
    struct instruction Inst;
} memory_buffer;

typedef struct {
    int ReadData, ALUOut, Result;
    struct instruction Inst;
} writeback_buffer;

typedef struct {
    fetch_buffer     fetch;
    decode_buffer    decode;
    execute_buffer   execute;
    memory_buffer    memory;
    writeback_buffer writeback;
} cpu_state;

void simulate_fetch(cpu_state *state);
void simulate_decode(cpu_state *state);
void simulate_execute(cpu_state *state);
void simulate_memory(cpu_state *state);
void simulate_writeback(cpu_state *state);

#endif //LAB1_PIPELINE_H
