/*
 * File:   translation-context.h
 * Author: spink
 *
 * Created on 27 February 2015, 15:07
 */

#ifndef TRANSLATION_CONTEXT_H
#define	TRANSLATION_CONTEXT_H

#include <define.h>
#include <shared-memory.h>
#include <shared-jit.h>

namespace captive {
	namespace arch {
		class SharedMemory;

		namespace jit {
			class TranslationContext
			{
			public:
				TranslationContext(SharedMemory& allocator, shared::TranslationBlock& block);

				inline void add_instruction(const shared::IRInstruction& instruction) {
					add_instruction(_current_block_id, instruction);
				}

				inline void add_instruction(shared::IRBlockId block_id, const shared::IRInstruction& instruction) {
					ensure_buffer(_block.ir_insn_count + 1);

					_block.ir_insn[_block.ir_insn_count] = instruction;
					_block.ir_insn_count++;
				}

				inline shared::IRBlockId current_block() const { return _current_block_id; }
				inline void current_block(shared::IRBlockId block_id) { _current_block_id = block_id; }

				inline shared::IRBlockId alloc_block() {
					return _block.ir_block_count++;
				}

				inline shared::IRRegId alloc_reg(uint8_t size) {
					return _block.ir_reg_count++;
				}

				inline void cancel() {
					_block.ir_insn_count = 0;
					_block.ir_reg_count = 0;
					_block.ir_block_count = 0;
					_allocator.free(_block.ir_insn);
				}

			private:
				SharedMemory& _allocator;
				shared::TranslationBlock& _block;

				shared::IRBlockId _current_block_id;
				uint32_t _ir_insn_buffer_size;

				inline void ensure_buffer(uint32_t elem_capacity)
				{
					uint32_t required_size = (sizeof(shared::IRInstruction) * elem_capacity);

					if (_ir_insn_buffer_size < required_size) {
						while (_ir_insn_buffer_size < required_size) {
							_ir_insn_buffer_size <<= 1;
						}

						_block.ir_insn = (shared::IRInstruction *)_allocator.reallocate(_block.ir_insn, _ir_insn_buffer_size);
					}
				}
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */

