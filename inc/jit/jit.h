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
		};

		struct RawInstruction {
			enum RawInstructionType {
				INVALID,

				NOP,
				TRAP,

				MOV,
				CMOV,

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

				TAKE_EXCEPTION,
				SET_CPU_MODE,
			};

			RawInstructionType type;
			RawOperand operands[4];

			std::string mnemonic() const;
		};

		struct RawBytecode {
			uint32_t block_id;
			RawInstruction insn;

			std::string render() const;
		};

		struct RawVRegDescriptor {
			uint8_t size;
		};

		struct RawBytecodeDescriptor {
			uint32_t block_count;
			uint32_t vreg_count;
			RawVRegDescriptor vregs[1024];
			uint32_t bytecode_count;
			RawBytecode bc[];
		};

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

		//protected:
		//	virtual bool generate_internal_bytecode(const RawBytecode *ir, uint32_t count);
		protected:
			void *_code_arena;
			uint64_t _code_arena_size;
		};

		class BlockJIT
		{
		public:
			virtual ~BlockJIT();
			virtual void *compile_block(const RawBytecodeDescriptor *bcd) = 0;
		};

		class RegionJIT
		{
		public:
			virtual ~RegionJIT();
			virtual void *compile_region(const RawBytecodeDescriptor *bcd) = 0;
		};
	}
}

#endif	/* JIT_H */

