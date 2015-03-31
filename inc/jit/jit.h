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
	namespace hypervisor {
		class SharedMemory;
	}

	namespace util {
		class ThreadPool;
	}

	namespace shared {
		struct RegionWorkUnit;
		struct BlockWorkUnit;
	}

	namespace jit {
		class BlockJIT;
		class RegionJIT;

		struct RawOperand {
			enum RawOperandType {
				NONE,
				CONSTANT,
				VREG,
				BLOCK,
				FUNC,
				PC
			};

			RawOperandType type;
			uint64_t val;
			uint8_t size;

			std::string render() const;
		} __packed;

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
		} __packed;

		struct RawBytecode {
			uint32_t block_id;
			RawInstruction insn;

			std::string render() const;
		} __packed;

		struct RawVRegDescriptor {
			uint8_t size;
		} __packed;

		struct RawBytecodeDescriptor {
			uint32_t block_count;
			uint32_t vreg_count;
			uint32_t bytecode_count;
			RawBytecode bc[];
		} __packed;

		class JIT
		{
		public:
			JIT(util::ThreadPool& worker_threads);
			virtual ~JIT();

			inline util::ThreadPool& worker_threads() const { return _worker_threads; }

			virtual bool init() = 0;

			virtual BlockJIT& block_jit() = 0;
			virtual RegionJIT& region_jit() = 0;

			void set_shared_memory(hypervisor::SharedMemory& shmem) {
				_shared_memory = &shmem;
			}

		protected:
			util::ThreadPool& _worker_threads;
			hypervisor::SharedMemory *_shared_memory;

			bool quick_opt(RawBytecode *bc, uint32_t count);
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

		struct BlockCompilationResult
		{
			uint64_t work_unit_id;
			void *fn_ptr;
		};

		struct RegionCompilationResult
		{
			uint64_t work_unit_id;
			void *fn_ptr;
		};

		class BlockJIT : public JITStrategy
		{
		public:
			typedef void (*block_completion_t)(BlockCompilationResult result, void *completion_data);

			BlockJIT(JIT& owner);
			virtual ~BlockJIT();
			virtual BlockCompilationResult compile_block(shared::BlockWorkUnit *work_unit) = 0;
			void compile_block_async(shared::BlockWorkUnit *work_unit, block_completion_t completion, void *completion_data);
		};

		class RegionJIT : public JITStrategy
		{
		public:
			typedef void (*region_completion_t)(shared::RegionWorkUnit *work_unit, RegionCompilationResult result, void *completion_data);

			RegionJIT(JIT& owner);
			virtual ~RegionJIT();

			virtual RegionCompilationResult compile_region(shared::RegionWorkUnit *work_unit) = 0;
			void compile_region_async(shared::RegionWorkUnit *work_unit, region_completion_t completion, void *completion_data);
		};
	}
}

#endif	/* JIT_H */

