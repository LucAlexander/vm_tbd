	JMP	NC	main

exit:
	LDW	R0	#0
	PSH	R0
	RET

aloc:
	LAR	R2	FP
	ADD	R2	R2	#x1
	LDW	R1	R2	#x8	; number of bytes
	LDW	R0	R2	#xc	; heap base
	LDW	R2	R2	#x10	; load heap capacity
	LDW	R3	R0	#0	; dereference base to find heap size
	ADD	R5	R3	R1	; get new heap size
	CMP	R5	R2		; if heap exceeds capacity
	JMP	GE	exit	; return 0
	STR	R5	R0	#0	; else write new heap size
	ADD	R3	R3	R0	; add original heap size to heap base
	ADD	R3	R3	#1	; add 1 to get return address
	PSH	R3
	RET
