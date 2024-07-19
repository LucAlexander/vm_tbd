	JMP	NC	main

add:
	LAR	R1	FP
	ADD	R1	R1	#x1
	LDW R3	R1	#x8
	LDW R2	R1	#xc
	ADD R0	R3	R2
	RET

main:
	LDW	R0	#5
	LDW R1	#6
	PSH	R0
	PSH	R1
	JMP	NC	add
	INT	END

