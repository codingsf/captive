/*
 * File:   encode.h
 * Author: spink
 *
 * Created on 27 April 2015, 14:26
 */

#ifndef ENCODE_H
#define	ENCODE_H

#include <define.h>
#include <allocator.h>
#include <x86/defs.h>

namespace captive {
	namespace arch {
		namespace x86 {
			struct X86Register
			{
				X86Register(uint8_t size, uint8_t raw_index, bool hi = false, bool nw = false) : size(size), raw_index(raw_index), hireg(hi), newreg(nw) { }
				uint8_t size;
				uint8_t raw_index;
				bool hireg;
				bool newreg;

				inline bool operator==(const X86Register& other) const { return other.size == size && other.raw_index == raw_index && other.hireg == hireg && other.newreg == newreg; }
				inline bool operator!=(const X86Register& other) const { return !(other.size == size && other.raw_index == raw_index && other.hireg == hireg && other.newreg == newreg); }
			};

			extern X86Register REG_RAX, REG_EAX, REG_AX, REG_AL;
			extern X86Register REG_RCX, REG_ECX, REG_CX, REG_CL;
			extern X86Register REG_RDX, REG_EDX, REG_DX, REG_DL;
			extern X86Register REG_RBX, REG_EBX, REG_BX, REG_BL;
			extern X86Register REG_RSP, REG_ESP, REG_SP, REG_SIL;
			extern X86Register REG_RBP, REG_EBP, REG_BP, REG_BPL;
			extern X86Register REG_RSI, REG_ESI, REG_SI, REG_SIL;
			extern X86Register REG_RDI, REG_EDI, REG_DI, REG_DIL;
			extern X86Register REG_R8, REG_R8D, REG_R8W, REG_R8B;
			extern X86Register REG_R9, REG_R9D, REG_R9W, REG_R9B;
			extern X86Register REG_R10, REG_R10D, REG_R10W, REG_R10B;
			extern X86Register REG_R11, REG_R11D, REG_R11W, REG_R11B;
			extern X86Register REG_R12, REG_R12D, REG_R12W, REG_R12B;
			extern X86Register REG_R13, REG_R13D, REG_R13W, REG_R13B;
			extern X86Register REG_R14, REG_R14D, REG_R14W, REG_R14B;
			extern X86Register REG_R15, REG_R15D, REG_R15W, REG_R15B;
			extern X86Register REG_RIZ, REG_RIP;

			struct X86Memory
			{
				uint8_t scale;
				const X86Register& index;
				const X86Register& base;
				int32_t displacement;

				X86Memory(const X86Register& _base) : scale(0), index(REG_RIZ), base(_base), displacement(0) { }
				X86Memory(const X86Register& _base, int32_t disp) : scale(0), index(REG_RIZ), base(_base), displacement(disp) { }

				static X86Memory get(const X86Register& base)
				{
					return X86Memory(base);
				}

				static X86Memory get(const X86Register& base, int32_t disp)
				{
					return X86Memory(base, disp);
				}
			};

			class X86Encoder
			{
			public:
				X86Encoder(Allocator& allocator);

				inline uint8_t *get_buffer() { return _buffer; }
				inline uint32_t get_buffer_size() { return _write_offset; }

				void call(const X86Register& reg);

				void push(const X86Register& reg);
				void pop(const X86Register& reg);

				void movzx(const X86Register& src, const X86Register& dst);
				void movsx(const X86Register& src, const X86Register& dst);

				void mov(const X86Register& src, const X86Register& dst);
				void mov(const X86Memory& src, const X86Register& dst);
				void mov(const X86Register& src, const X86Memory& dst);
				void mov(uint64_t src, const X86Register& dst);

				void mov8(uint64_t imm, const X86Memory& dst);
				void mov4(uint32_t imm, const X86Memory& dst);
				void mov2(uint16_t imm, const X86Memory& dst);
				void mov1(uint8_t imm, const X86Memory& dst);

				void andd(uint32_t val, const X86Register& dst);
				void andd(uint32_t val, const X86Memory& dst);
				void orr(uint32_t val, const X86Register& dst);
				void orr(uint32_t val, const X86Memory& dst);
				void xorr(uint32_t val, const X86Register& dst);
				void xorr(uint32_t val, const X86Memory& dst);

				void andd(const X86Register src, const X86Register& dest);
				void andd(const X86Register src, const X86Memory& dest);
				void orr(const X86Register src, const X86Register& dest);
				void orr(const X86Register src, const X86Memory& dest);
				void xorr(const X86Register src, const X86Register& dest);
				void xorr(const X86Register src, const X86Memory& dest);

				void shl(uint8_t amount, const X86Register& dst);
				void shr(uint8_t amount, const X86Register& dst);
				void sar(uint8_t amount, const X86Register& dst);

				void add(const X86Register& src, const X86Register& dst);
				void add(uint32_t val, const X86Register& dst);

				void sub(const X86Register& src, const X86Register& dst);
				void sub(uint32_t val, const X86Register& dst);

				void cmp(const X86Register& src, const X86Register& dst);
				void cmp(uint32_t val, const X86Register& dst);
				void test(const X86Register& op1, const X86Register& op2);

				void jmp_reloc(uint32_t& reloc_offset);
				void jnz_reloc(uint32_t& reloc_offset);

				void sete(const X86Register& dst);
				void setne(const X86Register& dst);

				void leave();
				void ret();
				void hlt();
				void nop();

				uint32_t current_offset() const { return _write_offset; }

			private:
				Allocator& _alloc;

				uint8_t *_buffer;
				uint32_t _buffer_size;
				uint32_t _write_offset;

				inline void ensure_buffer()
				{
					if (_write_offset >= _buffer_size) {
						_buffer_size += 64;
						_buffer = (uint8_t *)_alloc.reallocate(_buffer, _buffer_size);
					}
				}

				inline void emit8(uint8_t b)
				{
					ensure_buffer();
					_buffer[_write_offset++] = b;
				}

				inline void emit16(uint16_t v)
				{
					ensure_buffer();

					_buffer[_write_offset++] = v & 0xff;
					_buffer[_write_offset++] = (v >> 8) & 0xff;
				}

				inline void emit32(uint32_t v)
				{
					ensure_buffer();

					_buffer[_write_offset++] = v & 0xff;
					_buffer[_write_offset++] = (v >> 8) & 0xff;
					_buffer[_write_offset++] = (v >> 16) & 0xff;
					_buffer[_write_offset++] = (v >> 24) & 0xff;
				}

				inline void emit64(uint64_t v)
				{
					ensure_buffer();

					_buffer[_write_offset++] = v & 0xff;
					_buffer[_write_offset++] = (v >> 8) & 0xff;
					_buffer[_write_offset++] = (v >> 16) & 0xff;
					_buffer[_write_offset++] = (v >> 24) & 0xff;
					_buffer[_write_offset++] = (v >> 32) & 0xff;
					_buffer[_write_offset++] = (v >> 40) & 0xff;
					_buffer[_write_offset++] = (v >> 48) & 0xff;
					_buffer[_write_offset++] = (v >> 56) & 0xff;
				}

				void encode_arithmetic(uint8_t oper, uint32_t imm, const X86Register& dst);

				void encode_mod_reg_rm(uint8_t mreg, const X86Register& rm);
				void encode_mod_reg_rm(const X86Register& reg, const X86Register& rm);
				void encode_mod_reg_rm(uint8_t mreg, const X86Memory& rm);
				void encode_mod_reg_rm(const X86Register& reg, const X86Memory& rm);

				void encode_rex_prefix(bool b, bool x, bool r, bool w);

				void encode_opcode_mod_rm(uint16_t opcode, const X86Register& reg, const X86Memory& rm);
				void encode_opcode_mod_rm(uint16_t opcode, const X86Register& reg, const X86Register& rm);
				void encode_opcode_mod_rm(uint16_t opcode, uint8_t oper, const X86Register& rm);
				void encode_opcode_mod_rm(uint16_t opcode, uint8_t oper, uint8_t size, const X86Memory& rm);
			};
		}
	}
}

#endif	/* ENCODE_H */

