/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:51
 */

#ifndef CPU_H
#define	CPU_H

#include <define.h>
#include <trace.h>
#include <mmu.h>

#define DECODE_CACHE_SIZE	8192
#define DECODE_OBJ_SIZE		128
#define DECODE_CACHE_ENTRIES	(DECODE_CACHE_SIZE / DECODE_OBJ_SIZE)

namespace captive {
	namespace arch {
		namespace jit {
			class GuestBasicBlock;
		}

		class Environment;
		class MMU;
		class Interpreter;
		class Decode;
		class JIT;

		class CPU
		{
		public:
			CPU(Environment& env);
			virtual ~CPU();

			virtual bool init(unsigned int ep) = 0;
			bool run();

			virtual MMU& mmu() const = 0;
			virtual Interpreter& interpreter() const = 0;
			virtual JIT& jit() const = 0;

			inline Environment& env() const { return _env; }
			inline Trace& trace() { return *_trace; }

			virtual uint32_t read_pc() const = 0;
			virtual uint32_t write_pc(uint32_t new_pc_val) = 0;
			virtual uint32_t inc_pc(uint32_t delta) = 0;

			virtual void dump_state() const = 0;

			inline uint64_t get_insns_executed() const { return insns_executed; }

			inline bool kernel_mode() const { return local_state._kernel_mode; }

			inline void kernel_mode(bool km) {
				if (local_state._kernel_mode != km) {
					local_state._kernel_mode = km;
					mmu().cpu_privilege_change(km);
				}
			}

			void flush_decode_cache();
			inline void schedule_decode_cache_flush() {
				_should_flush_decode_cache = true;
			}

		protected:
			virtual bool decode_instruction(uint32_t addr, Decode *insn) = 0;
			virtual void *reg_state() = 0;

			inline void inc_insns_executed() {
				insns_executed++;
			}

			Trace *_trace;

			struct {
				bool _kernel_mode;
				uint32_t last_exception_action;
			} local_state;

		private:
			uint64_t insns_executed;
			bool _should_flush_decode_cache;

			Environment& _env;

			uint8_t decode_cache[DECODE_CACHE_SIZE];
			inline Decode *get_decode(uint32_t pc) const {
				return (Decode *)&decode_cache[(pc % DECODE_CACHE_ENTRIES) * DECODE_OBJ_SIZE];
			}

			bool run_interp();
			bool run_block_jit();
			bool run_region_jit();

			jit::GuestBasicBlock *get_basic_block(uint32_t block_addr);
			bool compile_basic_block(uint32_t block_addr, jit::GuestBasicBlock *block);

			bool handle_pending_action(uint32_t action);
		};

		extern CPU *active_cpu;
	}
}

#endif	/* CPU_H */

