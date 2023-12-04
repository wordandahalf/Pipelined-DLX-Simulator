#ifndef LAB1_INSTRUCTION_H
#define LAB1_INSTRUCTION_H

#include <stdbool.h>
#include "globals.h"

// An enumeration of the possible ALU operations an
// instruction can encode.
typedef enum {
    UNDEFINED, PLUS, MINUS
} alu_op;

typedef enum {
    READ, WRITE, NO_OPERATION
} mem_op;

const struct instruction nop = { .op = NOP, .rd = NOT_USED, .rt = NOT_USED, .rs = NOT_USED, .imm = 0 };

/**
 * @return the number of the register the provided instruction writes to. If the
 * instruction does not write to a register, returns NOT_USED.
 */
int instruction_get_output_register(struct instruction instruction) {
    switch (instruction.op) {
        case ADDI:
        case SUBI:
        case LW:
            return instruction.rt;
        case ADD:
        case SUB:
            return instruction.rd;
        default:
            return NOT_USED;
    }
}

/**
 * @return true if the provided instruction has an immediate operand, false otherwise.
 */
bool instruction_has_immediate(struct instruction instruction) {
    switch (instruction.op) {
        case ADDI:
        case SUBI:
        case LW:
        case SW:
            return true;
        default:
            return false;
    }
}

mem_op instruction_get_memory_operation(struct instruction instruction) {
    switch (instruction.op) {
        case LW: return READ;
        case SW: return WRITE;
        default: return NO_OPERATION;
    }
}

bool instruction_is_branch(struct instruction instruction) {
    switch (instruction.op) {
        case BEQZ:
        case BNEZ:
            return true;
        default:
            return false;
    }
}

/**
 * @return the ALU operation the provided instruction encodes for.
 */
alu_op instruction_get_alu_op(struct instruction instruction) {
    switch (instruction.op) {
        case ADDI:
        case ADD:
        case LW:
        case SW:
            return PLUS;
        case SUBI:
        case SUB:
            return MINUS;
        default:
            return UNDEFINED;
    }
}

/**
 * @param reader the instruction executing after writer
 * @param writer the instruction executed before reader
 * @return the number of the register that will encounter a read-after-write data hazard. If no RAW hazard
 * occurs, returns NOT_USED.
 */
int instruction_get_reg_read_after_write(struct instruction reader, struct instruction writer) {
    int write_register = instruction_get_output_register(writer);

    if (write_register == NOT_USED)
        return NOT_USED;

    switch (reader.op) {
        case ADD:
        case SUB:
        case LW:
        case SW:
            if (reader.rs == write_register) return reader.rs;
            if (reader.rt == write_register) return reader.rt;
            return NOT_USED;
        case ADDI:
        case SUBI:
        case BEQZ:
        case BNEZ:
            if (reader.rs == write_register) return reader.rs;
            return NOT_USED;
        default:
            return NOT_USED;
    }
}

#endif //LAB1_INSTRUCTION_H
