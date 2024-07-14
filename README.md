# Simple Fantasy CPU VM

Compile with `make`

Assemble a program with `-a file.asm -o output.rom` to assemble a binary rom that targets the VM.

Run the program rom with `-r output.rom`

## Instruction set

    .----------------------------------------------,
	| NOP |             |           |              |
	| LDR | 0000 m reg  | 2 byte address or int    |
	| LDI | 0 m dst src | 2 byte int or reg offset |
	| STR | 0000 m reg  | 2 byte address or reg    |
	| STI | 0 m src dst | 2 byte int or reg offset |
    |-----+-------------+--------------------------|
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
    | COM | 0000 m reg  | 2 byte literal or reg    |
    |-----+-------------+--------------------------|
	| PSH | 0000 m reg  | 2 byte literal           |
    | POP | 00000 reg   |           |              |
    | CMP | 00000 op1   | 00000 op2 |              |
    | JMP | metric byte | 2 byte label address     |
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
add:
	POP R3          ; pop PC
	POP R4          ; get arg 1
	POP R5          ; get arg 0
	ADD R6 R4 R5    ; do functionality for procedure
	PSH R6          ; push procedure return
	PSH R3          ; push PC back onto stack for return
	RET             ; pop and jump to program counter


    LDR R0 #65      
	PSH R0          ; push arg0 
	PSH #x10        ; push arg 1
	JMP NC add      ; invoke procedure with no condition (NC)
	POP R0          ; get procedures return value

```

Invoking interrupts requires loading arguments into registers.

```asm
	LDR R0 #0       ; load program return code
	INT END         ; call interrupt to end program
```
