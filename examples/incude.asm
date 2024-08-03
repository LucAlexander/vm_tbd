+examples/heap

main:
	LDW	R0	#x4			; setup base heap size
	STR	R0	&x100040	; choose heap start address
	
	LAR	R2	ST
	LDW	R0	#x100040	; heap base
	SUB	R2	R2	R0		; capacity with stack limit
	LDW	R1	#24			; requested memory (bytes)
	PSH	R2
	PSH	R0
	PSH	R1
	JSR	NC	aloc		; heap allocation request
	POP	R0				; returns address of new memory

	LAR	R2	ST
	LDW	R0	#x100040
	SUB	R2	R2	R0
	LDW	R1	#1
	PSH	R2
	PSH	R0
	PSH	R1
	JSR	NC	aloc
	POP	R0

	INT	END
