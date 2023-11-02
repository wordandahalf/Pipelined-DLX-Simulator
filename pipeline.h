//
// Created by Ryan Jones on 10/23/23.
//

#ifndef LAB1_PIPELINE_H
#define LAB1_PIPELINE_H

#include <stdbool.h>
#include "globals.h"

typedef enum {
    NONE, EXECUTE, MEMORY, WRITEBACK
} forwarding_source;

typedef struct {
    struct fetch_state {
        int PC, PCBranch;
        bool StallF;
    } fetch;

    struct decode_state {
        int PCPlus4D;
        struct instruction InstD;
        bool StallD, PCSrc;
        forwarding_source ForwardAD, ForwardBD;
    } decode;

    struct execute_state {
        int A, B, ALUOut;
        struct instruction InstE;
        forwarding_source ForwardAE, ForwardBE;
    } execute;

    struct memory_state {
        int ALUOut, WriteData;
        struct instruction Inst;
    } memory;

    struct writeback_state {
        int ReadData, ALUOut, Result;
        struct instruction Inst;
    } writeback;
} cpu_state;

void dump_cpu_state(cpu_state state) {
    printf("fetch{PC=%4d, PCBranch=%4d, StallF=%d}, ", state.fetch.PC, state.fetch.PCBranch, state.fetch.StallF);
    printf("decode{InstD=%3d, PCPlus4D=%d, StallD=%d, PCSrc=%d, ForwardAD=%d, ForwardBD=%d}, ", state.decode.InstD.op, state.decode.PCPlus4D, state.decode.StallD, state.decode.PCSrc, state.decode.ForwardAD, state.decode.ForwardBD);
    printf("execute{InstE=%3d, A=%3d, B=%3d, ForwardAE=%d, ForwardBE=%d} ", state.execute.InstE.op, state.execute.A, state.execute.B, state.execute.ForwardAE, state.execute.ForwardBE);
    printf("memory{Inst=%3d, ALUOut=%3d, WriteData=%3d}, ", state.memory.Inst.op, state.memory.ALUOut, state.memory.WriteData);
    printf("writeback{Inst=%3d, ALUOut=%3d, ReadData=%3d, Result=%3d}\n", state.writeback.Inst.op, state.writeback.ALUOut, state.writeback.ReadData, state.writeback.Result);
}

void simulate_fetch(cpu_state *state);
void simulate_decode(cpu_state *state);
void simulate_execute(cpu_state *state);
void simulate_memory(cpu_state *state);
void simulate_writeback(cpu_state *state);

#endif //LAB1_PIPELINE_H
