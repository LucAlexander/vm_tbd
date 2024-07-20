# Simple Fantasy CPU VM

Compile with `make`

Assemble a program with `-a file.asm -o output.rom` to assemble a binary rom that targets the VM.

Run the program rom with `-r output.rom`

## Instruction set


    
    ,----------------------------------------------.
	| NOP |             |           |              |
    |-----+-------------+-----------+--------------|
    | LDW | 00  dst src | 00000 ofs |              |
	|     | 01  dst src | 2 byte ofs literal       |
	|     | 10  dst     | 2 byte src address       |
	|     | 11  dst     | 2 byte src literal       |
	| LDB | 00  dst src | 00000 ofs |              |
	|     | 01  dst src | 2 byte ofs literal       |
	|     | 10  dst     | 2 byte src address       |
	| STR | 00  src dst | 00000 ofs |              |
	|     | 01  src dst | 2 byte ofs literal       |
	|     | 10  src     | 2 byte dst address       |
	| STB | 00  src dst | 00000 ofs |              |
	|     | 01  src dst | 2 byte ofs literal       |
	|     | 10  src     | 2 byte dst address       |
    |-----+-------------+-----------+--------------|
	| LAR | 00000   dst | 4 bit src |              |
    |-----+-------------+-----------+--------------|
	| ADD | 0 m dst op1 | 2 byte int or reg op2    |
	| SUB | ...         | ...                      |
	| MUL | ...         | ...                      |
	| DIV | ...         | ...                      |
    | MOD | ...         | ...                      |
	| LSL | ...         | ...                      |
    | LSR | ...         | ...                      |
    | AND | ...         | ...                      |
    | ORR | ...         | ...                      |
    | XOR | ...         | ...                      |
    |-----+-------------+--------------------------|
    | COM | 0000 m reg  | 2 byte literal or reg    |
    |-----+-------------+--------------------------|
	| PSH | 0000 m reg  | 2 byte literal           |
    | POP | 00000 reg   |           |              |
    | CMP | 00000 op1   | 00000 op2 |              |
    | JMP | metric byte | 2 byte label address     |
    | JSR | metric byte | 2 byte label address     |
    | RET |             |           |              |
    | INT | interrupt   |           |              |
    `----------------------------------------------'

## Comparison metrics

    .--------------------.
    | NC | No condition  |
    | EQ | Equal         |
    | NE | Not Equal     |
    | LT | Less thanion  |
    | GT | Greater than  |
    | LE | Less Equal    |
    | GE | Greater Equal |
    `--------------------'

## Interrupts

    .-------------------------------------------------------------------,
    | OUT | Print ascii string to stdout | R0 <- address, R1 <- length  |
    | KBD | Get keyboard key status      | (unimplemented)              |
    | END | End the program and return   | R0 <- process return code    |
    `-------------------------------------------------------------------'
 

# Calling convention

All the responsibility of cleanup lies within the called procedure.

```asm

	LDW R0 #5
	LDW R1 #6
	PSH R0        ; push arg 1
	PSH R1        ; push arg 2
	JSR NC add    ; jump to subroutine
	POP R0        ; retrieve return value
	INT END

add:
	LAR R1 FP     ; load auxiliary register (frame pointer)
	ADD R1 R1 #x1 ; add 1 to get previous element in stack
	LDW R3 R1 #x8 ; load stack address offset 8 bytes from the frame pointer (arg 2)
	LDW R2 R1 #xc ; load stack address offset 12 bytes from the frame pointer (arg 1)
	ADD R2 R3 R2  ; function body
	PSH R2        ; push return value
	RET           ; return to previous stack frame

```

Invoking interrupts requires loading arguments into registers.

```asm

	LDW R0 #0       ; load program rom return code
	INT END         ; call interrupt to end program

```
