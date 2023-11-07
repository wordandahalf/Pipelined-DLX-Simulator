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

    // Do nothing if stalling is requested
    if (fetch->StallF) {
        fetch->StallF = false;
        return;
    }

    // Flush if requested
    if (fetch->FlushF) {
        printf("Flush requested!\n");
        flush_instruction(&state->decode.InstD);
        fetch->FlushF = false;
        fetch->PC = fetch->PCBranch;
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

    // Update the program counter for the next cycle.
    // todo: flush if current PC is not equal to the pc branch?
    fetch->PC = state->decode.PCSrc ? fetch->PCBranch : PCPlus4;
}

void simulate_decode(cpu_state *state) {
    struct decode_state *decode = &state->decode;
    const struct instruction InstrD = state->decode.InstD;

    // Inject a NOP into the execute stage when requested to stall.
    if (decode->StallD) {
        decode->StallD = false;
        flush_instruction(&state->execute.InstE);
        return;
    }

    // Stall for a single cycle if requested
    if (decode->StallD) {
        decode->StallD = false;
        return;
    }

    int Rd1, Rd2;

    // Access register file
    Rd1 = int_regs[InstrD.rs];
    Rd2 = int_regs[InstrD.rt];

    // Resolve jump instructions
    bool PCSrcD = false;
    switch (InstrD.op) {
        case BEQZ: PCSrcD = Rd1 == 0; break;
        case BNEZ: PCSrcD = Rd1 != 0; break;
        case J:    PCSrcD = 1; break;
    }

    decode->PCSrc = PCSrcD;
    state->fetch.FlushF = PCSrcD;

    if (PCSrcD) {
        printf("Immediate: %x\n", InstrD.imm);
        printf("Target: %x\n", InstrD.imm + decode->PCPlus4D);
    }

    // Again, there should be a shift here, but since instruction memory, yadda yadda
    state->fetch.PCBranch = InstrD.imm + decode->PCPlus4D;

    state->execute.InstE = InstrD;
    state->execute.A = Rd1;
    state->execute.B = Rd2;
}

void simulate_execute(cpu_state *state) {
    struct execute_state *execute = &state->execute;
    const struct instruction InstE = execute->InstE;

    int WriteDataE;
    int SrcAE, SrcBE;

    if (execute->ForwardAE == NONE)
        SrcAE = execute->A;
    else if(execute->ForwardAE == MEMORY)
        SrcAE = state->memory.ALUOut;
    else if(execute->ForwardAE == WRITEBACK)
        SrcAE = state->writeback.Result;

    if (execute->ForwardBE == NONE)
        WriteDataE = execute->B;
    else if(execute->ForwardBE == MEMORY)
        WriteDataE = state->memory.ALUOut;
    else if(execute->ForwardBE == WRITEBACK)
        WriteDataE = state->writeback.Result;

    // If we are writing to a register read in a jump instruction,
    // tell the fetch and decode stages to stall and inject a NOP here.
    const struct instruction InstD = state->decode.InstD;
    if (InstD.op == BEQZ || InstD.op == BNEZ) {
        if (get_register_read_after_write(state->decode.InstD, InstE) != NOT_USED) {
            state->fetch.StallF = true;
            state->decode.StallD = true;
        }
    }

    if (execute->ForwardAE != NONE)
        printf("Forwarded first operand from %d\n", execute->ForwardAE);

    if (execute->ForwardBE != NONE)
        printf("Forwarded second operand from %d\n", execute->ForwardBE);

    execute->ForwardAE = NONE;
    execute->ForwardBE = NONE;

    if (InstE.op == ADDI || InstE.op == SUBI ||
        InstE.op == LW   || InstE.op == SW) {
        SrcBE = execute->InstE.imm;
    } else {
        SrcBE = WriteDataE;
    }

    int ALUOut;

    switch (InstE.op) {
        case SUBI:
        case SUB:
        case BEQZ:
        case BNEZ:
            ALUOut = SrcAE - SrcBE;
            break;
        case ADDI:
        case ADD:
        case LW:
        case SW:
            ALUOut = SrcAE + SrcBE;
            break;
    }

    state->memory.ALUOut = ALUOut;
    state->memory.WriteData = WriteDataE;
    state->memory.Inst = InstE;
}

void simulate_memory(cpu_state *state) {
    // TODO: implement stall condition
    const struct memory_state *memory = &state->memory;

    const int ALUOutM = memory->ALUOut;
    const struct instruction InstM = memory->Inst;

    if (InstM.op == LW || InstM.op == SW) {
        if (ALUOutM < 0 || ALUOutM >= MAX_WORDS_OF_DATA) {
            printf("Exception: out-of-bounds data memory access at %d\n", ALUOutM);
            exit(0);
        }
    }

    switch (InstM.op) {
        case LW:
            state->writeback.ReadData = data_mem[ALUOutM];
            printf("Read %d from %d\n", data_mem[ALUOutM], ALUOutM);
            break;
        case SW:
            data_mem[ALUOutM] = memory->WriteData;
            printf("Wrote %d to %d\n", memory->WriteData, ALUOutM);
            break;
    }

    int hazard_register = get_register_read_after_write(state->execute.InstE, InstM);
    if (hazard_register != NOT_USED) {
        if (hazard_register == state->execute.InstE.rs)
            state->execute.ForwardAE = MEMORY;
        if (hazard_register == state->execute.InstE.rt)
            state->execute.ForwardBE = MEMORY;
    }

    // If we are writing to a register read in a jump instruction,
    // tell the fetch and decode stages to stall and inject a NOP here.
    const struct instruction InstD = state->decode.InstD;
    if (InstD.op == BEQZ || InstD.op == BNEZ) {
        if (get_register_read_after_write(state->decode.InstD, InstM) != NOT_USED) {
            state->fetch.StallF = true;
            state->decode.StallD = true;
        }
    }

    state->writeback.Inst = InstM;
    state->writeback.ALUOut = ALUOutM;
}

void simulate_writeback(cpu_state *state) {
    struct writeback_state *writeback = &state->writeback;
    const struct instruction InstW = writeback->Inst;

    int *dest = NULL;
    int data = 0;

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

        printf("Writing data!\n");
        *dest = data;
    }

    int hazard_register = get_register_read_after_write(state->execute.InstE, InstW);
    if (hazard_register != NOT_USED) {
        if (hazard_register == state->execute.InstE.rs)
            state->execute.ForwardAE = WRITEBACK;
        if (hazard_register == state->execute.InstE.rt)
            state->execute.ForwardBE = WRITEBACK;
    }

    writeback->Result = data;
    inst_executed++;
}

void simulate_cycle(cpu_state *state) {
    // Simulate each pipeline stage every cycle.
    // This is done in reverse to ensure that each
    // stage's change to the state of the processor
    // only affects it after the current cycle ends.
    simulate_writeback(state);
    dump_cpu_state(*state);

    simulate_memory(state);
    dump_cpu_state(*state);

    simulate_execute(state);
    dump_cpu_state(*state);

    simulate_decode(state);
    dump_cpu_state(*state);

    simulate_fetch(state);
    dump_cpu_state(*state);

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

    // Execute the simulator until the last instruction is fetched
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
