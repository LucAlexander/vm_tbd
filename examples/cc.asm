	LDW	R0	#4
	JMP	NC	one
	INT	END

one:
	JMP	NC	two
	ADD	R0	R0	R0
	RET

two:
	RET
