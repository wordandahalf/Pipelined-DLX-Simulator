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
    NONE, MEMORY, WRITEBACK
} pipeline_forwarding_source;

typedef struct {
    // Pipeline buffer for the fetch stage containing
    // persistent state related to the fetching of instructions
    // from instruction memory
    struct fetch_state {
        int PC, PCBranch;
        bool StallF;
        bool FlushF;
    } fetch;

    // Pipeline buffer for the decode stage containing
    // persistent state related to the decoding of instructions
    // and resolution of jumps
    struct decode_state {
        int PCPlus4D;
        struct instruction InstD;
        bool StallD, PCSrc;
    } decode;

    // Pipeline buffer for the execute stage containing persistent
    // state related to the execution of instructions and
    // forwarding of known results
    struct execute_state {
        int A, B, ALUOut;
        struct instruction InstE;
        pipeline_forwarding_source ForwardAE, ForwardBE;
    } execute;

    // Pipeline buffer for the memory stage containing persistent
    // state related to the accessing of memory
    struct memory_state {
        int ALUOut, WriteData;
        struct instruction Inst;
    } memory;

    // Pipeline buffer for the writeback stage containing persistent
    // state related to the writing of results to the register file
    struct writeback_state {
        int ReadData, ALUOut, Result;
        struct instruction Inst;
    } writeback;

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

#endif //LAB1_PROCESSOR_H
