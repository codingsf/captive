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

		class JIT
		{
		public:
			virtual ~JIT();

			virtual bool init() = 0;

			virtual BlockJIT& block_jit() = 0;
			virtual RegionJIT& region_jit() = 0;
		};

		struct RawOperand {
			enum RawOperandType {
				CONSTANT,
				VREG,
				BLOCK
			};

			RawOperandType type;
			uint64_t val;
			uint8_t size;

			std::string render() const;
		};

		struct RawInstruction {
			enum RawInstructionType {
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

				READ_REG,
				WRITE_REG,
				READ_MEM,
				WRITE_MEM,

				JMP,
				BRANCH,
				TAKE_EXCEPTION,
			};

			RawInstructionType type;
			RawOperand operands[4];

			std::string mnemonic() const;
		};

		struct RawBytecode {
			uint32_t block_id;
			RawInstruction insn;

			void dump() const;
		};

		class BlockJIT
		{
		public:
			virtual ~BlockJIT();
			virtual uint64_t compile_block(const RawBytecode *ir, uint32_t count) = 0;
		};

		class RegionJIT
		{
		public:
			virtual ~RegionJIT();
			virtual uint64_t compile_region(const RawBytecode *ir, uint32_t count) = 0;
		};
	}
}

#endif	/* JIT_H */

