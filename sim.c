#include <stdio.h>
#include <stdlib.h>
#include "processor.h"
#include "debug.h"

void pipeline_fetch(cpu_state *state) {
    struct fetch_state *fetch = &state->fetch;

    // Do nothing if stalling is requested. The fetch stage always stalls
    // alongside the decode stage, which handles injecting a NOP.
    if (fetch->StallF) {
        fetch->StallF = false;
        return;
    }

    // Flush, if requested. This adds a NOP into the decode stage in order
    // to account for mispredicted jumps.
    if (fetch->FlushF) {
        instruction_write_nop(&state->decode.InstD);
        fetch->FlushF = false;
        fetch->PC = fetch->PCBranch;
        return;
    }

    const int pc = fetch->PC;

    // A pipelined processor is not done executing until the last instruction reaches the writeback stage.
    // To facilitate this, we fill the pipeline with NOPs when accessing out-of-bounds instructions not caused
    // by a jump instruction (i.e., jumping out-of-bounds will still result in an error being thrown).
    if (pc > state->instructions_count - 1) {
        // If the last instruction has reached the writeback stage, we should halt the processor. This occurs when
        // four additional instructions have been fetched by the processor.
        if (pc >= state->instructions_count + 3) {
            state->halt = true;
        } else {
            // Otherwise, we keep injecting NOPs.
            instruction_write_nop(&state->decode.InstD);
        }
    }

    // The signal is called "PCPlus4" because in implementation, memory will
    // be byte addressed. However, the instruction memory of this simulator
    // contains instruction structs directly, so it should be incremented
    // by 1.
    const int PCPlus4 = fetch->PC + 1;

    state->decode.InstD = state->instruction_memory[pc];
    state->decode.PCPlus4D = PCPlus4;

    // Update the program counter for the next cycle. If we are jumping to an out-of-bounds address,
    // halt the simulator with an error.
    if (state->decode.PCSrc && (fetch->PCBranch < 0 || fetch->PCBranch > state->instructions_count - 1)) {
        printf("out-of-bounds jump to %d\n", fetch->PCBranch);
        exit(ERROR_ILLEGAL_JUMP);
    }

    fetch->PC = state->decode.PCSrc ? fetch->PCBranch : PCPlus4;
}

void pipeline_decode(cpu_state *state) {
    struct decode_state *decode = &state->decode;
    const struct instruction InstrD = state->decode.InstD;

    // Inject a NOP into the execute stage when requested to stall.
    if (decode->StallD) {
        decode->StallD = false;
        instruction_write_nop(&state->execute.InstE);
        return;
    }

    int Rd1, Rd2;

    // Access register file
    Rd1 = state->register_file[InstrD.rs];
    Rd2 = state->register_file[InstrD.rt];

    // Resolve jump instructions
    bool PCSrcD = false;
    switch (InstrD.op) {
        case BEQZ: PCSrcD = Rd1 == 0; break;
        case BNEZ: PCSrcD = Rd1 != 0; break;
        case J:    PCSrcD = 1; break;
    }

    decode->PCSrc = PCSrcD;
    state->fetch.FlushF = PCSrcD;

    // Again, there should be a shift here, but since instruction memory is not byte-addressed,
    // it is omitted.
    state->fetch.PCBranch = InstrD.imm + decode->PCPlus4D;

    state->execute.InstE = InstrD;
    state->execute.A = Rd1;
    state->execute.B = Rd2;
}

void pipeline_execute(cpu_state *state) {
    struct execute_state *execute = &state->execute;
    const struct instruction InstE = execute->InstE;

    int SrcAE = execute->A;
    int WriteDataE = execute->B;

    // Handle forwarding from the memory and writeback stages for
    // the two operands.
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
        if (instruction_get_register_read_after_write(state->decode.InstD, InstE) != NOT_USED) {
            state->fetch.StallF = true;
            state->decode.StallD = true;
        }
    }

    execute->ForwardAE = NONE;
    execute->ForwardBE = NONE;

    int SrcBE = instruction_has_immediate(InstE) ? execute->InstE.imm : WriteDataE;

    int ALUOut = 0;

    switch (instruction_get_alu_op(InstE)) {
        case PLUS:
            ALUOut = SrcAE + SrcBE;
            break;
        case MINUS:
            ALUOut = SrcAE - SrcBE;
            break;
    }

    state->memory.ALUOut = ALUOut;
    state->memory.WriteData = WriteDataE;
    state->memory.Inst = InstE;
}

void pipeline_memory(cpu_state *state) {
    const struct memory_state *memory = &state->memory;

    const int ALUOutM = memory->ALUOut;
    const struct instruction InstM = memory->Inst;

    if (InstM.op == LW || InstM.op == SW) {
        if (ALUOutM < 0 || ALUOutM >= MAX_WORDS_OF_DATA) {
            printf("Exception: out-of-bounds data memory access at %d\n", ALUOutM);
            exit(ERROR_ILLEGAL_MEM_ACCESS);
        }
    }

    switch (InstM.op) {
        case LW:
            state->writeback.ReadData = state->data_memory[ALUOutM];
            break;
        case SW:
            state->data_memory[ALUOutM] = memory->WriteData;
            break;
    }

    int hazard_register = instruction_get_register_read_after_write(state->execute.InstE, InstM);
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
        if (instruction_get_register_read_after_write(state->decode.InstD, InstM) != NOT_USED) {
            state->fetch.StallF = true;
            state->decode.StallD = true;
        }
    }

    state->writeback.Inst = InstM;
    state->writeback.ALUOut = ALUOutM;
}

void pipeline_writeback(cpu_state *state) {
    struct writeback_state *writeback = &state->writeback;
    const struct instruction InstW = writeback->Inst;

    int *dest = NULL;
    int data = 0;

    if (InstW.op == ADD || InstW.op == SUB)
        dest = &state->register_file[InstW.rd];
    if (InstW.op == ADDI || InstW.op == SUBI || InstW.op == LW)
        dest = &state->register_file[InstW.rt];

    if (InstW.op >= ADDI && InstW.op <= SUB)
        data = writeback->ALUOut;
    if (InstW.op == LW)
        data = writeback->ReadData;

    if (dest != NULL) {
        if (dest == state->register_file + R0) {
            printf("Exception: Attempt to overwrite R0");
            exit(ERROR_ILLEGAL_REG_WRITE);
        }
        *dest = data;
    }

    int hazard_register = instruction_get_register_read_after_write(state->execute.InstE, InstW);
    if (hazard_register != NOT_USED) {
        if (hazard_register == state->execute.InstE.rs)
            state->execute.ForwardAE = WRITEBACK;
        if (hazard_register == state->execute.InstE.rt)
            state->execute.ForwardBE = WRITEBACK;
    }

    writeback->Result = data;
    state->instructions_executed++;
}

void simulate_cycle(cpu_state *state) {
    // Simulate each pipeline stage every cycle.
    // This is done in reverse to ensure that each
    // stage's change to the state of the processor
    // only affects it after the current cycle ends.
    pipeline_writeback(state);
    pipeline_memory(state);
    pipeline_execute(state);
    pipeline_decode(state);
    pipeline_fetch(state);
}


int main(int argc, char **argv) {
    if (argc != 2) {  /* Check command line inputs */
        printf("Usage: sim [program]\n");
        exit(0);
    }

    cpu_state state = {};

    /* assemble input program */
    AssembleSimpleDLX(argv[1], state.instruction_memory, &state.instructions_count);

    /* set initial simulator values */
    state.cycles_executed = 0;       /* simulator cycle count */
    state.instructions_executed = 0; /* simulator instruction count */
    state.register_file[R0] = 0;     /* register R0 is alway zero */

    // Execute the simulator until it is halted
    while (!state.halt) {
        simulate_cycle(&state);  /* simulate one cycle */
        state.cycles_executed++; /* update cycle count */

        /* check if simulator is stuck in an infinite loop */
        if (state.cycles_executed > MAX_CYCLES) {
            printf("\n\n *** Runaway program? (Program halted.) ***\n\n");
            break;
        }
    }

    /* print final register values and simulator statistics */
    printf("Registers:\n");
    print_registers(state.register_file);

    printf("Memory:\n");
    print_memory(state.data_memory);
}
