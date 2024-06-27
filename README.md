# Simple VM

Compile with `make`

Assemble a program with `-a file.asm -o output.rom` to generate a binary rom that targets the VM.

Run the program rom with `-r output.rom`

# Notes in system interrupts

Keyboard interrupts are unimplemented.

Load the address of a string into register 0, and the length of the string in register 1 before calling `INT OUT` to write to stdout.

Load the return code for the program into register 0 before calling `INT END` to end the program execution.
