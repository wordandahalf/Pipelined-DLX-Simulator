#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processor.h"
#include "debug.h"

void pipeline_fetch(cpu_state *state) {
    struct fetch_buffer *fetch = &state->fetch_buffer;

    // Do nothing if stalling is requested. The fetch stage always stalls
    // alongside the decode_buffer stage, which handles injecting a NOP into the execute stage.
    if (fetch->stall) {
        fetch->stall = false;
        return;
    }

    // Flush if requested. This adds a NOP into the decode stage in order
    // to account for mispredicted jumps.
    if (fetch->flush) {
        state->decode_buffer.inst = nop;
        fetch->flush = false;
        fetch->pc = fetch->pc_branch;
        return;
    }

    const int pc = fetch->pc;

    // A pipelined processor is not done executing until the last instruction reaches the writeback stage.
    // To facilitate this, we fill the pipeline with NOPs when accessing out-of-bounds instructions not caused
    // by a should_jump instruction (i.e., jumping out-of-bounds will still result in an error being thrown).
    if (pc > state->instructions_count - 1) {
        // If the last instruction has reached the writeback stage, we should halt the processor. This occurs when
        // four additional instructions have been fetched by the processor.
        if (pc >= state->instructions_count + 3) {
            state->halt = true;
        } else {
            // Otherwise, we keep injecting NOPs.
            state->decode_buffer.inst = nop;
        }
    }

    const int next_pc = fetch->pc + 1;

    state->decode_buffer.inst = state->instruction_memory[pc];
    state->decode_buffer.pc_next = next_pc;

    // Update the program counter for the next cycle. If we are jumping to an out-of-bounds address,
    // halt the simulator with an error.
    if (state->decode_buffer.should_jump && (fetch->pc_branch < 0 || fetch->pc_branch > state->instructions_count - 1)) {
        printf("out-of-bounds should_jump to %d\n", fetch->pc_branch);
        exit(ERROR_ILLEGAL_JUMP);
    }

    fetch->pc = state->decode_buffer.should_jump ? fetch->pc_branch : next_pc;
}

void pipeline_decode(cpu_state *state) {
    struct decode_buffer *decode = &state->decode_buffer;
    const struct instruction inst = state->decode_buffer.inst;

    // Inject a NOP into the execute stage when requested to stall. Instruct the fetch stage to stall.
    if (decode->stall) {
        decode->stall = false;
        state->fetch_buffer.stall = true;
        state->execute_buffer.inst = nop;
        return;
    }

    int a, b;

    // Access register file. Use the forwarded value for avoiding control hazards, if necessary.
    a = decode->forward ? decode->data : state->register_file[inst.rs];
    b = state->register_file[inst.rt];
    decode->forward = false;

    // Resolve should_jump instructions
    bool should_jump = false;
    switch (inst.op) {
        case BEQZ:
            should_jump = a == 0;
            break;
        case BNEZ:
            should_jump = a != 0;
            break;
        case J:
            should_jump = true;
            break;
    }

    decode->should_jump = should_jump;
    state->fetch_buffer.flush = should_jump;

    // Again, there should be a shift here, but since instruction memory is not byte-addressed,
    // it is omitted.
    state->fetch_buffer.pc_branch = inst.imm + decode->pc_next;

    state->execute_buffer.inst = inst;
    state->execute_buffer.a = a;
    state->execute_buffer.b = b;
}

void pipeline_execute(cpu_state *state) {
    struct execute_buffer *execute = &state->execute_buffer;
    const struct instruction inst = execute->inst;

    int a = execute->a;
    int write_data = execute->b;

    // Handle forwarding from the memory and writeback stages for
    // the two operands.
    if(execute->foward_a == MEMORY)
        // If the instruction reads from memory, we want the value read from memory, not
        // the calculated address.
        a = instruction_get_memory_operation(state->memory_buffer.inst) == READ
                ? state->writeback_buffer.read_data
                : state->memory_buffer.alu_out;
    else if(execute->foward_a == WRITEBACK)
        a = state->writeback_buffer.result;

    if(execute->forward_b == MEMORY)
        // See above.
        write_data = instruction_get_memory_operation(state->memory_buffer.inst) == READ
                ? state->writeback_buffer.read_data
                : state->memory_buffer.alu_out;
    else if(execute->forward_b == WRITEBACK)
        write_data = state->writeback_buffer.result;

    execute->foward_a  = NO_FORWARDING;
    execute->forward_b = NO_FORWARDING;

    int b = instruction_has_immediate(inst) ? execute->inst.imm : write_data;

    int alu_out = 0;

    switch (instruction_get_alu_op(inst)) {
        case PLUS:  alu_out = a + b; break;
        case MINUS: alu_out = a - b; break;
    }

    // We don't forward to avoid control hazards in the execute stage.
    if (instruction_is_branch(state->decode_buffer.inst)) {
        processor_stall_on_hazard(state, state->decode_buffer.inst, inst);
    }

    state->memory_buffer.alu_out = alu_out;
    state->memory_buffer.write_data = write_data;
    state->memory_buffer.inst = inst;
}

void pipeline_memory(cpu_state *state) {
    const struct memory_buffer *memory = &state->memory_buffer;

    const int alu_out = memory->alu_out;
    int data = alu_out;
    const struct instruction inst = memory->inst;
    const mem_op op = instruction_get_memory_operation(inst);

    // Validate the address to be accessed, if necessary.
    if (op != NO_OPERATION) {
        if (alu_out < 0 || alu_out >= MAX_WORDS_OF_DATA) {
            printf("Exception: out-of-bounds data memory access at %d\n", alu_out);
            exit(ERROR_ILLEGAL_MEM_ACCESS);
        }
    }

    // Perform the necessary memory operation
    switch (op) {
        case READ:
            data = state->data_memory[alu_out];
            state->writeback_buffer.read_data = state->data_memory[alu_out];

            // If we are reading from memory, we have to stall if either the execute or decode
            // read from the register this operation writes to
            processor_stall_on_hazard(state, state->decode_buffer.inst, inst);
            processor_stall_on_hazard(state, state->execute_buffer.inst, inst);
            break;
        case WRITE:
            state->data_memory[alu_out] = memory->write_data;
            break;
    }

    processor_forward_on_hazard(state, &state->execute_buffer.foward_a,
                                state->execute_buffer.inst, inst, MEMORY, data);

    state->writeback_buffer.inst = inst;
    state->writeback_buffer.alu_out = alu_out;
}

void pipeline_writeback(cpu_state *state) {
    struct writeback_buffer *writeback = &state->writeback_buffer;
    const struct instruction inst = writeback->inst;

    int *dest = NULL;
    int data = 0;

    if (inst.op == ADD || inst.op == SUB)
        dest = &state->register_file[inst.rd];
    if (inst.op == ADDI || inst.op == SUBI || inst.op == LW)
        dest = &state->register_file[inst.rt];

    if (inst.op >= ADDI && inst.op <= SUB)
        data = writeback->alu_out;
    if (inst.op == LW)
        data = writeback->read_data;

    if (dest != NULL) {
        if (dest == state->register_file + R0) {
            printf("Exception: Attempt to overwrite R0");
            exit(ERROR_ILLEGAL_REG_WRITE);
        }

        *dest = data;
    }

    processor_forward_on_hazard(state, &state->execute_buffer.foward_a,
                                state->execute_buffer.inst, inst, WRITEBACK, data);

    writeback->result = data;

    // Only increment counter if the instruction executed was not a NOP
    if (inst.op != nop.op) {
        state->instructions_executed++;
    }
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
