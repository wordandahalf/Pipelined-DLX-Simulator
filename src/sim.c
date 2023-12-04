#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processor.h"
#include "debug.h"

void pipeline_fetch(cpu_state *state) {
    struct fetch_buffer *fetch = &state->fetch_buffer;

    // Do nothing if stalling is requested. The fetch stage always stalls
    // alongside the decode_buffer stage, which handles injecting a NOP into the execute stage.
    if (fetch->StallF) {
//        printf("(IF) next: %d\n", fetch->PC + 1);
        fetch->StallF = false;
        return;
    }

    // Flush if requested. This adds a NOP into the decode stage in order
    // to account for mispredicted jumps.
    if (fetch->FlushF) {
        state->decode_buffer.instruction = nop;
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
            state->decode_buffer.instruction = nop;
        }
    }

    // The signal is called "PCPlus4" because in implementation memory will
    // be byte addressed. However, the instruction memory of this simulator
    // contains instruction structs directly, so it should be incremented
    // by 1.
    const int PCPlus4 = fetch->PC + 1;

//    printf("(IF) next: %d\n", PCPlus4);

    // increment PC once before stall, then do not continue to increment

    state->decode_buffer.instruction = state->instruction_memory[pc];
    state->decode_buffer.PCPlus4D = PCPlus4;

    // Update the program counter for the next cycle. If we are jumping to an out-of-bounds address,
    // halt the simulator with an error.
    if (state->decode_buffer.PCSrc && (fetch->PCBranch < 0 || fetch->PCBranch > state->instructions_count - 1)) {
        printf("out-of-bounds jump to %d\n", fetch->PCBranch);
        exit(ERROR_ILLEGAL_JUMP);
    }

    fetch->PC = state->decode_buffer.PCSrc ? fetch->PCBranch : PCPlus4;
}

void pipeline_decode(cpu_state *state) {
    struct decode_buffer *decode = &state->decode_buffer;
    const struct instruction InstrD = state->decode_buffer.instruction;

    // Inject a NOP into the execute stage when requested to stall. Instruct the fetch stage to stall.
    if (decode->StallD) {
        decode->StallD = false;

        state->fetch_buffer.StallF = true;

//        printf("(ID) Stalled IF\n");

        state->execute_buffer.instruction = nop;
        return;
    }

    int Rd1, Rd2;

//    printf("(ID) %d %d %d %d\n", InstrD.op, InstrD.rs, InstrD.rt, InstrD.rd);

    // Access register file
    Rd1 = decode->Forward ? decode->data : state->register_file[InstrD.rs];
//    if (decode->Forward)
//        if (InstrD.op == BEQZ || InstrD.op == BNEZ)
//            printf("(ID) Got %d\n", decode->data);

    Rd2 = state->register_file[InstrD.rt];
    decode->Forward = false;

    // Resolve jump instructions
    bool PCSrcD = false;
    switch (InstrD.op) {
        case BEQZ:
//            printf("(ID) BEQZ: R%d (%d) == 0?\n", InstrD.rs, Rd1);
            PCSrcD = Rd1 == 0;
            break;
        case BNEZ:
//            printf("(ID) BNEZ: R%d (%d) != 0?\n", InstrD.rs, Rd1);
            PCSrcD = Rd1 != 0;
            break;
        case J:
            PCSrcD = 1;
            break;
    }

    decode->PCSrc = PCSrcD;
    state->fetch_buffer.FlushF = PCSrcD;

    // Again, there should be a shift here, but since instruction memory is not byte-addressed,
    // it is omitted.
    state->fetch_buffer.PCBranch = InstrD.imm + decode->PCPlus4D;

    state->execute_buffer.instruction = InstrD;
    state->execute_buffer.A = Rd1;
    state->execute_buffer.B = Rd2;
}

void pipeline_execute(cpu_state *state) {
    struct execute_buffer *execute = &state->execute_buffer;
    const struct instruction InstE = execute->instruction;

    int SrcAE = execute->A;
    int WriteDataE = execute->B;

    // Handle forwarding from the memory and writeback stages for
    // the two operands.
    if(execute->ForwardAE == MEMORY)
        SrcAE = state->memory_buffer.instruction.op == LW ? state->writeback_buffer.ReadData : state->memory_buffer.ALUOut;
    else if(execute->ForwardAE == WRITEBACK)
        SrcAE = state->writeback_buffer.Result;

    if(execute->ForwardBE == MEMORY)
        WriteDataE = state->memory_buffer.instruction.op == LW ? state->writeback_buffer.ReadData : state->memory_buffer.ALUOut;
    else if(execute->ForwardBE == WRITEBACK)
        WriteDataE = state->writeback_buffer.Result;

    // If we are executing a LW instruction that writes to a register being read in the
    // decode stage, the fetch and decode stages need to be stalled until the result is known
    // after the memory stage. Similarly, if we are executing a BEQZ or BNEZ that causes a RAW
    // hazard to occur, a stall must occur for the result of this operation to be forwardable.
    const struct instruction InstD = state->decode_buffer.instruction;
    if (InstD.op == BEQZ || InstD.op == BNEZ) {
        processor_stall_on_hazard(state, InstD, InstE);
    }

    execute->ForwardAE = NONE;
    execute->ForwardBE = NONE;

    int SrcBE = instruction_has_immediate(InstE) ? execute->instruction.imm : WriteDataE;

    int ALUOut = 0;

    switch (instruction_get_alu_op(InstE)) {
        case PLUS:
            ALUOut = SrcAE + SrcBE;
            break;
        case MINUS:
            ALUOut = SrcAE - SrcBE;
            break;
    }

    // We never forward the computed result of a LW instruction to a branch; it must stall
    // until the LW instruction finishes the memory stage.
    if (InstE.op != LW && state->decode_buffer.instruction.op == BEQZ || state->decode_buffer.instruction.op == BNEZ) {
//        printf("(EX) Decode has branch\n");
        if (instruction_get_output_register(InstE) != NOT_USED) {
//            printf("(EX) Instruction writes (%d) to R%d\n", InstE.op, instruction_get_output_register(InstE));
            if (instruction_get_register_read_after_write(state->decode_buffer.instruction, InstE) != NOT_USED) {
//                printf("(EX) Read after write: R%d\n", instruction_get_output_register(InstE));
                state->decode_buffer.Forward = true;
                state->decode_buffer.data = ALUOut;
//                printf("(EX) forwarding to decode: %d\n", ALUOut);
            }
        }
    }

//    printf("(EX) %d %d %d %d\n", InstE.op, InstE.rs, InstE.rt, InstE.rd);
    state->memory_buffer.ALUOut = ALUOut;
    state->memory_buffer.WriteData = WriteDataE;
    state->memory_buffer.instruction = InstE;
}

void pipeline_memory(cpu_state *state) {
    const struct memory_buffer *memory = &state->memory_buffer;

    const int ALUOutM = memory->ALUOut;
    int data = ALUOutM;
    const struct instruction InstM = memory->instruction;

    if (InstM.op == LW || InstM.op == SW) {
        if (ALUOutM < 0 || ALUOutM >= MAX_WORDS_OF_DATA) {
            printf("Exception: out-of-bounds data memory access at %d\n", ALUOutM);
//            printf("Instruction: %d %d %d %d\n", InstM.op, InstM.rs, InstM.rt, InstM.rd);
            exit(ERROR_ILLEGAL_MEM_ACCESS);
        }
    }

    // Access memory
    switch (InstM.op) {
        case LW:
            data = state->data_memory[ALUOutM];
            state->writeback_buffer.ReadData = state->data_memory[ALUOutM];
            break;
        case SW:
            state->data_memory[ALUOutM] = memory->WriteData;
            break;
    }

    // See lines 118-121.
    const struct instruction InstD = state->decode_buffer.instruction;
    if (InstM.op == LW) {
        processor_stall_on_hazard(state, InstD, InstM);
        processor_stall_on_hazard(state, state->execute_buffer.instruction, InstM);
    }

    processor_forward_on_hazard(&state->execute_buffer.ForwardAE,
                                state->execute_buffer.instruction, InstM, MEMORY);

    if (state->decode_buffer.instruction.op == BEQZ || state->decode_buffer.instruction.op == BNEZ) {
//        printf("(MEM) Decode has branch\n");
        if (instruction_get_output_register(InstM) != NOT_USED) {
//            printf("(MEM) Instruction writes (%d) to R%d\n", InstM.op, instruction_get_output_register(InstM));
            if (instruction_get_register_read_after_write(state->decode_buffer.instruction, InstM) != NOT_USED) {
//                printf("(MEM) Read after write: R%d\n", instruction_get_output_register(InstM));
                state->decode_buffer.Forward = true;
                state->decode_buffer.data = data;
//                printf("(MEM) forwarding to decode: %d\n", data);
            }
        }
    }

//    printf("(MEM) %d %d %d %d\n", InstM.op, InstM.rs, InstM.rt, InstM.rd);
    state->writeback_buffer.instruction = InstM;
    state->writeback_buffer.ALUOut = ALUOutM;
}

void pipeline_writeback(cpu_state *state) {
    struct writeback_buffer *writeback = &state->writeback_buffer;
    const struct instruction InstW = writeback->instruction;

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

//        printf("Wrote %d to R%d\n", data, dest - state->register_file);
        *dest = data;
    }

    processor_forward_on_hazard(&state->execute_buffer.ForwardAE,
                                state->execute_buffer.instruction, InstW, WRITEBACK);

    writeback->Result = data;

    if (state->decode_buffer.instruction.op == BEQZ || state->decode_buffer.instruction.op == BNEZ) {
//        printf("(WB) Decode has branch\n");
        if (instruction_get_output_register(InstW) != NOT_USED) {
//            printf("(WB) Instruction writes (%d) to R%d\n", InstW.op, instruction_get_output_register(InstW));
            if (instruction_get_register_read_after_write(state->decode_buffer.instruction, InstW) != NOT_USED) {
//                printf("(WB) Read after write: R%d\n", instruction_get_output_register(InstW));
                state->decode_buffer.Forward = true;
                state->decode_buffer.data = data;
//                printf("(WB) forwarding to decode: %d\n", data);
            }
        }
    }

    // Only increment counter if the instruction executed was not a NOP
    if (InstW.op != nop.op) {
        state->instructions_executed++;
//        printf("(WB) Writeback for %d (%d)\n", InstW.op, data);
    }
}

void simulate_cycle(cpu_state *state) {
//    printf("--- Cycle #%d ---\n", state->cycles_executed);
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
    bool debug;
    char* program_name;

    switch (argc) {
        case 3:
            debug = (strcmp(argv[1], "-D") == 0);
            program_name = argv[2];
            break;
        case 2:
            program_name = argv[1];
            break;
        default:
            printf("Usage: sim [args] [program]\n\n");
            printf("Arguments:\n");
            printf("\t-D\toutput additional information about simulator state\n");
            exit(0);
    }

    cpu_state state = {};

    /* assemble input program */
    AssembleSimpleDLX(program_name, state.instruction_memory, &state.instructions_count);

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

    if (debug) {
        printf("Registers:\n");
        print_registers(state.register_file);
        printf("Memory:\n");
        print_memory(state.data_memory);
        printf("Instructions: %d\n", state.instructions_executed);
        printf("Cycles: %d\n", state.cycles_executed);
   } else {
        printf("Final register file values:\n");
        print_registers_original(state.register_file);
        printf("\nCycles executed: %d\n", state.cycles_executed);
        printf("IPC:  %6.3f\n", (float) state.instructions_executed / (float) state.cycles_executed);
        printf("CPI:  %6.3f\n", (float) state.cycles_executed / (float) state.instructions_executed);
    }
}
