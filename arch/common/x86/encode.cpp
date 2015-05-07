#include <x86/encode.h>
#include <local-memory.h>

using namespace captive::arch::x86;

namespace captive { namespace arch { namespace x86 {
X86Register REG_RAX(8, 0), REG_EAX(4, 0), REG_AX(2, 0), REG_AL(1, 0);
X86Register REG_RCX(8, 1), REG_ECX(4, 1), REG_CX(2, 1), REG_CL(1, 1);
X86Register REG_RDX(8, 2), REG_EDX(4, 2), REG_DX(2, 2), REG_DL(1, 2);
X86Register REG_RBX(8, 3), REG_EBX(4, 3), REG_BX(2, 3), REG_BL(1, 3);
X86Register REG_RSP(8, 4), REG_ESP(4, 4), REG_SP(2, 4), REG_SPL(1, 4, false, true);
X86Register REG_RBP(8, 5), REG_EBP(4, 5), REG_BP(2, 5), REG_BPL(1, 5, false, true);
X86Register REG_RSI(8, 6), REG_ESI(4, 6), REG_SI(2, 6), REG_SIL(1, 6, false, true);
X86Register REG_RDI(8, 7), REG_EDI(4, 7), REG_DI(2, 7), REG_DIL(1, 7, false, true);

X86Register REG_R8(8, 0, true), REG_R8D(4, 0, true), REG_R8W(2, 0, true), REG_R8B(1, 0, true);
X86Register REG_R9(8, 1, true), REG_R9D(4, 1, true), REG_R9W(2, 1, true), REG_R9B(1, 1, true);
X86Register REG_R10(8, 2, true), REG_R10D(4, 2, true), REG_R10W(2, 2, true), REG_R10B(1, 2, true);
X86Register REG_R11(8, 3, true), REG_R11D(4, 3, true), REG_R11W(2, 3, true), REG_R11B(1, 3, true);
X86Register REG_R12(8, 4, true), REG_R12D(4, 4, true), REG_R12W(2, 4, true), REG_R12B(1, 4, true);
X86Register REG_R13(8, 5, true), REG_R13D(4, 5, true), REG_R13W(2, 5, true), REG_R13B(1, 5, true);
X86Register REG_R14(8, 6, true), REG_R14D(4, 6, true), REG_R14W(2, 6, true), REG_R14B(1, 6, true);
X86Register REG_R15(8, 7, true), REG_R15D(4, 7, true), REG_R15W(2, 7, true), REG_R15B(1, 7, true);

X86Register REG_RIZ(0, 8);
X86Register REG_RIP(0, 9);
} } }

#define REX	0x40
#define REX_B	0x41
#define REX_X	0x42
#define REX_R	0x44
#define REX_W	0x48

#define OPER_SIZE_OVERRIDE 0x66
#define ADDR_SIZE_OVERRIDE 0x67

X86Encoder::X86Encoder(Allocator& allocator) : _alloc(allocator), _buffer(NULL), _buffer_size(0), _write_offset(0)
{
}

void X86Encoder::push(const X86Register& reg)
{
	if (reg.size == 2 || reg.size == 8) {
		if (reg.size == 16) {
			emit8(0x66);
		}

		if (reg.hireg) {
			encode_rex_prefix(true, false, false, false);
		}

		emit8(0x50 + reg.raw_index);
	} else {
		assert(false);
	}
}

void X86Encoder::push(uint32_t imm)
{
	emit8(0x68);
	emit32(imm);
}

void X86Encoder::pop(const X86Register& reg)
{
	if (reg.size == 2 || reg.size == 8) {
		if (reg.size == 16) {
			emit8(0x66);
		}

		if (reg.hireg) {
			encode_rex_prefix(true, false, false, false);
		}

		emit8(0x58 + reg.raw_index);
	} else {
		assert(false);
	}
}

void X86Encoder::lea(const X86Memory& addr, const X86Register& dst)
{
	encode_opcode_mod_rm(0x8d, dst, addr);
}

void X86Encoder::mov(const X86Register& src, const X86Register& dst)
{
	assert(src.size == dst.size);

	if (src.size == 1) {
		if (src.newreg || dst.newreg) {
			emit8(REX);
		}

		emit8(0x88);
		encode_mod_reg_rm(src, dst);
	} else {
		uint8_t rex = 0;

		if (src.size == 8) {
			rex |= REX_W;
		} else if (src.size == 2) {
			assert(false);
		}

		if (src.hireg) {
			rex |= REX_R;
		}

		if (dst.hireg) {
			rex |= REX_B;
		}

		if (rex) {
			emit8(rex);
		}

		emit8(0x89);
		encode_mod_reg_rm(src, dst);
	}
}

void X86Encoder::mov(const X86Memory& src, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0x8a, dst, src);
	} else {
		encode_opcode_mod_rm(0x8b, dst, src);
	}
}

void X86Encoder::mov(const X86Register& src, const X86Memory& dst)
{
	if (src.size == 1) {
		encode_opcode_mod_rm(0x88, src, dst);
	} else {
		encode_opcode_mod_rm(0x89, src, dst);
	}
}

void X86Encoder::mov(uint64_t src, const X86Register& dst)
{
	if (dst.size == 1) {
		emit8(0xb0 + dst.raw_index);
		emit8(src);
	} else {
		if (dst.size == 8) {
			emit8(REX_W);
		} else if (dst.size == 2) {
			emit8(OPER_SIZE_OVERRIDE);
		}

		emit8(0xb8 + dst.raw_index);

		switch(dst.size) {
		case 8: emit64(src); break;
		case 4: emit32(src); break;
		case 2: emit16(src); break;
		}
	}
}

void X86Encoder::mov8(uint64_t src, const X86Memory& dst)
{
	assert(false);
}

void X86Encoder::mov4(uint32_t src, const X86Memory& dst)
{
	encode_opcode_mod_rm(0xc7, 0, 4, dst);
	emit32(src);
}

void X86Encoder::mov2(uint16_t src, const X86Memory& dst)
{
	encode_opcode_mod_rm(0xc7, 0, 2, dst);
	emit16(src);
}

void X86Encoder::mov1(uint8_t src, const X86Memory& dst)
{
	encode_opcode_mod_rm(0xc6, 0, 1, dst);
	emit8(src);
}

void X86Encoder::movzx(const X86Register& src, const X86Register& dst)
{
	assert(src.size < dst.size);
	assert(src.size == 1 || src.size == 2);
	assert(dst.size == 2 || dst.size == 4 || dst.size == 8);

	if (src.size == 1) {
		encode_opcode_mod_rm(0x1b6, dst, src);
	} else if (src.size == 2) {
		encode_opcode_mod_rm(0x1b7, dst, src);
	}
}

void X86Encoder::movsx(const X86Register& src, const X86Register& dst)
{
	assert(src.size < dst.size);
	assert(src.size == 1 || src.size == 2);
	assert(dst.size == 2 || dst.size == 4 || dst.size == 8);

	if (src.size == 1) {
		encode_opcode_mod_rm(0x1be, src, dst);
	} else if (src.size == 2) {
		encode_opcode_mod_rm(0x1bf, src, dst);
	}
}

void X86Encoder::andd(uint32_t val, const X86Memory& dst)
{
	assert(false);
}

void X86Encoder::andd(uint32_t val, const X86Register& dst)
{
	encode_arithmetic(4, val, dst);
}

void X86Encoder::andd(const X86Register src, const X86Register& dest)
{
	assert(src.size == dest.size);

	if (src.size == 1) {
		encode_opcode_mod_rm(0x20, src, dest);
	} else {
		encode_opcode_mod_rm(0x21, src, dest);
	}
}

void X86Encoder::orr(uint32_t val, const X86Memory& dst)
{
	assert(false);
}

void X86Encoder::orr(uint32_t val, const X86Register& dst)
{
	encode_arithmetic(1, val, dst);
}

void X86Encoder::orr(const X86Register src, const X86Register& dest)
{
	assert(src.size == dest.size);

	if (src.size == 1) {
		encode_opcode_mod_rm(0x08, src, dest);
	} else {
		encode_opcode_mod_rm(0x09, src, dest);
	}
}

void X86Encoder::xorr(uint32_t val, const X86Memory& dst)
{
	assert(false);
}

void X86Encoder::xorr(uint32_t val, const X86Register& dst)
{
	encode_arithmetic(6, val, dst);
}

void X86Encoder::xorr(const X86Register src, const X86Register& dest)
{
	assert(src.size == dest.size);

	if (src.size == 1) {
		encode_opcode_mod_rm(0x30, src, dest);
	} else {
		encode_opcode_mod_rm(0x31, src, dest);
	}
}

void X86Encoder::shl(uint8_t amount, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0xc0, 4, dst);
		emit8(amount);
	} else {
		encode_opcode_mod_rm(0xc1, 4, dst);
		emit8(amount);
	}
}

void X86Encoder::shr(uint8_t amount, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0xc0, 5, dst);
		emit8(amount);
	} else {
		encode_opcode_mod_rm(0xc1, 5, dst);
		emit8(amount);
	}
}

void X86Encoder::sar(uint8_t amount, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0xc0, 7, dst);
		emit8(amount);
	} else {
		encode_opcode_mod_rm(0xc1, 7, dst);
		emit8(amount);
	}
}

void X86Encoder::shl(const X86Register& amount, const X86Register& dst)
{
	assert(amount == REG_CL);

	if (dst.size == 1) {
		encode_opcode_mod_rm(0xd2, 4, dst);
	} else {
		encode_opcode_mod_rm(0xd3, 4, dst);
	}
}

void X86Encoder::shr(const X86Register& amount, const X86Register& dst)
{
	assert(amount == REG_CL);

	if (dst.size == 1) {
		encode_opcode_mod_rm(0xd2, 5, dst);
	} else {
		encode_opcode_mod_rm(0xd3, 5, dst);
	}
}

void X86Encoder::sar(const X86Register& amount, const X86Register& dst)
{
	assert(amount == REG_CL);

	if (dst.size == 1) {
		encode_opcode_mod_rm(0xd2, 7, dst);
	} else {
		encode_opcode_mod_rm(0xd3, 7, dst);
	}
}

void X86Encoder::add(const X86Register& src, const X86Register& dst)
{
	if (src.size == 1) {
		encode_opcode_mod_rm(0x0, src, dst);
	} else {
		encode_opcode_mod_rm(0x1, src, dst);
	}
}

void X86Encoder::add(const X86Memory& src, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0x2, dst, src);
	} else {
		encode_opcode_mod_rm(0x3, dst, src);
	}
}

void X86Encoder::add(uint32_t val, const X86Register& dst)
{
	encode_arithmetic(0, val, dst);
}

void X86Encoder::sub(const X86Register& src, const X86Register& dst)
{
	if (src.size == 1) {
		encode_opcode_mod_rm(0x28, src, dst);
	} else {
		encode_opcode_mod_rm(0x29, src, dst);
	}
}

void X86Encoder::sub(const X86Memory& src, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0x2a, dst, src);
	} else {
		encode_opcode_mod_rm(0x2b, dst, src);
	}
}

void X86Encoder::sub(uint32_t val, const X86Register& dst)
{
	encode_arithmetic(5, val, dst);
}

void X86Encoder::cmp(const X86Register& src, const X86Register& dst)
{
	assert(src.size == dst.size);

	if (src.size == 1) {
		encode_opcode_mod_rm(0x38, src, dst);
	} else {
		encode_opcode_mod_rm(0x39, src, dst);
	}
}

void X86Encoder::cmp(uint32_t val, const X86Register& dst)
{
	encode_arithmetic(7, val, dst);
}

void X86Encoder::cmp(uint32_t val, const X86Memory& dst)
{
	encode_arithmetic(7, val, 4, dst);
}

void X86Encoder::encode_arithmetic(uint8_t oper, uint32_t imm, const X86Register& dst)
{
	if (dst.size == 1) {
		encode_opcode_mod_rm(0x80, oper, dst);
		emit8(imm);
	} else {
		if (imm < 128) {
			encode_opcode_mod_rm(0x83, oper, dst);
			emit8(imm);
		} else {
			encode_opcode_mod_rm(0x81, oper, dst);

			if (dst.size == 4 || dst.size == 8) {
				emit32(imm);
			} else if (dst.size == 2) {
				emit16(imm);
			}
		}
	}
}

void X86Encoder::encode_arithmetic(uint8_t oper, uint32_t imm, uint8_t size, const X86Memory& dst)
{
	if (size == 1) {
		encode_opcode_mod_rm(0x80, oper, size, dst);
		emit8(imm);
	} else {
		if (imm < 128) {
			encode_opcode_mod_rm(0x83, oper, size, dst);
			emit8(imm);
		} else {
			encode_opcode_mod_rm(0x81, oper, size, dst);

			if (size == 4 || size == 8) {
				emit32(imm);
			} else if (size == 2) {
				emit16(imm);
			}
		}
	}
}

void X86Encoder::test(const X86Register& op1, const X86Register& op2)
{
	assert(op1.size == op2.size);

	if (op1.size == 1) {
		encode_opcode_mod_rm(0x84, op2, op1);
	} else {
		encode_opcode_mod_rm(0x85, op2, op1);
	}
}

void X86Encoder::call(const X86Register& reg)
{
	emit8(0xff);
	encode_mod_reg_rm(REG_RDX, reg);
}

void X86Encoder::jmp_reloc(uint32_t& reloc_offset)
{
	emit8(0xe9);
	reloc_offset = _write_offset;
	emit32(0);
}

void X86Encoder::jnz_reloc(uint32_t& reloc_offset)
{
	emit8(0x0f);
	emit8(0x85);
	reloc_offset = _write_offset;
	emit32(0);
}

void X86Encoder::sete(const X86Register& dst)
{
	assert(dst.size == 1);

	if (dst.newreg) emit8(REX); else if (dst.hireg) emit8(REX_X);

	emit8(0x0f);
	emit8(0x94);
	encode_mod_reg_rm(0, dst);
}

void X86Encoder::setne(const X86Register& dst)
{
	assert(dst.size == 1);

	if (dst.newreg) emit8(REX); else if (dst.hireg) emit8(REX_X);

	emit8(0x0f);
	emit8(0x95);
	encode_mod_reg_rm(0, dst);
}
void X86Encoder::setl(const X86Register& dst)
{
	assert(dst.size == 1);

	if (dst.newreg) emit8(REX); else if (dst.hireg) emit8(REX_X);

	emit8(0x0f);
	emit8(0x9c);
	encode_mod_reg_rm(0, dst);
}

void X86Encoder::setle(const X86Register& dst)
{
	assert(dst.size == 1);

	if (dst.newreg) emit8(REX); else if (dst.hireg) emit8(REX_X);

	emit8(0x0f);
	emit8(0x9e);
	encode_mod_reg_rm(0, dst);
}

void X86Encoder::setg(const X86Register& dst)
{
	assert(dst.size == 1);

	if (dst.newreg) emit8(REX); else if (dst.hireg) emit8(REX_X);

	emit8(0x0f);
	emit8(0x9f);
	encode_mod_reg_rm(0, dst);
}

void X86Encoder::setge(const X86Register& dst)
{
	assert(dst.size == 1);

	if (dst.newreg) emit8(REX); else if (dst.hireg) emit8(REX_X);

	emit8(0x0f);
	emit8(0x9d);
	encode_mod_reg_rm(0, dst);
}

void X86Encoder::ret()
{
	emit8(0xc3);
}

void X86Encoder::leave()
{
	emit8(0xc9);
}

void X86Encoder::hlt()
{
	emit8(0xf4);
}

void X86Encoder::nop()
{
	emit8(0x90);
}

void X86Encoder::nop(const X86Memory& mem)
{
	emit8(0x0f);
	emit8(0x1f);
	encode_mod_reg_rm(0, mem);
}

void X86Encoder::encode_rex_prefix(bool b, bool x, bool r, bool w)
{
	uint8_t rex = 0x40;

	if (b) rex |= 1;
	if (x) rex |= 2;
	if (r) rex |= 4;
	if (w) rex |= 8;

	emit8(rex);
}

void X86Encoder::encode_mod_reg_rm(uint8_t mreg, const X86Register& rm)
{
	uint8_t mod, mrm;

	mod = 3;
	mrm = rm.raw_index;

	emit8((mod & 3) << 6 | (mreg & 7) << 3 | (mrm & 7));
}

void X86Encoder::encode_mod_reg_rm(const X86Register& reg, const X86Register& rm)
{
	encode_mod_reg_rm(reg.raw_index, rm);
}

void X86Encoder::encode_mod_reg_rm(uint8_t mreg, const X86Memory& rm)
{
	uint8_t mod, mrm;

	if (rm.displacement == 0) {
		mod = 0;
	} else if (rm.displacement < 128 && rm.displacement > -127) {
		mod = 1;
	} else {
		mod = 2;
	}

	if (rm.base == REG_RSP) {
		mrm = 4; // Need a SIB byte
	} else if (rm.base == REG_R12) {
		mrm = 4; // Need a SIB byte
	} else if (rm.base == REG_RIP) {
		assert(false); // Need to think about this
	} else {
		mrm = rm.base.raw_index;
	}

	if (mod == 0 && rm.base == REG_RBP) {
		mod = 1;
	}

	// Emit the MODRM byte
	emit8((mod & 3) << 6 | (mreg & 7) << 3 | (mrm & 7));

	// Determine if we need to emit a SIB byte
	if (mrm == 4 && mod != 3) {
		uint8_t s, i, b;

		if (rm.scale == 0) {
			s = 0;
			i = 4;
			b = rm.base.raw_index;
		} else {
			assert(false); // Not supported yet
		}

		emit8((s & 3) << 6 | (i & 7) << 3 | (b & 7));
	}

	if (mod == 1) {
		emit8(rm.displacement);
	} else if (mod == 2) {
		emit32(rm.displacement);
	}
}

void X86Encoder::encode_mod_reg_rm(const X86Register& reg, const X86Memory& rm)
{
	encode_mod_reg_rm(reg.raw_index, rm);
}

void X86Encoder::encode_opcode_mod_rm(uint16_t opcode, const X86Register& reg, const X86Memory& rm)
{
	assert(reg.size == 1 || reg.size == 2 || reg.size == 4 || reg.size == 8);
	assert(rm.base.size == 4 || rm.base.size == 8);

	// Figure out what prefixes are needed (if any)

	// Emit an operand-size override if the reg operand is 16-bits
	if (reg.size == 2) {
		emit8(OPER_SIZE_OVERRIDE);
	}

	// Emit an address-size override if the memory base is 32-bits
	if (rm.base.size == 4) {
		emit8(ADDR_SIZE_OVERRIDE);
	}

	uint8_t rex = 0;

	// Any REX prefix will do to access the new registers
	if (reg.newreg) {
		rex |= REX;
	}

	// If the reg operand is 64-bits, emit a REX_W
	if (reg.size == 8) {
		rex |= REX_W;
	}

	// If the reg operand is a high register, emit a REX_R
	if (reg.hireg) {
		rex |= REX_R;
	}

	// If the base operand is a high register, emit a REX_B
	if (rm.base.hireg) {
		rex |= REX_B;
	}

	// If we are to emit a REX prefix, do that now.
	if (rex) {
		emit8(rex);
	}

	// Emit the opcode
	if (opcode > 0x100) {
		emit8(0x0f);
	}

	emit8(opcode & 0xff);

	// Emit the modrm
	encode_mod_reg_rm(reg, rm);
}

void X86Encoder::encode_opcode_mod_rm(uint16_t opcode, const X86Register& reg, const X86Register& rm)
{
	// Register sizes MUST match
	// TODO: Think about this
	//assert(reg.size == rm.size);

	// Figure out what prefixes are needed (if any)

	// Emit an operand-size override if the reg operand is 16-bits
	if (reg.size == 2) {
		emit8(OPER_SIZE_OVERRIDE);
	}

	uint8_t rex = 0;

	// Any REX prefix will do to access the new registers
	if (reg.newreg) {
		rex |= REX;
	}

	// If the reg or rm operand is 64-bits, emit a REX_W
	if (reg.size == 8) {
		rex |= REX_W;
	}

	// If the reg operand is a high register, emit a REX_R
	if (reg.hireg) {
		rex |= REX_R;
	}

	// If the rm operand is a high register, emit a REX_B
	if (rm.hireg) {
		rex |= REX_B;
	}

	// If we are to emit a REX prefix, do that now.
	if (rex) {
		emit8(rex);
	}

	// Emit the opcode
	if (opcode > 0x100) {
		emit8(0x0f);
	}

	emit8(opcode & 0xff);

	// Emit the modrm
	encode_mod_reg_rm(reg, rm);
}

void X86Encoder::encode_opcode_mod_rm(uint16_t opcode, uint8_t oper, const X86Register& rm)
{
	// Figure out what prefixes are needed (if any)

	// Emit an operand-size override if the reg operand is 16-bits
	if (rm.size == 2) {
		emit8(OPER_SIZE_OVERRIDE);
	}

	uint8_t rex = 0;

	// Any REX prefix will do to access the new registers
	if (rm.newreg) {
		rex |= REX;
	}

	// If the rm operand is 64-bits, emit a REX_W
	if (rm.size == 8) {
		rex |= REX_W;
	}

	// If the rm operand is a high register, emit a REX_B
	if (rm.hireg) {
		rex |= REX_B;
	}

	// If we are to emit a REX prefix, do that now.
	if (rex) {
		emit8(rex);
	}

	// Emit the opcode
	if (opcode > 0x100) {
		emit8(0x0f);
	}

	emit8(opcode & 0xff);

	// Emit the modrm
	encode_mod_reg_rm(oper, rm);
}

void X86Encoder::encode_opcode_mod_rm(uint16_t opcode, uint8_t oper, uint8_t size, const X86Memory& rm)
{
	assert(size == 1 || size == 2 || size == 4 || size == 8);
	assert(rm.base.size == 4 || rm.base.size == 8);

	// Figure out what prefixes are needed (if any)

	// Emit an operand-size override if the reg operand is 16-bits
	if (size == 2) {
		emit8(OPER_SIZE_OVERRIDE);
	}

	// Emit an address-size override if the memory base is 32-bits
	if (rm.base.size == 4) {
		emit8(ADDR_SIZE_OVERRIDE);
	}

	uint8_t rex = 0;

	// If the reg operand is 64-bits, emit a REX_W
	if (size == 8) {
		rex |= REX_W;
	}

	// If the base operand is a high register, emit a REX_B
	if (rm.base.hireg) {
		rex |= REX_B;
	}

	// If we are to emit a REX prefix, do that now.
	if (rex) {
		emit8(rex);
	}

	// Emit the opcode
	if (opcode > 0x100) {
		emit8(0x0f);
	}

	emit8(opcode & 0xff);

	// Emit the modrm
	encode_mod_reg_rm(oper, rm);
}
