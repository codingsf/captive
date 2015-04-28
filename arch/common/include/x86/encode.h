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
				X86Register(uint8_t size, uint8_t raw_index) : size(size), raw_index(raw_index) { }
				uint8_t size;
				uint8_t raw_index;
			};

			struct X86Memory
			{
				uint8_t scale;
				X86Register& index;
				X86Register& base;
				int32_t displacement;
			};

			class X86Encoder
			{
			public:
				X86Encoder(Allocator& allocator);

				inline uint8_t *get_buffer() { return _buffer; }
				inline uint32_t get_buffer_size() { return _write_offset; }

				void push(const X86Register& reg);
				void pop(const X86Register& reg);

				void mov(const X86Register& src, const X86Register& dst);
				void mov(const X86Memory& src, const X86Register& dst);
				void mov(const X86Register& src, const X86Memory& dst);

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

				inline void emit(uint8_t b)
				{
					ensure_buffer();
					_buffer[_write_offset++] = b;
				}

				void encode_mod_reg_rm(const X86Register& reg, const X86Register& rm);
				void encode_mod_reg_rm(const X86Register& reg, const X86Memory& rm);
			};

			extern X86Register REG_RAX, REG_EAX, REG_AX, REG_AL;
			extern X86Register REG_RCX, REG_ECX, REG_CX, REG_CL;
			extern X86Register REG_RDX, REG_EDX, REG_DX, REG_DL;
			extern X86Register REG_RBX, REG_EBX, REG_BX, REG_BL;
			extern X86Register REG_RSP, REG_ESP, REG_SP, REG_SIL;
			extern X86Register REG_RBP, REG_EBP, REG_BP, REG_BPL;
			extern X86Register REG_RSI, REG_ESI, REG_SI, REG_SIL;
			extern X86Register REG_RDI, REG_EDI, REG_DI, REG_DIL;
		}
	}
}

#endif	/* ENCODE_H */

