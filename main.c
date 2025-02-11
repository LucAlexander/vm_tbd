#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <devices.h>
#include <SDL2/SDL.h>

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
	FP,
	PC,
	SR, //-zs- 4th, 1st bit unused for now
	REGISTER_COUNT
};

// opcodes
enum {
//  OPCODE | 1 BYTE     | 1 BYTE    | 1 BYTE     |
	NOP=0,
	LDW,//  00  dst src | 00000 ofs |            |
		//  01  dst src | 2 byte ofs literal     |
		//  10  dst     | 2 byte src address     |
		//  11  dst     | 2 byte src literal     |
	LDB,//  00  dst src | 00000 ofs |            |
		//  01  dst src | 2 byte ofs literal     |
		//  10  dst     | 2 byte src address     |
	STR,//  00  src dst | 00000 ofs |            |
		//  01  src dst | 2 byte ofs literal     |
		//  10  src     | 2 byte dst address     |
	STB,//  00  src dst | 00000 ofs |            |
		//  01  src dst | 2 byte ofs literal     |
		//  10  src     | 2 byte dst address     |
	LAR,//          dst | 4 bit src |            |
	ADD,//  0 m dst op1 | 2 byte int or reg op2  |
	SUB,//  ...         | ...                    |
	MUL,//  ...         | ...                    |
	DIV,//  ...         | ...                    |
	MOD,//  ...         | ...                    |
	LSL,//  ...         | ...                    |
	LSR,//  ...         | ...                    |
	AND,//  ...         | ...                    |
	ORR,//  ...         | ...                    |
	XOR,//  ...         | ...                    |
	COM,//  0000 m reg  | 2 byte literal or reg  |
	PSH,//  0000 m reg  | 2 byte literal         |
	POP,//  00000 reg   |           |            |
	CMP,//  00000 op1   | 00000 op2 |            |
	JMP,//  metric byte | 2 byte label address   |
	JSR,//  metric byte | 2 byte label address   |
	RET,//  pop to pc                            |
	INT//   interrupt   |           |            |
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
typedef uint32_t word;

typedef struct label_assoc{
	enum{LABEL_MATCH, LABEL_NEED} tag;
	char k[8];
	uint32_t v;
	struct label_assoc* next;
	struct label_assoc* other;
}label_assoc;

#define PROG_ADDRESS 0x0
#define PROG_SIZE 0x100000
#define PROG_END PROG_ADDRESS+PROG_SIZE
#define DEV_COUNT 0x8
#define DEV_SIZE 0x8
/* Device layout
 * Device Size (Word)
 * Device Type (Byte)
 * Device Pointer (3 Bytes)
 */
#define DEV_END	(DEV_COUNT*DEV_SIZE)+PROG_END
#define RAM_START DEV_END
#define RAM_SIZE 0xA00000
#define RAM_END RAM_START+RAM_SIZE
#define DEV_MEM 0x4000000
#define MEM_SIZE RAM_END+DEV_MEM

static word reg[REGISTER_COUNT];
static byte ram[MEM_SIZE];

#define NEXT ram[reg[PC]++]
#define LOAD ((NEXT<<8) + (NEXT))
#define READ(addr) ((ram[addr]<<8) + (ram[addr+1]))
#define WRITE(addr, val)\
	ram[addr] = (val>>24) & (0xFF);\
	ram[addr+1] = (val>>16) & (0xFF);\
	ram[addr+2] = (val>>8) & (0xFF);\
	ram[addr+3] = (val) & (0xFF);\


#define LOAD_WORD(r, addr)\
	reg[r] = ((ram[addr] << 24)\
           | (ram[addr+1] << 16)\
           | (ram[addr+2] << 8)\
           | (ram[addr+3]));\


void stack_push(word value){
	for (uint8_t i = 0;i<4;++i){
		ram[reg[ST]--] = (value >> (0x8*i)) & 0xFF;
#if (DEBUG == 1)
		printf("push [%x] ]%u\n", reg[ST]+1, ram[reg[ST]+1]);
#endif
	}
}

word stack_pop(){
	word v = 0;
	for (uint8_t i = 0;i<4;++i){
#if (DEBUG == 1)
		printf("pop [%x]: %u\n", reg[ST]+1, ram[reg[ST]+1]);
#endif
		v += ram[++reg[ST]] << (0x8*(3-i));
	}
#if (DEBUG == 1)
		printf("%u\n", v);
#endif
	return v;
}

void to_stdout(){
	word n = reg[R1];
	for (uint32_t i=0;i<n;++i){
		putchar(ram[reg[R0]+i]);
	}
}

void handle_interrupt(byte intr){
	for (size_t i = PC;i<REGISTER_COUNT;++i){
		stack_push(reg[i]);
#if (DEBUG==1)
		printf("<- %u (%u)\n", i, reg[i]);
#endif
	}
	switch (intr){
	case END:
#if (DEBUG==1)
		printf("END\n");
#endif
		reg[PC]=PROG_END;
		stack_push(reg[R0]);
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
		reg[i] = stack_pop();
#if (DEBUG==1)
		printf("%u <- %u\n", i, reg[i]);
#endif
	}
	NEXT;
	NEXT;
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

void set_status(word val){
	reg[SR] = ((val==0) << 2)
			| (val > 0)
			| (0);
}

void progress(){
	byte a, b, m, src, dst, op1, op2;
	int32_t offset, src_val, x, y;
	word src_address, dst_address, preserve;
	byte opcode = NEXT;
	switch (opcode){
	case LDW:
		a = NEXT;
		m = (a>>6) & 0x3;
		dst = (a>>3) & 0x7;
		switch(m){
		case 0:
			src_address = reg[a & 0x7] + reg[(NEXT) & 0x7];
			LOAD_WORD(dst, src_address)
			NEXT;
			break;
		case 1:
			src_address = reg[a & 0x7]+(LOAD);
#if (DEBUG == 1)
		printf("%x offset from %u (%x)\n", src_address, a&0x7, reg[a&0x7]);
#endif
			LOAD_WORD(dst, src_address)
			break;
		case 2:
			src_address = ram[LOAD];
			LOAD_WORD(dst, src_address)
			break;
		case 3:
			reg[dst] = LOAD;
			break;
		}
#if (DEBUG == 1)
		printf("LDW %u <- %x\n", dst, reg[dst]);
#endif
		break;
	case LDB:
		a = NEXT;
		m = (a>>6) & 0x3;
		dst = (a>>3) & 0x7;
		switch(m){
		case 0:
			src_address = reg[a & 0x7] + reg[(NEXT) & 0x7];
			reg[dst] = ram[src_address];
			NEXT;
			break;
		case 1:
			src_address = reg[a & 0x7]+(LOAD);
			reg[dst] = ram[src_address];
			break;
		case 2:
			reg[dst] = ram[LOAD];
			break;
		}
#if (DEBUG == 1)
		printf("LDB %u <- %x\n", dst, reg[dst]);
#endif
		break;
	case STR:
		a = NEXT;
		m = (a>>6) & 0x3;
		preserve = reg[(a>>3) & 0x7];
		switch(m){
		case 0:
			dst_address = reg[a & 0x7]+reg[(NEXT)&0x7];
			NEXT;
			break;
		case 1:
			dst_address = reg[a & 0x7]+(LOAD);
			break;
		case 2:
			dst_address = LOAD;
			break;
		}
		WRITE(dst_address, preserve)
#if (DEBUG == 1)
		printf("STR %u (%x) -> %x\n",(a>>3) & 0x7, preserve, dst_address);
#endif
		break;
	case STB:
		a = NEXT;
		m = (a>>6) & 0x3;
		preserve = reg[(a>>3) & 0x7];
		switch(m){
		case 0:
			dst_address = reg[a & 0x7]+reg[(NEXT)&0x7];
			NEXT;
			break;
		case 1:
			dst_address = reg[a & 0x7]+(LOAD);
			break;
		case 2:
			dst_address = LOAD;
			break;
		}
		ram[dst_address] = preserve & 0xFF;
#if (DEBUG == 1)
		printf("STB %u (%x) -> %x\n",(a>>3) & 0x7, preserve & 0xFF, dst_address);
#endif
		break;
	case LAR:
		dst = NEXT;
		src = NEXT;
		NEXT;
		reg[dst] = reg[src];
#if (DEBUG == 1)
		printf("LAR %u <- %u(%x)\n", dst, src, reg[src]);
#endif
		break;
	case ADD:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]+src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("ADD %u (%u) <- %u + %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case SUB:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]-src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("SUB %u (%u) <- %u - %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case MUL:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]*src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("MUL %u (%u) <- %u * %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case DIV:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]/src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("DIV %u (%u) <- %u / %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case MOD:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]%src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("MOD %u (%u) <- %u % %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case LSL:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]<<src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("LSL %u (%u) <- %u << %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case LSR:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1]>>src_val;
		set_status(reg[dst]);
#if (DEBUG == 1)
		printf("LSR %u (%u) <- %u >> %u \n", dst, reg[dst], reg[op1], src_val);
#endif
		break;
	case AND:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1] & src_val;
		set_status(reg[dst]);
		break;
	case ORR:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1] | src_val;
		set_status(reg[dst]);
		break;
	case XOR:
		a = NEXT;
		m = a >> 6;
		dst = (a >> 3) & 0x7;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[dst] = reg[op1] ^ src_val;
		set_status(reg[dst]);
		break;
	case COM:
		a = NEXT;
		m = a >> 3;
		op1 = a & 0x7;
		if (m){
			src_val = LOAD;
		}
		else{
			src_val = reg[NEXT];
			NEXT;
		}
		reg[op1] = ~src_val;
		set_status(reg[op1]);
		break;
	case PSH:
		a = NEXT;
		m = a > 3;
		if (m){
			src_val = reg[a & 0x7];
			NEXT;
			NEXT;
		}
		else{
			src_val = LOAD;
		}
#if (DEBUG == 1)
		printf("PSH [%x] <- %u\n", reg[ST], src_val);
#endif
		stack_push(src_val);
		break;
	case POP:
		a = NEXT;
		dst = a & 0x7;
		reg[dst] = stack_pop();
		NEXT;
		NEXT;
#if (DEBUG == 1)
		printf("POP %u <- %u\n", dst, reg[dst]);
#endif
		break;
	case CMP:
		x = reg[NEXT];
		y = reg[NEXT];
		reg[SR] = ((x==y) << 2)
				   | ((x>y) << 1)
				   | (0);
		NEXT;
#if (DEBUG == 1)
		printf("CMP %u =? %u\n", x, y);
#endif
		break;
	case JMP:
#if (DEBUG == 1)
		printf("JSR\n");
#endif
		if (check_metric(NEXT & 0x7)){
			reg[PC] = LOAD;
			break;
		}
		NEXT;
		NEXT;
		break;
	case JSR:
#if (DEBUG == 1)
		printf("JSR\n");
#endif
		if (check_metric(NEXT & 0x7)){
			stack_push(reg[FP]);
			stack_push(reg[PC]+2);
			reg[FP] = reg[ST];
			reg[PC] = LOAD;
			break;
		}
		NEXT;
		NEXT;
		break;
	case RET:
		x = stack_pop();
		reg[ST] = reg[FP];
		reg[PC] = stack_pop();
		reg[FP] = stack_pop();
		stack_push(x);
#if (DEBUG == 1)
		printf("RET -> %u\n", reg[PC]);
#endif
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
		NEXT;
		NEXT;
		NEXT;
		break;	  
	default:
#if (DEBUG == 1)
		printf("invalid instruction opcode %.2x\n", opcode);
#endif
		break;
	}
}

void load_rom(word address, size_t len){
	for (size_t i = 0;i<len;++i){
		ram[PROG_ADDRESS+i] = ram[address+i];
	}
}

#define DISPLAY_REG(r) printf("[%s]: %x (%u)\033[0m\n", #r, reg[r], reg[r])

void display_machine(){
	printf("\033[2J");
	printf("\033[H\033[1m");
	word offset = 64;
	if (PROG_END-reg[PC] < offset){
		offset = PROG_END-reg[PC];
	}
	printf("                                                                    Program Counter\033[0m\n");
	for (word adr = reg[PC];adr < reg[PC]+offset;adr+=4){
		if (adr==reg[PC]){
			printf("\033[1;33m");
		}
		printf("                                                                    [%x]: ", adr);
		for (uint8_t i = 0;i<4;++i){
			printf("%02x ", ram[adr+i]);
		}
		printf("\033[0m\n");
	}
	printf("\033[H\033[1m");
	offset = 64;
	if (reg[ST] >  RAM_SIZE - offset){
		offset = RAM_SIZE-reg[ST];
	}
	printf("                              Stack\033[0m\n");
	for (word adr = reg[ST]+offset;adr >= reg[ST];--adr){
		if (adr==reg[FP]){
			printf("\033[1;32m");
		}
		printf("                              [%x]: %x\033[0m\n", adr, ram[adr]);
	}
	printf("\033[H\033[1m");
	printf("CPU Registers\033[0m\n");
	DISPLAY_REG(R0);
	DISPLAY_REG(R1);
	DISPLAY_REG(R2);
	DISPLAY_REG(R3);
	DISPLAY_REG(R4);
	DISPLAY_REG(R5);
	DISPLAY_REG(R6);
	DISPLAY_REG(R7);
	DISPLAY_REG(ST);
	printf("\033[1;32m");
	DISPLAY_REG(FP);
	printf("\033[1;33m");
	DISPLAY_REG(PC);
	DISPLAY_REG(SR);
}

void run_rom(uint8_t debug){
	reg[PC] = PROG_ADDRESS;
	reg[ST] = RAM_SIZE-1;
	reg[FP] = reg[ST];
	while (reg[PC] != PROG_END) {
		if (debug){
			getc(stdin);
			display_machine();
		}
		progress();
	}
	printf("INFO rom exited with code %x\n", stack_pop());
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

byte parse_aux_register(FILE* fd, char c, uint8_t* err){
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
	MATCH_REGISTER(ST)
	MATCH_REGISTER(SR)
	MATCH_REGISTER(FP)
	{
		*err = 1;
		return 0;
	}
}

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
	*size += 3;
#if (DEBUG==1)
	printf("NOP ");
#endif
	return 1;
}

void push_2bytes(byte* encoded, size_t* const size, word address){
		for (uint8_t i = 0;i<2;++i){
			encoded[(*size)++] = (address >> ((1-i)*0x8)) & 0xFF;
#if (DEBUG==1)
			printf("%u ", (address >> ((1-i)*0x8))&0xFF);
#endif
		}
}

uint8_t parse_LAR(FILE* const fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = LAR;
#if (DEBUG==1)
	printf("LAR ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte r = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte s = parse_aux_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = r;
	encoded[(*size)++] = s;
	*size += 1;
	return 1;
}

uint8_t parse_LDW(FILE* const fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = LDW;
#if (DEBUG==1)
	printf("LDW ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte r = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	if (c=='&'){
		word address = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x2 << 6) | (r << 3);
		push_2bytes(encoded, size, address);
		return 1;
	}
	else if (c=='#'){
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x3 << 6) | (r << 3);
		push_2bytes(encoded, size, value);
		return 1;
	}
	byte s = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!= '&')
	if (c == '#'){
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x1 << 6) | (r << 3) | s;
		push_2bytes(encoded, size, value);
		return 1;
	}
	byte offset = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = (r<<3) | s;
	encoded[(*size)++] = offset;
	*size += 1;
	return 1;
}

uint8_t parse_LDB(FILE* const fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = LDB;
#if (DEBUG==1)
	printf("LDB ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte r = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!='#')
	if (c=='&'){
		word address = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x2 << 6) | (r << 3);
		push_2bytes(encoded, size, address);
		return 1;
	}
	byte s = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!= '&')
	if (c == '#'){
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x1 << 6) | (r << 3) | s;
		push_2bytes(encoded, size, value);
		return 1;
	}
	byte offset = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = (r<<3) | s;
	encoded[(*size)++] = offset;
	*size += 1;
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
	byte r = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!='#')
	if (c=='&'){
		word address = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x2 << 6) | (r << 3);
		push_2bytes(encoded, size, address);
		return 1;
	}
	byte s = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!= '&')
	if (c == '#'){
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x1 << 6) | (r << 3) | s;
		push_2bytes(encoded, size, value);
		return 1;
	}
	byte offset = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = (r<<3) | s;
	encoded[(*size)++] = offset;
	*size += 1;
	return 1;
}

uint8_t parse_STB(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = STB;
#if (DEBUG==1)
	printf("STB ");
#endif
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte r = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!='#')
	if (c=='&'){
		word address = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x2 << 6) | (r << 3);
		push_2bytes(encoded, size, address);
		return 1;
	}
	byte s = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF && c!= '&')
	if (c == '#'){
		int32_t value = parse_numeric(fd, &err);
		assert_return(!err)
		encoded[(*size)++] = (0x1 << 6) | (r << 3) | s;
		push_2bytes(encoded, size, value);
		return 1;
	}
	byte offset = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = (r<<3) | s;
	encoded[(*size)++] = offset;
	*size += 1;
	return 1;
}

uint8_t parse_alu_op(FILE* fd, byte* encoded, size_t* const size){
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte dst = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte op1 = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	if (c=='#'){
		int32_t val = parse_numeric(fd, &err);
		assert_return(!err);
		encoded[(*size)++] = (1<<6) | (dst<<3) | op1;
		push_2bytes(encoded, size, val);
		return 1;
	}
	byte op2 = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = (dst<<3) | op1;
	encoded[(*size)++] = op2;
	*size += 1;
	return 1;
}

uint8_t parse_ADD(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = ADD;
#if (DEBUG==1)
	printf("ADD ");
#endif
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_SUB(FILE* fd, byte* encoded, size_t* const size){
	encoded[(*size)++] = SUB;
#if (DEBUG==1)
	printf("SUB ");
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

uint8_t parse_LSL(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("LSL ");
#endif
	encoded[(*size)++] = LSL;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_LSR(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("LSR ");
#endif
	encoded[(*size)++] = LSR;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_AND(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("AND ");
#endif
	encoded[(*size)++] = AND;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_ORR(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("ORR ");
#endif
	encoded[(*size)++] = ORR;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_XOR(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("XOR ");
#endif
	encoded[(*size)++] = XOR;
	return parse_alu_op(fd, encoded, size);
}

uint8_t parse_COM(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("COM ");
#endif
	encoded[(*size)++] = COM;
	char c = parse_spaces(fd);
	uint8_t err = 0;
	assert_return(c!=EOF)
	byte dst = parse_register(fd, c, &err);
	assert_return(!err)
	c = parse_spaces(fd);
	assert_return(c!= EOF)
	if (c=='#'){
		encoded[(*size)++] = (1<<3) | dst;
		int32_t val = parse_numeric(fd, &err);
		assert_return(!err)
		push_2bytes(encoded, size, val);
		return 1;
	}
	encoded[(*size)++] = dst;
	byte src = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = src;
	*size += 1;
	return 1;
}

uint8_t parse_PSH(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("PSH ");
#endif
	encoded[(*size)++] = PSH;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	if (c=='#'){
		encoded[(*size)++] = 0;
		int32_t val = parse_numeric(fd, &err);
		assert_return(!err);
		push_2bytes(encoded, size, val);
		return 1;
	}
	byte src = parse_register(fd, c, &err);
	assert_return(!err)
	encoded[(*size)++] = (1<<3) | src;
	*size += 2;
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
	assert_return((c!=EOF) && (!err))
	encoded[(*size)++] = src;
	*size += 2;
	return 1;
}

uint8_t parse_CMP(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("CMP ");
#endif
	encoded[(*size)++] = CMP;
	char c = parse_spaces(fd);
	assert_return(c!=EOF)
	uint8_t err = 0;
	byte op1 = parse_register(fd, c, &err);
	assert_return((c!=EOF) && (!err))
	c = parse_spaces(fd);
	assert_return(c!=EOF)
	byte op2 = parse_register(fd, c, &err);
	assert_return((c!=EOF) && (!err))
	encoded[(*size)++] = op1;
	encoded[(*size)++] = op2;
	*size += 1;
	return 1;
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

uint8_t parse_JSR(FILE* fd, byte* encoded, size_t* const size, label_assoc** labels){
#if (DEBUG==1)
	printf("JSR ");
#endif
	encoded[(*size)++] = JSR;
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
	word label = seek_jump_label(labels, lab, *size);
	push_2bytes(encoded, size, label);
	return 1;
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
	word label = seek_jump_label(labels, lab, *size);
	push_2bytes(encoded, size, label);
	return 1;
}

uint8_t parse_RET(FILE* fd, byte* encoded, size_t* const size){
#if (DEBUG==1)
	printf("RET ");
#endif
	encoded[(*size)++] = RET;
	*size += 3;
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
	*size += 2;
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
			for (size_t i = 0;i<2;++i){
				encoded[head->v+i] = ip >> ((1-i)*0x8) & 0xFF;
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
	*size += 3;
	return 1;
}

uint8_t parse_comment(FILE* fd, char c){
	while (c != EOF && c != '\n'){
		c = fgetc(fd);
	}
	return c != EOF;
}

#define MATCH_OPCODE(tok) if (strcmp(#tok, op) == 0){ return parse_##tok(fd, encoded, size); } else

uint8_t parse_opcode(FILE* fd, char c, byte* encoded, size_t* const size, label_assoc** labels){
	if (c==';'){
		return parse_comment(fd, c);
	}
	char op[] = "...";
	uint8_t i = 0;
	while (c != EOF && i < 3){
		op[i++] = c;
		c = fgetc(fd);
	}
	assert_return(c!=EOF)
	MATCH_OPCODE(NOP)
	MATCH_OPCODE(LDW)
	MATCH_OPCODE(LDB)
	MATCH_OPCODE(STR)
	MATCH_OPCODE(STB)
	MATCH_OPCODE(LAR)
	MATCH_OPCODE(ADD)
	MATCH_OPCODE(SUB)
	MATCH_OPCODE(MUL)
	MATCH_OPCODE(DIV)
	MATCH_OPCODE(MOD)
	MATCH_OPCODE(LSL)
	MATCH_OPCODE(LSR)
	MATCH_OPCODE(AND)
	MATCH_OPCODE(ORR)
	MATCH_OPCODE(XOR)
	MATCH_OPCODE(COM)
	MATCH_OPCODE(PSH)
	MATCH_OPCODE(POP)
	MATCH_OPCODE(CMP)
	MATCH_OPCODE(RET)
	MATCH_OPCODE(INT)
	if (strcmp("JMP", op)==0){
		return parse_JMP(fd, encoded, size, labels);
	}
	else if (strcmp("JSR", op)==0){
		return parse_JSR(fd, encoded, size, labels);
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

uint8_t parse_include(FILE* fd, byte* encoded, size_t* const size, label_assoc** label_list);

uint8_t parse_body(FILE* fd, byte* encoded, size_t* const size, label_assoc** label_list){
	char c = fgetc(fd);
	while(c=='+'){
		if (!parse_include(fd, encoded, size, label_list)){
			printf("failed to parse inclusion\n");
			break;
		}
		c = parse_spaces(fd);
		if (c==EOF){
			break;
		}
	}
	while (c != EOF){
		if (whitespace(c)){
			c = parse_spaces(fd);
		}
		if (c==EOF){
			break;
		}
		if (!parse_opcode(fd, c, encoded, size, label_list)){
			printf("failed to parse instruction\n");
			fclose(fd);
			return 0;
		}
		c = fgetc(fd);
	}
	fclose(fd);
	return 1;
}

uint8_t parse_include(FILE* fd, byte* encoded, size_t* const size, label_assoc** label_list){
	char filename[] = "################.asm";
	size_t index = 0;
	char c = fgetc(fd);
	while ((!whitespace(c)) && index < 16){
		filename[index++] = c;
		assert_return(c!=EOF)
		c = fgetc(fd);
	}
	filename[index++] = '.';
	filename[index++] = 'a';
	filename[index++] = 's';
	filename[index++] = 'm';
	filename[index++] = '\0';
	printf("%s\n", filename);
	FILE* new_fd = fopen(filename, "r");
	assert_return(new_fd!=NULL)
	return parse_body(new_fd, encoded, size, label_list);
}

uint8_t assembler(int32_t argc, char** argv){
#if (DEBUG==1)
	printf("Assembler symbols:\n");
#endif
	assert_return((argc >= 5) && (strcmp(argv[3], "-o")==0))
	FILE* fd = fopen(argv[2], "r");
	assert_return(fd!=NULL)
	byte encoded[PROG_SIZE] = {0};
	label_assoc* label_list = NULL;
	size_t size = 0;
	parse_body(fd, encoded, &size, &label_list);
	free_label_assoc(label_list);
	for (size_t i = 0;i<size;++i){
		printf("%.2x ", encoded[i]);
		if (i%4 == 3){
			printf("\n");
		}
	}
	printf("\n");
	FILE* outfile = fopen(argv[4], "w");
	assert_return(outfile!=NULL)
	fwrite(encoded, 1, size, outfile);
	fclose(outfile);
}

uint8_t setup_devices(){
	word address = RAM_END;
	for (int i = 0;i<DEV_COUNT;++i){
		word device_ptr = PROG_END+(i*DEV_SIZE);
		word size = 0;
		switch (device_config[i].type){
		case DEV_NONE:
		default:
			return;
		case DEV_SCREEN:
			size = device_config[i].data.screen.w*device_config[i].data.screen.h;
		case DEV_KEYBOARD:
			size = device_config[i].data.keyboard.keys;
		case DEV_MOUSE:
			size = 8;
		}
		assert_return(address + size < MEM_SIZE)
		WRITE(device_ptr, size)
		word location = address | (device_config[i].type << 24);
		WRITE(device_ptr+4, location)
		address += size;
	}
}

uint8_t run_rom_image(int32_t argc, char** argv){
#if (DEBUG==1)
	printf("symbols:\n");
#endif
	assert_return(setup_devices())
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_CreateWindowAndRenderer(512, 512, SDL_WINDOW_OPENGL, &window, &renderer);
	SDL_SetWindowTitle(window, "VM");
	SDL_Init(SDL_INIT_EVERYTHING);
	assert_return(argc >= 3)
	FILE* fd = fopen(argv[2], "rb");
	assert_return(fd!=NULL)
	byte rom[PROG_SIZE];
	size_t size = fread(ram+PROG_ADDRESS, sizeof(byte), PROG_SIZE, fd);
	assert_return(size < PROG_SIZE)
	uint8_t debug = 0;
	if (argc >= 4){
		if (strcmp(argv[3], "-g")==0){
			debug = 1;
		}
	}
	run_rom(debug);
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	SDL_Quit();
	fclose(fd);
}

int main(int32_t argc, char** argv){
	printf("%x\n", RAM_START);
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
