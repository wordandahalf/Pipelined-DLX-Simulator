#include <stdio.h>
#include <stdlib.h>
#include "globals.h"
#include "pipeline.h"

// Max cycles simulator will execute -- to stop a runaway simulator
#define FAIL_SAFE_LIMIT  500000

struct instruction inst_mem[MAX_LINES_OF_CODE];  // instruction memory
int data_mem[MAX_WORDS_OF_DATA];             // data memory
int int_regs[16];                            // integer register file
int code_length;                             // lines of code in inst mem
int cycle;                                   // simulation cycle count
int inst_executed;                          // number of instr executed

void simulate_fetch(cpu_state *state) {
    fetch_buffer *buffer = &state->fetch;

    // Stall for a single cycle if requested
    if (buffer->StallF) {
        buffer->StallF = false;
        return;
    }

    const int pc = buffer->PC;

    // Validate the value of PC
    if (pc < 0 || pc >= code_length) {
        printf("out-of-bounds instruction memory access at %d\n", pc);
        exit(-1);
    }

    // The signal is called "PCPlus4" because in implementation, memory will
    // be byte addressed. However, the instruction memory of this simulator
    // contains instruction structs directly, so it should be incremented
    // by 1.
    const int PCPlus4 = buffer->PC + 1;

    state->decode.InstD = inst_mem[pc];
    state->decode.PCPlus4D = PCPlus4;

    // Update the program counter for the next cycle
    buffer->PC = buffer->PCSrc ? buffer->PCBranch : PCPlus4;
}

void simulate_decode(cpu_state *state) {
    decode_buffer *buffer = &state->decode;

    // Stall for a single cycle if requested
    if (buffer->StallD) {
        buffer->StallD = false;
        return;
    }

    // Access the instruction memory
    const struct instruction InstrD = state->decode.InstD;
    const int Rd1 = buffer->InstD.rs,
              Rd2 = buffer->InstD.rt;

    // Resolve jump instructions
    bool PCSrcD;

    switch (InstrD.op) {
        case BEQZ: PCSrcD = Rd1 == 0; break;
        case BNEZ: PCSrcD = Rd1 != 0; break;
        case J:    PCSrcD = 1; break;
    }

    //
    state->fetch.PCSrc = PCSrcD;
    state->fetch.PCBranch = (InstrD.imm << 2) + buffer->PCPlus4D;

    state->execute.InstE = InstrD;
    state->execute.A = Rd1;
    state->execute.B = Rd2;
}

void simulate_execute(cpu_state *state) {
    const execute_buffer *buffer = &state->execute;
    const struct instruction InstE = buffer->InstE;

    int WriteDataE;
    int SrcAE, SrcBE;

    switch (buffer->ForwardAE) {
        case none:
            SrcAE = buffer->A;
        case memory:
            SrcAE = state->memory.ALUOut;
            break;
        case writeback:
            SrcAE = state->writeback.Result;
            break;
    }

    switch (buffer->ForwardAE) {
        case none:
            WriteDataE = buffer->B;
        case memory:
            WriteDataE = state->memory.ALUOut;
            break;
        case writeback:
            WriteDataE = state->writeback.Result;
            break;
    }

    if (InstE.op == ADDI || InstE.op == SUBI ||
        InstE.op == LW   || InstE.op == SW) {
        SrcBE = buffer->InstE.imm;
    } else {
        SrcBE = WriteDataE;
    }

    if (InstE.op == SUB || InstE.op == SUBI)
        state->memory.ALUOut = SrcAE - SrcBE;
    else if(InstE.op == ADD || InstE.op == ADDI)
        state->memory.ALUOut = SrcAE + SrcBE;

    state->memory.WriteData = WriteDataE;
    state->memory.Inst = InstE;
}

void simulate_memory(cpu_state *state) {
    // TODO: implement stall condition
    const memory_buffer *buffer = &state->memory;

    const int ALUOutM = buffer->ALUOut;
    const struct instruction InstM = buffer->Inst;

    if (InstM.op == LW || InstM.op == SW) {
        if (ALUOutM < 0 || ALUOutM >= MAX_WORDS_OF_DATA) {
            printf("Exception: out-of-bounds data memory access at %d\n", ALUOutM);
            exit(0);
        }
    }

    switch (InstM.op) {
        case LW:
            state->writeback.ReadData = data_mem[ALUOutM];
            break;
        case SW:
            data_mem[ALUOutM] = buffer->WriteData;
            break;
    }

    state->writeback.Inst = InstM;
    state->writeback.ALUOut = buffer->ALUOut;
}

void simulate_writeback(cpu_state *state) {
    writeback_buffer *buffer = &state->writeback;
    const struct instruction InstW = buffer->Inst;

    int *dest = NULL;
    int data;

    if (InstW.op == ADD || InstW.op == SUB)
        dest = &int_regs[InstW.rd];
    if (InstW.op == ADDI || InstW.op == SUBI || InstW.op == LW)
        dest = &int_regs[InstW.rt];

    if (InstW.op >= ADD && InstW.op <= SUB)
        data = buffer->ALUOut;
    if (InstW.op == LW)
        data = buffer->ReadData;

    if (dest != NULL) {
        if (dest == int_regs + R0) {
            printf("Exception: Attempt to overwrite R0");
            exit(0);
        }

        *dest = data;
    }

    buffer->Result = data;
    inst_executed++;
}

void simulate_cycle(cpu_state *state) {
    // Simulate each pipeline stage every cycle.
    // This is done in reverse to ensure that each
    // stage's change to the state of the processor
    // only affects it after the current cycle ends.
    simulate_writeback(state);
    simulate_memory(state);
    simulate_execute(state);
    simulate_decode(state);
    simulate_fetch(state);
}


int main(int argc, char **argv) {
    int i;

    if (argc != 2) {  /* Check command line inputs */
        printf("Usage: sim [program]\n");
        exit(0);
    }

    /* assemble input program */
    AssembleSimpleDLX(argv[1], inst_mem, &code_length);

    /* set initial simulator values */
    cycle = 0;                /* simulator cycle count */
    int_regs[R0] = 0;         /* register R0 is alway zero */
    inst_executed = 0;

    cpu_state state = {};

    /* Main simulator loop */
    while (state.fetch.PC != code_length) {
        simulate_cycle(&state);      /* simulate one cycle */
        cycle += 1;                  /* update cycle count */

        /* check if simulator is stuck in an infinite loop */
        if (cycle > FAIL_SAFE_LIMIT) {
            printf("\n\n *** Runaway program? (Program halted.) ***\n\n");
            break;
        }
    }

    /* print final register values and simulator statistics */
    printf("Final register file values:\n");
    for (i = 0; i < 16; i += 4) {
        printf("  R%-2d: %-10d  R%-2d: %-10d", i, int_regs[i], i + 1, int_regs[i + 1]);
        printf("  R%-2d: %-10d  R%-2d: %-10d\n", i + 2, int_regs[i + 2], i + 3, int_regs[i + 3]);
    }
}
