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

main:
	LDW	R0	#x4		; setup base heap size
	STR	R0	&x200	; choose heap start address
	
	LAR	R2	ST
	LDW	R0	#x200	; heap base
	SUB	R2	R2	R0	; capacity with stack limit
	LDW	R1	#24		; requested memory (bytes)
	PSH	R2
	PSH	R0
	PSH	R1
	JSR	NC	aloc	; heap allocation request
	POP	R0			; returns address of new memory

	LAR	R2	ST
	LDW	R0	#x200
	SUB	R2	R2	R0
	LDW	R1	#1
	PSH	R2
	PSH	R0
	PSH	R1
	JSR	NC	aloc
	POP	R0

	INT	END
