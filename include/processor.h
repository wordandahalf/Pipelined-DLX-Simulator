#ifndef LAB1_PROCESSOR_H
#define LAB1_PROCESSOR_H

#include <stdbool.h>
#include "instruction.h"

// Max cycles simulator will execute -- to stop a runaway simulator
#define MAX_CYCLES 500000

#define ERROR_ILLEGAL_REG_WRITE  (-1)
#define ERROR_ILLEGAL_MEM_ACCESS (-2)
#define ERROR_ILLEGAL_JUMP (-3)

// An enumeration of pipeline stages from which data can be
// forwarded
typedef enum {
    NONE, EXECUTE, MEMORY, WRITEBACK
} forwarding_source;

typedef struct {
    // Pipeline buffer for the fetch stage containing
    // persistent state related to the fetching of instructions
    // from instruction memory
    struct fetch_buffer {
        int PC, PCBranch;
        bool StallF;
        bool FlushF;
    } fetch_buffer;

    // Pipeline buffer for the decode stage containing
    // persistent state related to the decoding of instructions
    // and resolution of jumps
    struct decode_buffer {
        int PCPlus4D;
        struct instruction instruction;
        bool StallD, PCSrc;
        bool Forward;
        int data;
    } decode_buffer;

    // Pipeline buffer for the execute stage containing persistent
    // state related to the execution of instructions and
    // forwarding of known results
    struct execute_buffer {
        int A, B, ALUOut;
        struct instruction instruction;
        forwarding_source ForwardAE, ForwardBE;
    } execute_buffer;

    // Pipeline buffer for the memory stage containing persistent
    // state related to the accessing of memory
    struct memory_buffer {
        int ALUOut, WriteData;
        struct instruction instruction;
    } memory_buffer;

    // Pipeline buffer for the writeback stage containing persistent
    // state related to the writing of results to the register file
    struct writeback_buffer {
        int ReadData, ALUOut, Result;
        struct instruction instruction;
    } writeback_buffer;

    // The instruction memory, directly containing parsed
    // instructions
    struct instruction instruction_memory[MAX_LINES_OF_CODE];

    // The number of instructions in instruction memory. The contents
    // beyond instructions_count - 1 is undefined.
    int instructions_count;

    // Data memory, word-addressed
    int data_memory[MAX_WORDS_OF_DATA];

    // The container for the 16 registers of the DLX processor. The first
    // register is always zero; writing to it will raise an error.
    int register_file[16];

    // The number of cycles the simulator has executed
    int cycles_executed;

    // The number of instructions the simulator has executed
    int instructions_executed;

    // If true, the simulator ceases execution of the program after
    // the current cycle.
    bool halt;
} cpu_state;

void pipeline_fetch(cpu_state *state);
void pipeline_decode(cpu_state *state);
void pipeline_execute(cpu_state *state);
void pipeline_memory(cpu_state *state);
void pipeline_writeback(cpu_state *state);

/**
 * Stalls the decode and fetch stages if a RAW data hazard occurs
 * @param state the processor state
 * @param reader the instruction executing after writer
 * @param writer the instruction executed before reader
 */
void processor_stall_on_hazard(cpu_state *state, struct instruction reader, struct instruction writer) {
    if (instruction_get_register_read_after_write(reader, writer) != NOT_USED) {
        state->decode_buffer.StallD = true;
    }
}

/**
 * Instructs the execute stage to forward the necessary operands from the provided
 * source when a RAW data hazard occurs.
 * @param state the processor state
 * @param reader the instruction executing after writer
 * @param writer the instruction executed before reader
 * @param source the source from which to forward
 */
void processor_forward_on_hazard(forwarding_source *stage, struct instruction reader, struct instruction writer,
                                 forwarding_source source) {
    int hazard_register = instruction_get_register_read_after_write(reader, writer);
    if (hazard_register != NOT_USED) {
        if (hazard_register == reader.rs)
            *stage = source;
        if (hazard_register == reader.rt)
            *(stage + 1) = source;
    }
}

#endif //LAB1_PROCESSOR_H
