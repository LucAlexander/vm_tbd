	LDR R0 #81
	PSH R0
	PSH #x10
	JMP NC sub
	POP R0
	LDR R1 #xA
	STR R0 &x200
	STR R1 &x204
	LDR R0 #x200
	LDR R1 #8
	INT OUT
	LDR R0 #0
	INT END

sub:
	POP R3
	POP R4
	POP R5
	SUB R6 R5 R4
	PSH R6
	PSH R3
	RET
