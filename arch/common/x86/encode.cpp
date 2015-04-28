#include <x86/encode.h>
#include <local-memory.h>

using namespace captive::arch::x86;

namespace captive { namespace arch { namespace x86 {
X86Register REG_RAX(8, 0), REG_EAX(4, 0), REG_AX(2, 0), REG_AL(1, 0);
X86Register REG_RCX(8, 1), REG_ECX(4, 1), REG_CX(2, 1), REG_CL(1, 1);
X86Register REG_RDX(8, 2), REG_EDX(4, 2), REG_DX(2, 2), REG_DL(1, 2);
X86Register REG_RBX(8, 3), REG_EBX(4, 3), REG_BX(2, 3), REG_BL(1, 3);
X86Register REG_RSP(8, 4), REG_ESP(4, 4), REG_SP(2, 4), REG_SPL(1, 4);
X86Register REG_RBP(8, 5), REG_EBP(4, 5), REG_BP(2, 5), REG_BPL(1, 5);
X86Register REG_RSI(8, 6), REG_ESI(4, 6), REG_SI(2, 6), REG_SIL(1, 6);
X86Register REG_RDI(8, 7), REG_EDI(4, 7), REG_DI(2, 7), REG_DIL(1, 7);
} } }

X86Encoder::X86Encoder(Allocator& allocator) : _alloc(allocator), _buffer(NULL), _buffer_size(0), _write_offset(0)
{
}

void X86Encoder::push(const X86Register& reg)
{
	if (reg.size == 2 || reg.size == 8) {
		if (reg.size == 16) {
			emit(0x66);
		}

		emit(0x50 + reg.raw_index);
	} else {
		assert(false);
	}
}

void X86Encoder::xorr(const X86Register src, const X86Register& dest)
{
	emit(0x31);
	encode_mod_reg_rm(src, dest);
}

void X86Encoder::mov(const X86Register& src, const X86Register& dst)
{

}

void X86Encoder::ret()
{
	emit(0xc3);
}

void X86Encoder::leave()
{
	emit(0xc9);
}

void X86Encoder::encode_mod_reg_rm(const X86Register& reg, const X86Register& rm)
{
	uint8_t mod, mreg, mrm;

	mod = 3;
	mreg = reg.raw_index;
	mrm = reg.raw_index;

	emit((mod & 3) << 6 | (mreg & 7) << 3 | (mrm & 7));
}

void X86Encoder::encode_mod_reg_rm(const X86Register& reg, const X86Memory& rm)
{
	uint8_t mod, mreg, mrm;

	if (rm.displacement == 0) {
		mod = 0;
	} else {
		assert(false);
	}

	mreg = reg.raw_index;
	mrm = reg.raw_index;

	emit((mod & 3) << 6 | (mreg & 7) << 3 | (mrm & 7));
}
