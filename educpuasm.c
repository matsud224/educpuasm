#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>

#define ERROR(...) do{\
		printf("ERROR: " __VA_ARGS__);\
		puts("");\
		exit(-1);\
	}while(0)

#define OUTBUF_SIZE 1024
#define SYMTABLE_SIZE 256
#define UNRESOLVEDTABLE_SIZE 512

#define NO_DATA 0
#define DIRECTIVE_DEFINE 0
#define DIRECTIVE_TEXT 1
#define DIRECTIVE_DATA 2
#define DIRECTIVE_BYTE 3
#define BYTE_DATA 3

typedef enum _token_type {
	NULL_TOKEN,
	UNKNOWN_SYMBOL,
	OPCODE, REGISTER,
	COMMA, COLON, LBRACKET, RBRACKET, LPAREN, RPAREN, PLUS,
	NUM,
	DIRETIVE,
	EOL, END
} token_type;

typedef struct _byte_data {
	uint8_t state;
	uint8_t value;
} byte_data;

typedef union _extrainfo {
	uint16_t value;
	char *str;
} extra_info;

typedef struct _symbol {
	const char *name;
	uint16_t value;
} symbol;

int current_line = 1;
int current_index = 0;
uint8_t current_address = 0;
byte_data outbuf[OUTBUF_SIZE];

symbol symbol_table[SYMTABLE_SIZE+1] = {{"_len", SYMTABLE_SIZE}};
symbol unresolved_table[UNRESOLVEDTABLE_SIZE+1] = {{"_len", UNRESOLVEDTABLE_SIZE}};

const symbol directive_table[] = {
	{"_len", 4},
	{".text", DIRECTIVE_TEXT},
	{".data", DIRECTIVE_DATA},
	{".define", DIRECTIVE_DEFINE},
	{".byte", DIRECTIVE_BYTE}
};

#define OPCODE_BASE_MASK 0xff
#define HAVE_OPERAND_A 0x100
#define HAVE_OPERAND_B 0x200
#define OPERAND_B_ST 0x400
#define OPERAND_B_IMM_ONLY 0x800
#define OPERAND_A_B_SWAP 0x1000

//operandA  ACC:+0, IX:+8
//operandB  ACC:+0, IX:+1, d:+2, [d]:+4, (d):+5, [IX+d]:+6, (IX+d):+7
const symbol opcode_table[] = {
	{"_len",41},
	{"nop",	0x00},
	{"hlt",	0x0f},
	{"halt",0x0f},
	{"out",	0x10},
	{"in",	0x1f},
	{"rcf",	0x20},
	{"scf",	0x2f},
	{"ld",	0x60 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"st", 	0x70 | HAVE_OPERAND_A | HAVE_OPERAND_B | OPERAND_B_ST | OPERAND_A_B_SWAP},
	{"add",	0xb0 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"adc",	0x90 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"sub",	0xa0 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"sbc",	0x80 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"cmp",	0xf0 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"and",	0xe0 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"or",	0xd0 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"eor",	0xc0 | HAVE_OPERAND_A | HAVE_OPERAND_B},
	{"sra",	0x40 | HAVE_OPERAND_A},
	{"sla",	0x41 | HAVE_OPERAND_A},
	{"srl",	0x42 | HAVE_OPERAND_A},
	{"sll",	0x43 | HAVE_OPERAND_A},
	{"rra",	0x44 | HAVE_OPERAND_A},
	{"rla",	0x45 | HAVE_OPERAND_A},
	{"rrl",	0x46 | HAVE_OPERAND_A},
	{"rll",	0x47 | HAVE_OPERAND_A},
	{"ba",	0x2e | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bnz",	0x2f | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bzp",	0x30 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bp",	0x31 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bni",	0x32 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bnc",	0x33 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bge",	0x34 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bgt",	0x35 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bvf",	0x36 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bz",	0x37 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bn",	0x38 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bzn",	0x39 | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bno",	0x3a | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"bc",	0x3b | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"blt",	0x3c | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
	{"ble",	0x3d | HAVE_OPERAND_B | OPERAND_B_IMM_ONLY},
};

const symbol register_table[] = {
	{"_len",	2},
	{"acc",		0},
	{"ix", 		1}
};

void table_add(symbol table[], const char *name, uint16_t value) {
	for(int i=1; i<=table[0].value; i++){
		if(!table[i].name){
			table[i].name = name;
			table[i].value = value;
			return;
		}
	}
	ERROR("Table is full.\n");
}

int strcmp_ignore_case(const char *s1, const char *s2) {
	while(*s1!='\0' && *s2!='\0' && tolower(*s1)==tolower(*s2)){
		s1++; s2++;
	}
	return *s1 - *s2;
}

int table_find(const symbol table[], const char *name) {
	for(int i=1; i<=table[0].value; i++)
		if(table[i].name && strcmp_ignore_case(table[i].name, name) == 0)
			return table[i].value;
	return -1;
}

token_type returned_token_type = NULL_TOKEN;
extra_info returned_token_extra;
void unget_token(token_type tt, extra_info *extra) {
	returned_token_type = tt;
	returned_token_extra = *extra;
}

token_type get_token(FILE *fp, extra_info *extra) {
	static char buf[64];
	int c, is_directive = 0;

	if(returned_token_type != NULL_TOKEN){
		if(extra) *extra = returned_token_extra;
		token_type tt = returned_token_type;
		returned_token_type = NULL_TOKEN;
		return tt;
	}

	while(isblank(c=fgetc(fp)) || c == ';')
		if(c == ';'){
			while(fgetc(fp) != '\n');
			current_line++;
			return EOL;
		}

	switch(c){
	case '\n':
		current_line++;
		return EOL;
	case EOF:
		return END;
	case ',':
		return COMMA;
	case ':':
		return COLON;
	case ']':
		return RBRACKET;
	case '[':
		return LBRACKET;
	case ')':
		return RPAREN;
	case '(':
		return LPAREN;
	case '+':
		return PLUS;
	case '.':
		is_directive = 1;
		break;
	}

	buf[0] = c;
	char *ptr;
	for(ptr = buf+1; isalnum(*ptr = fgetc(fp)); ptr++);
	ungetc(*ptr, fp);
	*ptr = '\0';

	if(isdigit(buf[0])){
		if(extra){
			if(buf[0] == '0' && buf[1] == 'x'){
				sscanf(buf+2, "%hx", &extra->value);
			}else if(tolower(*(ptr-1))=='h'){
				*(ptr-1) = '\0';
				sscanf(buf, "%hx", &extra->value);
			}else{
				sscanf(buf, "%hd", &extra->value);
			}
			if(extra->value > 255)
				ERROR("integer constant too big at line #%d", current_line);
		}
		return NUM;
	}

	if(is_directive){
		if(extra) extra->value = table_find(directive_table, buf);
		return DIRETIVE;
	}else{
		int retval;
		if((retval = table_find(opcode_table, buf)) >= 0){
			if(extra) extra->value = retval;
			return OPCODE;
		}else if((retval = table_find(register_table, buf)) >= 0){
			if(extra) extra->value = retval;
			return REGISTER;
		}else if((retval = table_find(symbol_table, buf)) >= 0){
			if(extra) extra->value = retval;
			return NUM;
		}else{
			if(extra) extra->str = strdup(buf);
			return UNKNOWN_SYMBOL;
		}
	}
}

void parse_eol(FILE *fp) {
	token_type tt = get_token(fp, NULL);
	if(tt != EOL && tt!= END)
		ERROR("invalid token next to statement at line #%d", current_line);
}

int parse_operand_a(FILE *fp) {
	extra_info info;
	if(get_token(fp, &info) == REGISTER){
		return 8*info.value;
	}else{
		ERROR("operand A required at line #%d.", current_line);
	}
}

int parse_operand_b(FILE *fp, uint16_t opcode, int *byte2) {
	extra_info info;
	token_type tt = get_token(fp, &info);
	switch(tt){
	case REGISTER:
		if(opcode & OPERAND_B_IMM_ONLY || opcode & OPERAND_B_ST)
			ERROR("bad operand B type at line #%d.", current_line);
		*byte2 = -1;
		return info.value;
	case NUM:
		if(opcode & OPERAND_B_ST)
			ERROR("bad operand B type at line #%d.", current_line);
		*byte2 = info.value;
		return 2;
	case UNKNOWN_SYMBOL:
		if(opcode & OPERAND_B_ST)
			ERROR("bad operand B type at line #%d.", current_line);
		*byte2 = 0;
		table_add(unresolved_table, info.str, current_index+1);
		return 2;
	case LBRACKET:
	case LPAREN:
	{
		int retval;
		token_type close_tok = tt+1;
		if(opcode & OPERAND_B_IMM_ONLY)
			ERROR("bad operand B type at line #%d.", current_line);
		switch(get_token(fp, &info)){
		case NUM:
			*byte2 = info.value;
			retval = 4 + (tt==LPAREN);
			break;
		case UNKNOWN_SYMBOL:
			*byte2 = 0;
			table_add(unresolved_table, info.str, current_index+1);
			retval = 4 + (tt==LPAREN);
			break;
		case REGISTER:
		{
			//[IX+d]
			token_type immtype;
			if(info.value != 1)
				ERROR("IX relative only at line #%d.", current_line);
			if(get_token(fp, NULL) != PLUS)
				ERROR("syntax error at line #%d.", current_line);
			immtype = get_token(fp, &info);
			if(immtype == NUM)
				*byte2 = info.value;
			else if(immtype == UNKNOWN_SYMBOL){
				*byte2 = 0;
				table_add(unresolved_table, info.str, current_index+1);
			}else
				ERROR("syntax error at line #%d.", current_line);
			retval = 6 + (tt==LPAREN);
			break;
		}
		default:
			ERROR("syntax error at line #%d.", current_line);
		}
		if(get_token(fp, NULL) != close_tok)
			ERROR("syntax error at line #%d.", current_line);
		return retval;
	}
	default:
		ERROR("operand B required at line #%d.", current_line);
	}
}

int parse_operands(FILE *fp, uint16_t opcode, int *byte2) {
	int opcode_add = 0;
	*byte2 = -1;
	if(opcode & HAVE_OPERAND_A){
		if(opcode & OPERAND_A_B_SWAP)
			opcode_add += parse_operand_b(fp, opcode, byte2);
		else
			opcode_add += parse_operand_a(fp);
	}
	if(opcode & HAVE_OPERAND_B){
		if(opcode & HAVE_OPERAND_A && opcode & HAVE_OPERAND_B && get_token(fp, NULL) != COMMA){
			ERROR("expected comma next to register name at line #%d.", current_line);
		}
		if(opcode & OPERAND_A_B_SWAP)
			opcode_add += parse_operand_a(fp);
		else
			opcode_add += parse_operand_b(fp, opcode, byte2);
	}
	return opcode_add;
}

void parse_label(FILE *fp, const char *str) {
	table_add(symbol_table, str, current_address);
	if(get_token(fp, NULL) != COLON)
		ERROR("expected colon next to label \'%s\' at line #%d.", str, current_line);
}

void parse_opcode(FILE *fp, uint16_t opcode) {
	int byte2;
	outbuf[current_index].state = BYTE_DATA;
	outbuf[current_index].value = (opcode & OPCODE_BASE_MASK) + parse_operands(fp, opcode, &byte2);
	current_index++;
	current_address++;
	if(byte2 >= 0){
		outbuf[current_index].state = BYTE_DATA;
		outbuf[current_index].value = byte2;
		current_index++;
		current_address++;
	}
	parse_eol(fp);
}

void parse_directive(FILE *fp, uint16_t dirtype) {
	token_type tt;
	extra_info info;
	switch(dirtype){
	case DIRECTIVE_DEFINE:
		tt = get_token(fp, &info);
		if(tt == UNKNOWN_SYMBOL){
			char *symname = info.str;
			if(get_token(fp, &info) != NUM)
				ERROR("expected number at line #%d.", current_line);
			table_add(symbol_table, symname, info.value);
		}else if(tt == NUM){
			ERROR("redefinition of symbol at line #%d.", current_line);
		}else{
			ERROR("syntax error at line #%d.", current_line);
		}
		break;
	case DIRECTIVE_TEXT:
	case DIRECTIVE_DATA:
		if(get_token(fp, &info) != NUM)
			ERROR("expected start address at line #%d.", current_line);
		outbuf[current_index].state = dirtype;
		outbuf[current_index++].value = info.value;
		current_address = info.value;
		break;
	case DIRECTIVE_BYTE:
		while((tt=get_token(fp, &info)) == NUM){
			outbuf[current_index].state = BYTE_DATA;
			outbuf[current_index++].value = info.value;
			current_address++;
		}
		unget_token(tt,&info);
		break;
	}
	parse_eol(fp);
}

void resolve_symbols() {
	int i = 1, size = unresolved_table[0].value;
	int addr;
	while(i<=size && unresolved_table[i].name){
		if((addr=table_find(symbol_table, unresolved_table[i].name)) < 0)
			ERROR("undefined symbol \'%s\'.", unresolved_table[i].name);
		else
			outbuf[unresolved_table[i].value].value = addr;
		i++;
	}
}

void print_codes(FILE *fp) {
	for(int i=0; i<OUTBUF_SIZE && outbuf[i].state!=NO_DATA; i++){
		switch(outbuf[i].state){
		case BYTE_DATA:
			fprintf(fp, "%02hhx ", outbuf[i].value);
			break;
		case DIRECTIVE_TEXT:
			fprintf(fp, "\n.text %02hhx\n", outbuf[i].value);
			break;
		case DIRECTIVE_DATA:
			fprintf(fp, "\n.data %02hhx\n", outbuf[i].value);
			break;
		default:
			ERROR("internal error...");
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	token_type type;
	extra_info info;

	if(argc != 3)
		ERROR("Usage: educpuasm <input file> <output file>");

	FILE *input = fopen(argv[1], "r");
	if(input == NULL)
		ERROR("could not open file: \"%s\".", argv[1]);
	while((type = get_token(input, &info)) != END){
		switch(type){
		case UNKNOWN_SYMBOL:
			parse_label(input, info.str);
			break;
		case OPCODE:
			parse_opcode(input, info.value);
			break;
		case DIRETIVE:
			parse_directive(input, info.value);
			break;
		case EOL:
			break;
		default:
			ERROR("invalid token at line #%d.", current_line);
		}
	}
	resolve_symbols();

	FILE *output = fopen(argv[2], "w");
	if(output == NULL)
		ERROR("could not open file: \"%s\".", argv[2]);
	print_codes(output);

	fclose(input); fclose(output);
    return 0;
}

