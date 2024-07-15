	LDR R7 #x200        ; heap start address
	LDR R0 #4
	STR R0 &x200        ; store size (4) at heap base
	JMP NC main

mmap:
	POP R0
	POP R1          ; how many bytes
	LDR R4 #x18
	LDR R2 &x200    ; get heap size first byte
	LSL R2 R2 R4
	SUB R4 R4 #x8
	LDR R5 &x201    ; second byte
	LSL R5 R5 R4
	ORR R2 R2 R5
	SUB R4 R4 #x8
	LDR R5 &x202    ; third byte
	LSL R5 R5 R4
	ORR R2 R2 R5
	SUB R4 R4 #x8
	LDR R5 &x203    ; fourth byte
	ORR R2 R2 R5
	ADD R3 R2 #x200 ; the top of the heap
	ADD R5 R1 R2    ; new size of heap
	ADD R1 R3 R1    ; the return address
    CMP R1 ST
    JMP GE mmap_B   ; if overlapping with stack, return 0
    STR R5 &x200    ; write new heap size to base
	JMP NC mmap_G   ; return the address
mmap_B:
	LDR R1 #0       ; on bad exit set return to 0
mmap_G:
	PSH R1
	PSH R0
	RET

main:
	LDR R0 #8
	PSH R0
	JMP NC mmap
	LDR R0 #3
	PSH R0
	JMP NC mmap
	POP R0
	INT END
