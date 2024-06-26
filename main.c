#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define DEBUG 0

// registers
enum {
	R0=0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	ST,
	PC,
	SR, //-zs- 4th, 1st bit unused for now
	REGISTER_COUNT
};

// opcodes
enum {
	NOP=0,
	LDR,// 0000 m reg, 4 byte address | 4 byte int
	LDI,// 0 m dst src  (to dereference), 4 byte offset | reg offset
	STR,// 0000 m reg|000, 4 byte int, 4 byte address
	STI,// 0 m src dst (to dereference), 4 byte offset | reg offset
	ADD,//00000 dst, 00 op1 op2
	MUL,
	DIV,
	MOD,
	ASL,// 00 dst src
	ASR,
	LSR,
	PSH,//00000 reg
	POP,//00000 reg
	CMP,//00 op1 op2
	JMP,//metric byte, 4 byte label
	RET,// pop to pc
	INT// byte system interrupt
};

// system interrupts
enum {
	END=0,
	KBD,
	OUT // R0: str R1: len
};

// comparison metrics
enum {
	NC=0,
	EQ,
	NE,
	LT,
	GT,
	LE,
	GE
};

typedef uint8_t byte;

typedef struct label_assoc{
	enum{LABEL_MATCH, LABEL_NEED} tag;
	char k[8];
	uint32_t v;
	struct label_assoc* next;
	struct label_assoc* other;
}label_assoc;

static int32_t reg[REGISTER_COUNT];
static int32_t stack[0x1000];
static byte ram[0x1000];

#define ROM_ADDRESS 0x0
#define ROM_SIZE 0x100
#define ROM_END ROM_ADDRESS+ROM_SIZE

#define STACK_PUSH(v) stack[reg[ST]++] = v
#define STACK_POP stack[--reg[ST]]
#define NEXT ram[reg[PC]++]
#define LOAD (NEXT<<24)+ (NEXT<<16)+ (NEXT<<8)+ (NEXT)
#define LOAD_ADDRESS (uint32_t)(LOAD)
#define LOAD_VALUE (int32_t)(LOAD)
#define READ(addr) (ram[addr]<<24) + (ram[addr+1]<<16) + (ram[addr+2]<<8) + (ram[addr+3])
#define READ_ADDRESS(addr) (uint32_t)(READ(addr))
#define READ_VALUE(addr) (int32_t)(READ(addr))
#define WRITE(addr, val)\
	ram[addr] = (val>>24) & (0xFF);\
	ram[addr+1] = (val>>16) & (0xFF);\
	ram[addr+2] = (val>>8) & (0xFF);\
	ram[addr+3] = (val) & (0xFF);\

void to_stdout(){
	uint32_t n = reg[R1];
	for (uint32_t i=0;i<n;++i){
		putchar(ram[reg[R0]+i]);
	}
}

void handle_interrupt(byte intr){
	for (size_t i = PC;i<REGISTER_COUNT;++i){
		STACK_PUSH(reg[i]);
	}
	switch (intr){
	case END:
#if (DEBUG==1)
		printf("END\n");
#endif
		reg[PC]=ROM_END;
		STACK_PUSH(reg[R0]);
		return;
	case KBD:
#if (DEBUG==1)
		printf("KBD\n");
#endif
		break;
	case OUT:
#if (DEBUG==1)
		printf("OUT\n");
#endif
		to_stdout();
		break;
	}
	for (size_t i = REGISTER_COUNT-1;i>=PC;--i){
		reg[i] = STACK_POP;
	}
}

uint8_t check_metric(byte metric){
	switch (metric){
	case NC: return 1;
	case EQ: return reg[SR] & (1<<2);
	case NE: return !(reg[SR] & (1<<2));
	case LT: return !(reg[SR] & (1<<1));
	case GT: return reg[SR] & (1<<1);
	case LE: return (!(reg[SR] & (1<<1))) | (reg[SR] & (1<<2));
	case GE: return reg[SR];
	}
	return 0;
}

void progress(){
	byte a, b, m, src, dst, op1, op2;
	int32_t offset, src_val, x, y;
	uint32_t src_address, dst_address, preserve;
	byte opcode = NEXT;
	switch (opcode){
	case LDR:
#if (DEBUG == 1)
		printf("LDR\n");
#endif
		a = NEXT;
		m = a >> 3;
		dst = a & 0x7;
		if (m){
			reg[dst] = ram[LOAD_ADDRESS];
			break;
		}
		reg[dst] = LOAD_VALUE;
		break;
	case LDI:
#if (DEBUG == 1)
		printf("LDI\n");
#endif
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & (0x7);
		src_address = ram[reg[a & 0x7]];
		if (m){
			offset = LOAD_VALUE;
		}
		else{
			offset = reg[NEXT];
		}
		reg[dst] = READ(src_address+offset);
		break;
	case STR:
#if (DEBUG == 1)
		printf("STR\n");
#endif
		a = NEXT;
		m = a >> 3;
		if (m){
			src = a & 0x7;
			dst_address = LOAD_ADDRESS;
			WRITE(dst_address, reg[src])
			break;
		}
		src_val = LOAD_VALUE;
		WRITE(LOAD_ADDRESS, src_val)
		break;
	case STI:
#if (DEBUG == 1)
		printf("STI\n");
#endif
		a = NEXT;
		m = a >> 6;
		src = (a >> 3) & (0x7);
		dst_address = reg[a & 0x7];
		if (m){
			offset = LOAD_VALUE;
		}
		else{
			offset = reg[NEXT];
		}
		WRITE(dst_address+offset, reg[src])
		break;
	case ADD:
#if (DEBUG == 1)
		printf("ADD\n");
#endif
		a = NEXT;
		b = NEXT;
		dst = a & 0x7;
		op1 = b >> 3;
		op2 = b & 0x7;
		reg[dst] = reg[op1]+reg[op2];
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case MUL:
#if (DEBUG == 1)
		printf("MUL\n");
#endif
		a = NEXT;
		b = NEXT;
		dst = a & 0x7;
		op1 = b >> 3;
		op2 = b & 0x7;
		reg[dst] = reg[op1]*reg[op2];
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case DIV:
#if (DEBUG == 1)
		printf("DIV\n");
#endif
		a = NEXT;
		b = NEXT;
		dst = a & 0x7;
		op1 = b >> 3;
		op2 = b & 0x7;
		reg[dst] = reg[op1]/reg[op2];
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case MOD:
#if (DEBUG == 1)
		printf("MOD\n");
#endif
		a = NEXT;
		b = NEXT;
		dst = a & 0x7;
		op1 = b >> 3;
		op2 = b & 0x7;
		reg[dst] = reg[op1]%reg[op2];
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case ASL:
#if (DEBUG == 1)
		printf("ASL\n");
#endif
		a = NEXT;
		dst = a >> 3;
		src = a & 0x7;
		reg[dst] = reg[src] << 1;
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case ASR:
#if (DEBUG == 1)
		printf("ASR\n");
#endif
		a = NEXT;
		dst = a >> 3;
		src = a & 0x7;
		preserve = reg[src] & 1<<31;
		reg[dst] = (reg[src] >> 1) | preserve;
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case LSR:
#if (DEBUG == 1)
		printf("LSR\n");
#endif
		a = NEXT;
		dst = a >> 3;
		src = a & 0x7;
		reg[dst] = reg[src] >> 1;
		reg[SR] = ((reg[dst]==0) << 2)
				   | ((reg[dst] > 0) << 1)
				   | (0);
		break;
	case PSH:
#if (DEBUG == 1)
		printf("PSH\n");
#endif
		a = NEXT;
		src = a & 0x7;
		STACK_PUSH(reg[src]);
		break;
	case POP:
#if (DEBUG == 1)
		printf("POP\n");
#endif
		a = NEXT;
		dst = a & 0x7;
		reg[dst] = STACK_POP;
		break;
	case CMP:
#if (DEBUG == 1)
		printf("CMP\n");
#endif
		a = NEXT;
		x = reg[a >> 3];
		y = reg[a & 0x7];
		reg[SR] = ((x==y) << 2)
				   | ((x>y) << 1)
				   | (0);
		break;
	case JMP:
#if (DEBUG == 1)
		printf("JMP\n");
#endif
		if (check_metric(NEXT & 0x7)){
			STACK_PUSH(reg[PC]);
			reg[PC] = LOAD_ADDRESS;
			break;
		}
		preserve = LOAD_ADDRESS;// eats the bytes needed to be et
		break;
	case RET:
#if (DEBUG == 1)
		printf("RET\n");
#endif
		reg[PC] = STACK_POP;
		break;
	case INT:
#if (DEBUG == 1)
		printf("INT\n");
#endif
		handle_interrupt(NEXT);
		break;
	case NOP:
#if (DEBUG == 1)
		printf("NOP\n");
#endif
		break;	  
	default:
#if (DEBUG == 1)
		printf("invalid instruction opcode %.2x\n", opcode);
#endif
		break;
	}
}

void load_rom(uint32_t address, size_t len){
	for (size_t i = 0;i<len;++i){
		ram[ROM_ADDRESS+i] = ram[address+i];
	}
}

void run_rom(){
	reg[PC] = ROM_ADDRESS;
	reg[ST] = 0;
	while (reg[PC] != ROM_END) {
		progress();
	}
}

#define assert_return(cond) if (!(cond)) {printf("assertion " #cond " failed at %u\n",__LINE__);return 0;}
#define assert_error(cond) if (!(cond)) {printf("assertion " #cond " failed at %u\n",__LINE__);*err=1;return 0;}

uint8_t whitespace(char c){
	return (c == ' ') || (c == '\n') || (c == '\r') || (c == '\t');
}

char parse_spaces(FILE* fd){
	char c;
	do {
		c = fgetc(fd);
		if (!whitespace(c)){
			return c;
		}
	} while(c!=EOF);
	return EOF;
}

#define MATCH_REGISTER(tok) if (strcmp(#tok, r)==0){ return tok; } else

byte parse_register(FILE* fd, char c, uint8_t* err){
	char r[] = "..";
	uint8_t i = 0;
	while (c != EOF && i < 2){
		r[i++] = c;
		c = fgetc(fd);
	}
	assert_error(c!=EOF)
#if (DEBUG==1)
	printf("%s ", r);
#endif
	MATCH_REGISTER(R0)
	MATCH_REGISTER(R1)
	MATCH_REGISTER(R2)
	MATCH_REGISTER(R3)
	MATCH_REGISTER(R4)
	MATCH_REGISTER(R5)
	MATCH_REGISTER(R6)
	MATCH_REGISTER(R7)
	MATCH_REGISTER(SR)
	{
		*err = 1;
		return 0;
	}
}

int32_t parse_numeric(FILE* fd, uint8_t* err){
	uint8_t base = 10;
	char num[] = "................................";
	uint8_t i = 0;
	char c = fgetc(fd);
	while(c != EOF && i < 10 && (!whitespace(c))){
		num[i++] = c;
		c = fgetc(fd);
	}
	assert_error(c!=EOF)
	char pivot = num[0];
	if (pivot=='x'){
		base = 16;
		num[0] = '0';
	}
	if (pivot=='b'){
		base = 2;
		num[0] = '0';
	}
	char* ptr;
	int32_t result = strtol(num, &ptr, base);
#if (DEBUG==1)
	printf("(%s base %u = %d) ", num, base, result);
#endif
	return result;
}

uint8_t parse_NOP(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = NOP;
#if (DEBUG==1)
	printf("NOP ");
#endif
	return 1;
}

void push_4bytes(byte* encoded, size_t* const size, uint32_t address){
		for (uint8_t i = 0;i<4;++i){
			encoded[(*size)++] = (address >> (32-(8*(i+1)))) & (0xFF);
#if (DEBUG==1)
			printf("%u ", (address >> (32-(8*(i+1))))&(0xFF));
#endif
		}
}

uint8_t parse_LDR(FILE* const fd, byte* encoded, size_t* const const size){
	encoded[(*size)++] = LDR;
#if (DEBUG==1)
	printf("LDR ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte r = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	if (c=='&'){
		encoded[(*size)++] = r | (1<<3);
		uint32_t address = parse_numeric(fd, &err);
		assert_return(!err)
		push_4bytes(encoded, size, address);
	}
	else if (c=='#'){
		encoded[(*size)++] = r;
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		push_4bytes(encoded, size, value);
	}
	return 1;
}

uint8_t parse_LDI(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = LDI;
#if (DEBUG==1)
	printf("LDI ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte dst = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte src = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	if (c=='#'){
		int32_t offset = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (1<<6) | (dst << 3) | src;
		push_4bytes(encoded, size, offset);
	}
	else{
		byte off = parse_register(fd, c, &err);
		assert_return(!err)
		encoded[(*size)++] = (dst << 3) | src;
		encoded[(*size)++] = off;
	}
	return 1;
}

uint8_t parse_STR(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = STR;
#if (DEBUG==1)
	printf("STR ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	if (c == '#'){
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = 0;
		push_4bytes(encoded, size, value);
	}
	else{
		byte src = parse_register(fd, c, &err);
		assert_return(!err)
		encoded[(*size)++] = src | (1<<3);
	}
	c = parse_spaces(fd);
	assert_return(c!=EOF && c=='&')
	uint32_t address = parse_numeric(fd, &err);
	assert_return(!err)
	push_4bytes(encoded, size, address);
	return 1;
}

uint8_t parse_STI(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("STI ");
#endif
	encoded[(*size)++] = STI;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte src = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte dst = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	if (c=='#'){
		int32_t offset = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (1<<6) | (src << 3) | dst;
		push_4bytes(encoded, size, offset);
	}
	else{
		byte off = parse_register(fd, c, &err);
		assert_return(!err)
		encoded[(*size)++] = (src << 3) | dst;
		encoded[(*size)++] = off;
	}
	return 1;
}

uint8_t parse_alu_op(FILE* fd, byte* encoded, size_t* const size){
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte dst = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte op1 = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte op2 = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	encoded[(*size)++] = dst;
	encoded[(*size)++] = (op1<<3) | op2;
	return 1;
}

uint8_t parse_ADD(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = ADD;
#if (DEBUG==1)
	printf("ADD ");
#endif
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_MUL(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("MUL ");
#endif
	encoded[(*size)++] = MUL;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_DIV(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("DIV ");
#endif
	encoded[(*size)++] = DIV;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_MOD(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("MOD ");
#endif
	encoded[(*size)++] = MOD;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_2_register_byte(FILE* fd, byte* encoded, size_t* const size){
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte dst = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte src = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	encoded[(*size)++] = (dst << 3) | src;
	return 1;
}

uint8_t parse_ASL(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("ASL ");
#endif
	encoded[(*size)++] = ASL;
	return parse_2_register_byte(fd, encoded, size);
}

uint8_t parse_ASR(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("ASR ");
#endif
	encoded[(*size)++] = ASR;
	return parse_2_register_byte(fd, encoded, size);
}

uint8_t parse_LSR(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("LSR ");
#endif
	encoded[(*size)++] = LSR;
	return parse_2_register_byte(fd, encoded, size);
}

uint8_t parse_PSH(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("PSH ");
#endif
	encoded[(*size)++] = PSH;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte src = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	encoded[(*size)++] = src;
	return 1;
}

uint8_t parse_POP(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("POP ");
#endif
	encoded[(*size)++] = POP;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte src = parse_register(fd, c, &err);
	assert_return(c!=EOF)
	encoded[(*size)++] = src;
	return 1;
}

uint8_t parse_CMP(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("CMP ");
#endif
	encoded[(*size)++] = CMP;
	return parse_2_register_byte(fd, encoded, size);
}

#define MATCH_METRIC(tok) if (strcmp(#tok, op)==0){ encoded[(*size)++] = tok; } else

uint8_t parse_metric(FILE* fd, char c, byte* encoded, size_t* const size){
	char op[] = "..";
	uint8_t i = 0;
	while (c != EOF && i < 2){
		op[i++] = c;
		c = fgetc(fd);
	}
#if (DEBUG==1)
	printf("%s ", op);
#endif
	assert_return(c!=EOF)
	MATCH_METRIC(EQ)
	MATCH_METRIC(NE)
	MATCH_METRIC(LT)
	MATCH_METRIC(GT)
	MATCH_METRIC(LE)
	MATCH_METRIC(GE)
	{
		encoded[(*size)++]=NC;
	}
	return 1;
}

uint32_t seek_jump_label(label_assoc** labels, char* lab, size_t ip){
	label_assoc* head = *labels;
	while (head != NULL){
		if (strcmp(head->k, lab)!=0){
			head = head->next;
			continue;
		}
		if (head->tag == LABEL_MATCH){
			return head->v;
		}
		label_assoc* rest = head->other;
		label_assoc* new = malloc(sizeof(label_assoc));
		new->tag = LABEL_NEED;
		memcpy(new->k, lab, 8);
		new->v = ip;
		new->next = NULL;
		new->other = rest;
		head->other = new;
		return 0;
	}
	label_assoc* new = malloc(sizeof(label_assoc));
	new->tag = LABEL_NEED;
	memcpy(new->k, lab, 8);
	new->v = ip;
	new->next = *labels;
	*labels = new;
	new->other = NULL;
	return 0;
}

uint8_t parse_JMP(FILE* fd, byte* encoded, size_t* const size, label_assoc** labels){
#if (DEBUG==1)
	printf("JMP ");
#endif
	encoded[(*size)++] = JMP;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	assert_return(parse_metric(fd, c, encoded, size))
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	char lab[8] = "labelMax";
	size_t i = 0;
	while (c!=EOF && i<8 && !whitespace(c)){
		lab[i++] = c;
		c = fgetc(fd);
	}
	assert_return(whitespace(c));
	lab[i] = '\0';
	uint32_t label = seek_jump_label(labels, lab, *size);
	push_4bytes(encoded, size, label);
	return 1;
}

uint8_t parse_RET(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("RET ");
#endif
	encoded[(*size)++] = RET;
	return 1;
}

#define MATCH_INTERRUPT(tok) if (strcmp(#tok, op) == 0) { encoded[(*size)++] = tok; } else

uint8_t parse_interrupt(FILE* fd, char c, byte* encoded, size_t* const size){
	char op[] = "...";
	uint8_t i = 0;
	while (c != EOF && i < 3){
		op[i++] = c;
		c = fgetc(fd);
	}
	assert_return(c!=EOF)
#if (DEBUG==1)
	printf("%s ", op);
#endif
	MATCH_INTERRUPT(KBD)
	MATCH_INTERRUPT(OUT)
	{
#if (DEBUG==1)
		printf("(END)\n");
#endif
		encoded[(*size)++]=END;
	}
	return 1;

}

uint8_t parse_INT(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("INT ");
#endif
	encoded[(*size)++] = INT;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	return parse_interrupt(fd, c, encoded, size);
}

label_assoc* match_label(label_assoc* labels, char* lab, size_t ip, byte* encoded, uint8_t* err){
	label_assoc* head = labels;
	while (head != NULL){
		if (strcmp(head->k, lab) != 0){
			head = head->next;
			continue;
		}
		assert_error(head->tag != LABEL_MATCH)
		label_assoc* new = malloc(sizeof(label_assoc));
		new->tag = LABEL_MATCH;
		memcpy(new->k, lab, 8);
		new->v = ip;
		new->other = NULL;
		new->next = labels;
		do{
			for (size_t i = 0;i<4;++i){
				encoded[head->v+i] = ip >> (32-((i+1)*8)) & (0xFF);
			}
			head = head->other;
		} while (head != NULL);
		return new;
	}
	label_assoc* new = malloc(sizeof(label_assoc));
	new->tag = LABEL_MATCH;
	memcpy(new->k, lab, 8);
	new->v = ip;
	new->next = labels;
	new->other = NULL;
	return new;
}

uint8_t parse_label(FILE* fd, char c, byte* encoded, size_t* const size, char* op, label_assoc** labels){
	char label[8] = "labelMax";
	size_t i = 0;
	for (;i<3;++i){
		label[i] = op[i];
	}
	while (c!=EOF && i<8 && c!=':'){
		label[i++] = c;
		c = fgetc(fd);
	}
	assert_return(c==':')
	label[i] = '\0';
#if (DEBUG==1)
	printf("(label %s) ", label);
#endif
	uint8_t err = 0;
	*labels = match_label(*labels, label, *size, encoded, &err);
	assert_return(!err)
	encoded[(*size)++] = NOP;
	return 1;
}

#define MATCH_OPCODE(tok) if (strcmp(#tok, op) == 0){ return parse_##tok(fd, encoded, size); } else

uint8_t parse_opcode(FILE* fd, char c, byte* encoded, size_t* const size, label_assoc** labels){
	char op[] = "...";
	uint8_t i = 0;
	while (c != EOF && i < 3){
		op[i++] = c;
		c = fgetc(fd);
	}
	assert_return(c!=EOF)
	MATCH_OPCODE(NOP)
	MATCH_OPCODE(LDR)
	MATCH_OPCODE(LDI)
	MATCH_OPCODE(STR)
	MATCH_OPCODE(STI)
	MATCH_OPCODE(ADD)
	MATCH_OPCODE(MUL)
	MATCH_OPCODE(DIV)
	MATCH_OPCODE(MOD)
	MATCH_OPCODE(ASL)
	MATCH_OPCODE(ASR)
	MATCH_OPCODE(LSR)
	MATCH_OPCODE(PSH)
	MATCH_OPCODE(POP)
	MATCH_OPCODE(CMP)
	MATCH_OPCODE(RET)
	MATCH_OPCODE(INT)
	if (strcmp("JMP", op)==0){
		return parse_JMP(fd, encoded, size, labels);
	}
	else {
		return parse_label(fd, c, encoded, size, op, labels);
	}
	return 1;
}

void free_label_assoc(label_assoc* list){
	if (list == NULL){
		return;
	}
	free_label_assoc(list->next);
	free_label_assoc(list->other);
	free(list);
}

uint8_t assembler(int32_t argc, char** argv){
#if (DEBUG==1)
	printf("Assembler symbols:\n");
#endif
	assert_return((argc >= 5) && (strcmp(argv[3], "-o")==0))
	FILE* fd = fopen(argv[2], "r");
	assert_return(fd!=NULL)
	byte encoded[ROM_END-ROM_ADDRESS];
	label_assoc* label_list = NULL;
	size_t size = 0;
	char c;
	do {
		c = parse_spaces(fd);
		if (c==EOF){
			break;
		}
		if (!parse_opcode(fd, c, encoded, &size, &label_list)){
			printf("failed to parse instruction\n");
			break;
		}
	} while (c != EOF);
	fclose(fd);
	free_label_assoc(label_list);
	for (size_t i = 0;i<size;++i){
		printf("%.2x ", encoded[i]);
	}
	printf("\n");
	FILE* outfile = fopen(argv[4], "w");
	assert_return(outfile!=NULL)
	fwrite(encoded, 1, size, outfile);
	fclose(outfile);
}

uint8_t run_rom_image(int32_t argc, char** argv){
#if (DEBUG==1)
	printf("ROM symbols:\n");
#endif
	assert_return(argc >= 3)
	FILE* fd = fopen(argv[2], "rb");
	assert_return(fd!=NULL)
	byte rom[ROM_SIZE];
	size_t size = fread(ram+ROM_ADDRESS, sizeof(byte), ROM_SIZE, fd);
	assert_return(size < ROM_SIZE)
	run_rom();
	fclose(fd);
}

int main(int32_t argc, char** argv){
	//char* args[] = {"./vm", "-r", "out.rom"};
	//return !run_rom_image(3, args);
	//char* args[] = {"./vm", "-a", "program.asm", "-o", "out.rom"};
	//return !assembler(5, args);
	if (argc < 2){
		printf("Please provide arguments\n-a input.asm -o output.rom\n-r image.rom\n");
		return 0;
	}
	if (strcmp(argv[1], "-a")==0){
		return !assembler(argc, argv);
	}
	if (strcmp(argv[1], "-r")==0){
		return !run_rom_image(argc, argv);
	}
	return 0;
}
