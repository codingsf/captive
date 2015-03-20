/*
 * File:   jit.h
 * Author: spink
 *
 * Created on 02 March 2015, 08:49
 */

#ifndef JIT_H
#define	JIT_H

#include <define.h>

namespace captive {
	namespace jit {
		class BlockJIT;
		class RegionJIT;

		struct RawOperand {
			enum RawOperandType {
				NONE,
				CONSTANT,
				VREG,
				BLOCK,
				FUNC
			};

			RawOperandType type;
			uint64_t val;
			uint8_t size;

			std::string render() const;
		} __attribute__((packed));

		struct RawInstruction {
			enum RawInstructionType {
				INVALID,

				VERIFY,

				NOP,
				TRAP,

				MOV,
				CMOV,
				LDPC,

				ADD,
				SUB,
				MUL,
				DIV,
				MOD,

				SHL,
				SHR,
				SAR,
				CLZ,

				AND,
				OR,
				XOR,

				CMPEQ,
				CMPNE,
				CMPGT,
				CMPGTE,
				CMPLT,
				CMPLTE,

				SX,
				ZX,
				TRUNC,

				READ_REG,
				WRITE_REG,
				READ_MEM,
				WRITE_MEM,

				CALL,
				JMP,
				BRANCH,
				RET,

				SET_CPU_MODE,
				WRITE_DEVICE,
				READ_DEVICE
			};

			RawInstructionType type;
			RawOperand operands[6];

			std::string mnemonic() const;
		} __attribute__((packed));

		struct RawBytecode {
			uint32_t block_id;
			RawInstruction insn;

			std::string render() const;
		} __attribute__((packed));

		struct RawVRegDescriptor {
			uint8_t size;
		} __attribute__((packed));

		struct RawBytecodeDescriptor {
			uint32_t block_count;
			uint32_t vreg_count;
			uint32_t bytecode_count;
			RawBytecode bc[];
		} __attribute__((packed));

		class JIT
		{
		public:
			JIT();
			virtual ~JIT();

			virtual bool init() = 0;

			virtual BlockJIT& block_jit() = 0;
			virtual RegionJIT& region_jit() = 0;

			void set_code_arena(void *code_arena, uint64_t arena_size) {
				_code_arena = code_arena;
				_code_arena_size = arena_size;
			}

			void set_ir_buffer(void *ir_buffer, uint64_t ir_buffer_size) {
				_ir_buffer = ir_buffer;
				_ir_buffer_size = ir_buffer_size;
			}

			void *get_code_arena() const { return _code_arena; }
			uint64_t get_code_arena_size() const { return _code_arena_size; }

			void *get_ir_buffer() const { return _ir_buffer; }
			uint64_t get_ir_buffer_size() const { return _ir_buffer_size; }

		protected:
			void *_code_arena;
			uint64_t _code_arena_size;

			void *_ir_buffer;
			uint64_t _ir_buffer_size;
		};

		class JITStrategy
		{
		public:
			JITStrategy(JIT& owner);
			virtual ~JITStrategy();

			inline JIT& owner() const { return _owner; }

		private:
			JIT& _owner;
		};

		class BlockJIT : public JITStrategy
		{
		public:
			BlockJIT(JIT& owner);
			virtual ~BlockJIT();
			uint64_t compile_block(uint64_t ir_offset);

		protected:
			virtual void *internal_compile_block(const RawBytecodeDescriptor *ir) = 0;
		};

		class RegionJIT : public JITStrategy
		{
		public:
			RegionJIT(JIT& owner);
			virtual ~RegionJIT();
			uint64_t compile_region(uint64_t ir_offset);

		protected:
			virtual void *internal_compile_region(const RawBytecodeDescriptor *ir) = 0;
		};
	}
}

#endif	/* JIT_H */

