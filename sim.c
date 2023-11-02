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
    struct fetch_state *fetch = &state->fetch;

    // Stall for a single cycle if requested
    if (fetch->StallF) {
        fetch->StallF = false;
        return;
    }

    const int pc = fetch->PC;

    // Validate the value of PC
    if (pc < 0 || pc >= code_length) {
        printf("out-of-bounds instruction memory access at %d\n", pc);
        exit(-1);
    }

    // The signal is called "PCPlus4" because in implementation, memory will
    // be byte addressed. However, the instruction memory of this simulator
    // contains instruction structs directly, so it should be incremented
    // by 1.
    const int PCPlus4 = fetch->PC + 1;

    state->decode.InstD = inst_mem[pc];
    state->decode.PCPlus4D = PCPlus4;

    // Update the program counter for the next cycle
    fetch->PC = state->decode.PCSrc ? fetch->PCBranch : PCPlus4;
}

void simulate_decode(cpu_state *state) {
    struct decode_state *decode = &state->decode;
    const struct instruction InstrD = state->decode.InstD;

    // If we are "empty", return
    if (InstrD.op == INVALID)
        return;

    // Stall for a single cycle if requested
    if (decode->StallD) {
        decode->StallD = false;
        return;
    }

    // TODO: Flush if PCSrc is high
    int Rd1, Rd2;

    if (decode->ForwardAD == NONE)
        Rd1 = decode->InstD.rs;
    else if(decode->ForwardAD == EXECUTE)
        Rd1 = state->execute.ALUOut;
        // TODO: definitely should be ReadDataM
    else if(decode->ForwardAD == MEMORY)
        Rd1 = state->memory.ALUOut;
    else if(decode->ForwardAD == WRITEBACK)
        Rd1 = state->writeback.Result;

    if (decode->ForwardBD == NONE)
        Rd2 = decode->InstD.rs;
    else if(decode->ForwardBD == EXECUTE)
        Rd2 = state->execute.ALUOut;
        // TODO: definitely should be ReadDataM
    else if(decode->ForwardBD == MEMORY)
        Rd2 = state->memory.ALUOut;
    else if(decode->ForwardBD == WRITEBACK)
        Rd2 = state->writeback.Result;

    decode->ForwardAD = NONE;
    decode->ForwardBD = NONE;

    // Resolve jump instructions
    bool PCSrcD;

    switch (InstrD.op) {
        case BEQZ: PCSrcD = Rd1 == 0; break;
        case BNEZ: PCSrcD = Rd1 != 0; break;
        case J:    PCSrcD = 1; break;
    }

    decode->PCSrc = PCSrcD;
    state->fetch.PCBranch = (InstrD.imm << 2) + decode->PCPlus4D;

    state->execute.InstE = InstrD;
    state->execute.A = Rd1;
    state->execute.B = Rd2;
}

void simulate_execute(cpu_state *state) {
    struct execute_state *execute = &state->execute;
    const struct instruction InstE = execute->InstE;

    // If we are "empty", return
    if (InstE.op == INVALID)
        return;

    int WriteDataE;
    int SrcAE, SrcBE;

    if (execute->ForwardAE == NONE)
        SrcAE = execute->A;
    else if(execute->ForwardAE == MEMORY)
        // TODO: definitely should be ReadDataM
        SrcAE = state->memory.ALUOut;
    else if(execute->ForwardAE == WRITEBACK)
        SrcAE = state->writeback.Result;

    if (execute->ForwardBE == NONE)
        WriteDataE = execute->A;
    else if(execute->ForwardBE == MEMORY)
        // TODO: definitely should be ReadDataM
        WriteDataE = state->memory.ALUOut;
    else if(execute->ForwardBE == WRITEBACK)
        WriteDataE = state->writeback.Result;

    execute->ForwardAE = NONE;
    execute->ForwardBE = NONE;

    if (InstE.op == ADDI || InstE.op == SUBI ||
        InstE.op == LW   || InstE.op == SW) {
        SrcBE = execute->InstE.imm;
    } else {
        SrcBE = WriteDataE;
    }

    int ALUOut;

    if (InstE.op == SUB || InstE.op == SUBI)
        ALUOut = SrcAE - SrcBE;
    else if(InstE.op == ADD || InstE.op == ADDI)
        ALUOut = SrcAE + SrcBE;

    if (InstE.rd != NOT_USED && InstE.rd == state->decode.InstD.rs)
        state->decode.ForwardAD = EXECUTE;
    if (InstE.rd != NOT_USED && InstE.rd == state->decode.InstD.rt)
        state->decode.ForwardBD = EXECUTE;

    state->memory.ALUOut = ALUOut;
    state->memory.WriteData = WriteDataE;
    state->memory.Inst = InstE;
}

void simulate_memory(cpu_state *state) {
    // TODO: implement stall condition
    const struct memory_state *memory = &state->memory;

    const int ALUOutM = memory->ALUOut;
    const struct instruction InstM = memory->Inst;

    // If we are "empty", return
    if (InstM.op == INVALID)
        return;

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
            data_mem[ALUOutM] = memory->WriteData;
            break;
    }

    // If we are writing to either operand of the instruction
    // in the decode or execute stage, we need to forward!
    if (InstM.op == SW) {
        if (InstM.rd != NOT_USED && InstM.rd == state->decode.InstD.rs)
            state->decode.ForwardAD = MEMORY;
        if (InstM.rd != NOT_USED && InstM.rd == state->decode.InstD.rt)
            state->decode.ForwardBD = MEMORY;
        if (InstM.rd != NOT_USED && InstM.rd == state->execute.InstE.rs)
            state->execute.ForwardAE = MEMORY;
        if (InstM.rd != NOT_USED && InstM.rd == state->execute.InstE.rt)
            state->execute.ForwardBE = MEMORY;
    }

    state->writeback.Inst = InstM;
    state->writeback.ALUOut = ALUOutM;
}

void simulate_writeback(cpu_state *state) {
    struct writeback_state *writeback = &state->writeback;
    const struct instruction InstW = writeback->Inst;

    // If we are "empty", return
    if (InstW.op == INVALID)
        return;

    int *dest = NULL;
    int data;

    if (InstW.op == ADD || InstW.op == SUB)
        dest = &int_regs[InstW.rd];
    if (InstW.op == ADDI || InstW.op == SUBI || InstW.op == LW)
        dest = &int_regs[InstW.rt];

    if (InstW.op >= ADDI && InstW.op <= SUB)
        data = writeback->ALUOut;
    if (InstW.op == LW)
        data = writeback->ReadData;

    if (dest != NULL) {
        if (dest == int_regs + R0) {
            printf("Exception: Attempt to overwrite R0");
            exit(0);
        }

        *dest = data;
    }

    // If we are writing to either operand of the instruction
    // in the decode or execute stage, we need to forward!
    // TODO: broken because you are a dumb moron and forgot addi and subi write to rt, not rd.
    if (InstW.rd != NOT_USED && InstW.rd == state->decode.InstD.rs)
        state->decode.ForwardAD = WRITEBACK;
    if (InstW.rd != NOT_USED && InstW.rd == state->decode.InstD.rt)
        state->decode.ForwardBD = WRITEBACK;
    if (InstW.rd != NOT_USED && InstW.rd == state->execute.InstE.rs)
        state->execute.ForwardAE = WRITEBACK;
    if (InstW.rd != NOT_USED && InstW.rd == state->execute.InstE.rt)
        state->execute.ForwardBE = WRITEBACK;

    writeback->Result = data;
    inst_executed++;
}

void simulate_cycle(cpu_state *state) {
    dump_cpu_state(*state);

    // Simulate each pipeline stage every cycle.
    // This is done in reverse to ensure that each
    // stage's change to the state of the processor
    // only affects it after the current cycle ends.
    simulate_writeback(state);
    simulate_memory(state);
    simulate_execute(state);
    simulate_decode(state);
    simulate_fetch(state);

    for (int i = 0; i < 16; i++)
        printf("%3d ", int_regs[i]);
    printf("\n");
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
