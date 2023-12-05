### Pipelined-DLX-Simulator

A DLX simulator with pipelining.

### Building
Run `make all` to build the simulator.

### Testing
To test it against the output of WinMIPS64, run `make test`.
This compares the output of the simulator (with the `-D` flag, see below) with the corresponding known, good output in
`test/` for each program in `programs/`.

To add a new test case, simply add the program in `programs/` and the expected output in `test/`. The files must be named identically and consist of only numbers.

### Usage
`Usage: sim [args] [program]`. The `-D` flag indicates enhanced debugging information should be printed after execution of the program.
Without any flags, it outputs the final register values, the number of cycles per instruction, and the number of instructions per cycle.
