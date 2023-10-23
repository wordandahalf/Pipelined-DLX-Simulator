Using GDB (GNU Debugger)

Compile the application as with the following command at the UNIX prompt:

  gcc sim.c assemble.c -g -o sim

The -g option tells the simulator to place debugging information into the 
executable (sim).

To run the debugger type at the UNIX prompt:

  gdb sim

This should bring up the debugger with the (gdb) prompt. At that prompt 
type in the arguments for your simulator (ie the DLX assembly file to 
run):

  set args prog1

You can either run the program directly now or you can place a break point 
in your program. To place a break point -- at line 33 of sim.c for 
instance -- type:

  break sim.c:35

To run the program type:

  run

If it reaches the break point, you can print the values of any variable by 
typing (for instance to see the value of IR):

  p IR

To then execute the program line by line type: s  or  n
If you are at a function call 's' will take you into the funtion (step 
into it). If you want to skip the details of the function n will take you 
to the next instruction.

To continue execution (not line by line), type: c
