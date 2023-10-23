This directory contains files for a simple unpipelined DLX simulator. The
simulator can be compiled using the "make" command at the UNIX prompt. Two
sample input programs "prog1" and "prog2" are provided. The simulator can
execute these programs using: "sim prog1".

The architecture simulator is implemented in sim.c. This file will be
modified to enhance the architecture simulated. The file assemble.c contains
a parser to read the input program. You won't have to modify this file
unless you want to add additional instructions to the simulator. The 
following instructions are supported by the simulator at present:
ADD, ADDI, SUB, SUBI, J, BEQZ, BNEZ, LW, and SW.

The simulator checks for out of bounds instruction memory and data memory
access. It also check if register 0 is overwritten. A fail safe mechanism
checks if the simulator is stuck in an infinite loop.


