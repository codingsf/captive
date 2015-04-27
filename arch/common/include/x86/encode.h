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
				X86Register(uint8_t size, uint8_t raw_index);
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

				void push(const X86Register& reg);
				void pop(const X86Register& reg);

				void mov(const X86Register& src, const X86Register& dst);
				void mov(const X86Memory& src, const X86Register& dst);
				void mov(const X86Register& src, const X86Memory& dst);

				void e_xor(const X86Register src, const X86Register& dest);

				void leave();
				void ret();

			private:
				Allocator& _alloc;

				uint8_t *_buffer;
				uint32_t _buffer_size;
				uint32_t _encoding_size;
				uint32_t _write_offset;

				inline void ensure_buffer()
				{
					if (_encoding_size >= _buffer_size) {
						_buffer_size += 64;
						_buffer = (uint8_t *)_alloc.reallocate(_buffer, _buffer_size);
					}
				}

				inline void emit(uint8_t b)
				{
					ensure_buffer();
					_buffer[_write_offset++] = b;
					_encoding_size++;
				}

				void encode_mod_reg_rm(const Operand& reg, const Operand& rm);
			};
		}
	}
}

#endif	/* ENCODE_H */

