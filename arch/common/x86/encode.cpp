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

X86Register REG_RIZ(0, 0);
} } }

#define REX	0x40
#define REX_B	0x41
#define REX_X	0x42
#define REX_R	0x44
#define REX_W	0x48

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

void X86Encoder::xorr(const X86Register src, const X86Register& dest)
{
	assert(src.size == dest.size);

	if (src.size == 8) {
		encode_rex_prefix(false, false, false, true);
	}

	emit8(0x31);
	encode_mod_reg_rm(src, dest);
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
		uint8_t rex = 0;

		if (dst.newreg) {
			rex |= REX;
		} else if (dst.hireg) {
			rex |= REX_R;
		}

		if (src.base.hireg) {
			rex |= REX_B;
		}

		if (rex) {
			emit8(rex);
		}

		emit8(0x8a);
		encode_mod_reg_rm(dst, src);
	} else {
		uint8_t rex = 0;

		if (dst.size == 8) {
			rex |= REX_W;
		} else if (dst.size == 2) {
			assert(false);
		}

		if (dst.hireg) {
			rex |= REX_R;
		}

		if (src.base.hireg) {
			rex |= REX_B;
		}

		if (rex) {
			emit8(rex);
		}

		emit8(0x8b);
		encode_mod_reg_rm(dst, src);
	}
}

void X86Encoder::mov(const X86Register& src, const X86Memory& dst)
{
	uint8_t rex = 0;

	if (src.size == 8) {
		rex |= REX_W;
	}

	if (src.hireg) {
		rex |= REX_R;
	}

	if (dst.base.hireg) {
		rex |= REX_B;
	}

	if (rex) {
		emit8(rex);
	}

	emit8(0x89);
	encode_mod_reg_rm(src, dst);
}

void X86Encoder::mov(uint64_t src, const X86Register& dst)
{
	if (dst.size == 8) {
		encode_rex_prefix(false, false, false, true);
	} else if (dst.size == 2) {
		assert(false);
	} else if (dst.size == 1) {
		assert(false);
	}

	emit8(0xb8 + dst.raw_index);

	switch(dst.size) {
	case 8:
		emit64(src);
		break;
	case 4:
		emit32(src);
		break;
	}
}

void X86Encoder::mov(uint64_t src, const X86Memory& dst)
{
	assert(false);
}

void X86Encoder::andd(uint32_t val, const X86Memory& dst)
{
	if (val < 256) {
		emit8(0x83);
		encode_mod_reg_rm(4, dst);
		emit8(val);
	} else {
		emit8(0x81);
		encode_mod_reg_rm(4, dst);
		emit32(val);
	}
}

void X86Encoder::andd(uint32_t val, const X86Register& dst)
{
	if (val < 256) {
		emit8(0x83);
		encode_mod_reg_rm(4, dst);
		emit8(val);
	} else {
		emit8(0x81);
		encode_mod_reg_rm(4, dst);
		emit32(val);
	}
}

void X86Encoder::shl(uint8_t amount, const X86Register& dst)
{
	if (dst.size == 1) {
		assert(false);
	} else {
		uint8_t rex = 0;

		if (dst.size == 8)
			rex |= REX_W;

		if (dst.hireg)
			rex |= REX_B;

		if (rex) emit8(rex);

		emit8(0xc1);
		encode_mod_reg_rm(4, dst);

		emit8(amount);
	}
}
void X86Encoder::shr(uint8_t amount, const X86Register& dst)
{
	if (dst.size == 1) {
		assert(false);
	} else {
		uint8_t rex = 0;

		if (dst.size == 8)
			rex |= REX_W;

		if (dst.hireg)
			rex |= REX_B;

		if (rex) emit8(rex);

		emit8(0xc1);
		encode_mod_reg_rm(5, dst);

		emit8(amount);
	}
}

void X86Encoder::sar(uint8_t amount, const X86Register& dst)
{
	if (dst.size == 1) {
		assert(false);
	} else {
		uint8_t rex = 0;

		if (dst.size == 8)
			rex |= REX_W;

		if (dst.hireg)
			rex |= REX_B;

		if (rex) emit8(rex);

		emit8(0xc1);
		encode_mod_reg_rm(7, dst);

		emit8(amount);
	}
}

void X86Encoder::sub(uint32_t val, const X86Register& dst)
{
	if (val < 256) {
		uint8_t rex = 0;

		if (dst.size == 8)
			rex |= REX_W;

		if (dst.hireg)
			rex |= REX_B;

		if (rex) emit8(rex);

		emit8(0x83);
		encode_mod_reg_rm(5, dst);

		emit8(val);
	} else {
		assert(false);
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

	mrm = rm.base.raw_index;

	if (rm.base == REG_RSP) assert(false);

	if (mod == 0 && rm.base == REG_RBP) {
		mod = 1;
	}

	emit8((mod & 3) << 6 | (mreg & 7) << 3 | (mrm & 7));

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
