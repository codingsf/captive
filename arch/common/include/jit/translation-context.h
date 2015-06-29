/*
 * File:   translation-context.h
 * Author: spink
 *
 * Created on 27 February 2015, 15:07
 */

#ifndef TRANSLATION_CONTEXT_H
#define	TRANSLATION_CONTEXT_H

#include <define.h>
#include <malloc.h>
#include <shared-jit.h>
#include <shared-memory.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext
			{
			public:
				TranslationContext();
				~TranslationContext();

				inline void add_instruction(const shared::IRInstruction& instruction) {
					add_instruction(_current_block_id, instruction);
				}

				inline void add_instruction(shared::IRBlockId block_id, const shared::IRInstruction& instruction) {
					ensure_buffer(_ir_insn_count + 1);

					_ir_insns[_ir_insn_count] = instruction;
					_ir_insns[_ir_insn_count].ir_block = block_id;
					_ir_insn_count++;
				}

				inline shared::IRBlockId current_block() const { return _current_block_id; }
				inline void current_block(shared::IRBlockId block_id) { _current_block_id = block_id; }

				inline shared::IRBlockId alloc_block() {
					return _ir_block_count++;
				}

				inline shared::IRRegId alloc_reg(uint8_t size) {
					return _ir_reg_count++;
				}
				
				inline uint32_t count() const { return _ir_insn_count; }
				
				inline shared::IRInstruction *at(uint32_t idx) const { return &_ir_insns[idx]; }
				
				inline void swap(uint32_t a, uint32_t b)
				{
					shared::IRInstruction tmp = _ir_insns[a];
					_ir_insns[a] = _ir_insns[b];
					_ir_insns[b] = tmp;
				}
				
				inline const shared::IRInstruction *get_ir_buffer() const { return _ir_insns; }
				
			private:
				shared::IRBlockId _current_block_id;
				
				uint32_t _ir_block_count;
				uint32_t _ir_reg_count;
				
				shared::IRInstruction *_ir_insns;
				
				uint32_t _ir_insn_count;
				uint32_t _ir_insn_buffer_size;

				inline void ensure_buffer(uint32_t elem_capacity)
				{
					uint32_t required_size = (sizeof(shared::IRInstruction) * elem_capacity);

					if (_ir_insn_buffer_size < required_size) {
						_ir_insn_buffer_size = required_size + (sizeof(shared::IRInstruction) * 255);
						_ir_insns = (shared::IRInstruction *)shrealloc(_ir_insns, _ir_insn_buffer_size);
					}
				}
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */
