#include <x86/decode.h>
#include <printf.h>

using namespace captive::arch::x86;

enum X86InstructionPrefixes
{
	NONE = 0,

	ADDRESS_SIZE_OVERRIDE = 1,
	OPERAND_SIZE_OVERRIDE = 2,
};

static X86InstructionPrefixes read_prefixes(const uint8_t **code)
{
	X86InstructionPrefixes p = NONE;

	bool prefixes_complete = false;
	while (!prefixes_complete) {
		switch (**code) {
		case 0x67: p = (X86InstructionPrefixes)(p | ADDRESS_SIZE_OVERRIDE); break;
		case 0x66: p = (X86InstructionPrefixes)(p | OPERAND_SIZE_OVERRIDE); break;
		default: prefixes_complete = true; continue;
		}

		(*code)++;
	}

	return p;
}

static void read_modrm(const uint8_t **code, uint8_t& mod, uint8_t& reg, uint8_t& rm)
{
	uint8_t val = **code;

	mod = (val >> 6) & 3;
	reg = (val >> 3) & 7;
	rm = (val >> 0) & 7;

	(*code)++;
}

static bool decode_mov_r_rm(X86InstructionPrefixes pfx, const uint8_t **code, MemoryInstruction& inst)
{
	inst.type = MemoryInstruction::I_MOV;

	uint8_t mod, reg, rm;

	read_modrm(code, mod, reg, rm);

	inst.Source.type = Operand::TYPE_REGISTER;

	switch (reg) {
	case 0:	inst.Source.reg = Operand::R_EAX; break;
	case 1:	inst.Source.reg = Operand::R_ECX; break;
	case 2:	inst.Source.reg = Operand::R_EDX; break;
	case 3:	inst.Source.reg = Operand::R_EBX; break;
	case 4:	inst.Source.reg = Operand::R_ESP; break;
	case 5:	inst.Source.reg = Operand::R_EBP; break;
	case 6:	inst.Source.reg = Operand::R_ESI; break;
	case 7:	inst.Source.reg = Operand::R_EDI; break;
	default: return false;
	}

	switch (mod) {
	case 0:
		inst.Dest.type = Operand::TYPE_MEMORY;

		switch (rm) {
		case 0: if (pfx & ADDRESS_SIZE_OVERRIDE) inst.Dest.mem.base_reg_idx = Operand::R_EAX; else inst.Dest.mem.base_reg_idx = Operand::R_RAX; break;
		case 1:	if (pfx & ADDRESS_SIZE_OVERRIDE) inst.Dest.mem.base_reg_idx = Operand::R_ECX; else inst.Dest.mem.base_reg_idx = Operand::R_RCX; break;
		case 2:	if (pfx & ADDRESS_SIZE_OVERRIDE) inst.Dest.mem.base_reg_idx = Operand::R_EDX; else inst.Dest.mem.base_reg_idx = Operand::R_RDX; break;
		case 3:	if (pfx & ADDRESS_SIZE_OVERRIDE) inst.Dest.mem.base_reg_idx = Operand::R_EBX; else inst.Dest.mem.base_reg_idx = Operand::R_RBX; break;
		case 4:	return false;
		case 5:	return false;
		case 6:	if (pfx & ADDRESS_SIZE_OVERRIDE) inst.Dest.mem.base_reg_idx = Operand::R_ESI; else inst.Dest.mem.base_reg_idx = Operand::R_RSI; break;
		case 7:	if (pfx & ADDRESS_SIZE_OVERRIDE) inst.Dest.mem.base_reg_idx = Operand::R_EDI; else inst.Dest.mem.base_reg_idx = Operand::R_RDI; break;
		}

		break;
	default: return false;
	}

	return true;
}

static const char *reg_names[] = {
	"%rax",
	"%rbx",
	"%rcx",
	"%rdx",
	"%rsp",
	"%rbp",
	"%rsi",
	"%rdi",

	"%eax",
	"%ebx",
	"%ecx",
	"%edx",
	"%esp",
	"%ebp",
	"%esi",
	"%edi",
};

bool captive::arch::x86::decode_memory_instruction(const uint8_t *code, MemoryInstruction& inst)
{
	const uint8_t *base = code;

	// Read Prefixes
	X86InstructionPrefixes p = read_prefixes(&code);

	// Read Opcode
	uint16_t opcode = 0;
	if (*code == 0x0f) {
		opcode |= 0x100;
		code++;
	}

	opcode |= *code++;

	printf("insn: prefixes = %d, opcode = %d\n", p, opcode);

	switch (opcode) {
	case 0x89: if (!decode_mov_r_rm(p, &code, inst)) return false; else break;
	default: return false;
	}

	inst.length = (code - base);
	printf("length=%d: ", inst.length);
	
	switch (inst.type) {
	case MemoryInstruction::I_MOV:
		printf("mov ");
		break;
	}

	switch (inst.Source.type) {
	case Operand::TYPE_REGISTER: printf("%s", reg_names[inst.Source.reg]); break;
	case Operand::TYPE_MEMORY: printf("(%s)", reg_names[inst.Source.mem.base_reg_idx]); break;
	}

	printf(", ");

	switch (inst.Dest.type) {
	case Operand::TYPE_REGISTER: printf("%s", reg_names[inst.Dest.reg]); break;
	case Operand::TYPE_MEMORY: printf("(%s)", reg_names[inst.Dest.mem.base_reg_idx]); break;
	}

	printf("\n");

	return true;
}
