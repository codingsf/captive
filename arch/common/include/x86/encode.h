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
				X86Register(uint8_t size, uint8_t raw_index, bool hi = false) : size(size), raw_index(raw_index), hireg(hi) { }
				uint8_t size;
				uint8_t raw_index;
				bool hireg;
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
			extern X86Register REG_RIZ;

			struct X86Memory
			{
				uint8_t scale;
				X86Register& index;
				X86Register& base;
				int32_t displacement;

				X86Memory(X86Register& _base) : scale(0), index(REG_RIZ), base(_base), displacement(0) { }

				static X86Memory get(X86Register& base)
				{
					return X86Memory(base);
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

				void mov(const X86Register& src, const X86Register& dst);
				void mov(const X86Memory& src, const X86Register& dst);
				void mov(const X86Register& src, const X86Memory& dst);

				void mov(uint64_t src, const X86Register& dst);

				void xorr(const X86Register src, const X86Register& dest);

				void leave();
				void ret();

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

				void encode_mod_reg_rm(const X86Register& reg, const X86Register& rm);
				void encode_mod_reg_rm(const X86Register& reg, const X86Memory& rm);

				void encode_rex_prefix(bool b, bool x, bool r, bool w);
			};
		}
	}
}

#endif	/* ENCODE_H */

