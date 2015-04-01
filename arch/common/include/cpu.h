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
#include <shmem.h>
#include <priv.h>

#define DECODE_CACHE_SIZE	8192
#define DECODE_OBJ_SIZE		128
#define DECODE_CACHE_ENTRIES	(DECODE_CACHE_SIZE / DECODE_OBJ_SIZE)

#define BLOCK_CACHE_SIZE	32768
#define BLOCK_OBJ_SIZE		16
#define BLOCK_CACHE_ENTRIES	(BLOCK_CACHE_SIZE / BLOCK_OBJ_SIZE)

namespace captive {
	namespace shared {
		struct RegionWorkUnit;
	}

	namespace arch {
		namespace jit {
			class GuestBasicBlock;
		}

		namespace profile {
			class Image;
			class Region;
		}

		class Environment;
		class MMU;
		class Interpreter;
		class Decode;
		class JIT;

		class CPU
		{
		public:
			CPU(Environment& env, profile::Image& profile_image, PerCPUData *per_cpu_data);
			virtual ~CPU();

			virtual bool init() = 0;
			bool run();

			virtual MMU& mmu() const = 0;
			virtual Interpreter& interpreter() const = 0;
			virtual JIT& jit() const = 0;

			inline profile::Image& profile_image() const { return _profile_image; }

			inline Environment& env() const { return _env; }
			inline Trace& trace() const { return *_trace; }
			inline PerCPUData& cpu_data() const { return *_per_cpu_data; }

			virtual uint32_t read_pc() const = 0;
			virtual uint32_t write_pc(uint32_t new_pc_val) = 0;
			virtual uint32_t inc_pc(uint32_t delta) = 0;

			virtual void dump_state() const = 0;

			inline uint64_t get_insns_executed() const { return cpu_data().insns_executed; }

			inline bool kernel_mode() const { return local_state._kernel_mode; }

			inline void kernel_mode(bool km) {
				if (local_state._kernel_mode != km) {
					local_state._kernel_mode = km;
					mmu().cpu_privilege_change(km);
					ensure_privilege_mode();
				}
			}

			inline bool executing_translation() const { return _exec_txl; }

			void invalidate_executed_page(pa_t phys_page_base_addr, va_t virt_page_base_addr);

			inline void schedule_decode_cache_flush() {
				_should_flush_decode_cache = true;
			}

			static inline CPU *get_active_cpu() {
				return current_cpu;
			}

			static inline void set_active_cpu(CPU* cpu) {
				current_cpu = cpu;
			}

			bool verify_check();

			void register_region(captive::shared::RegionWorkUnit *rwu);

			virtual void *reg_state() = 0;
			virtual uint32_t reg_state_size() = 0;

		protected:
			virtual bool decode_instruction_virt(gva_t addr, Decode *insn) = 0;
			virtual bool decode_instruction_phys(gpa_t addr, Decode *insn) = 0;

			inline void inc_insns_executed() {
				cpu_data().insns_executed++;
			}

			Trace *_trace;

			struct {
				bool _kernel_mode;
				uint32_t last_exception_action;
			} local_state;

		private:
			inline void assert_privilege_mode()
			{
				//assert((kernel_mode() && in_kernel_mode()) || (!kernel_mode() && in_user_mode()));
			}

			inline void ensure_privilege_mode()
			{
				// Switch x86 privilege mode, to match the mode of the emulated processor
				if (kernel_mode() && !in_kernel_mode()) {
					//printf("cpu: km=%d, ring=%d switching to ring0\n", kernel_mode(), current_ring());
//					switch_to_kernel_mode();
				} else if (!kernel_mode() && !in_user_mode()) {
					//printf("cpu: km=%d, ring=%d switching to ring3\n", kernel_mode(), current_ring());
//					switch_to_user_mode();
				}
			}

			static CPU *current_cpu;


			profile::Image& _profile_image;
			bool _should_flush_decode_cache;

			Environment& _env;
			PerCPUData *_per_cpu_data;

			bool _exec_txl;

			uint8_t decode_cache[DECODE_CACHE_SIZE];
			uint8_t block_cache[BLOCK_CACHE_SIZE];

			inline Decode *get_decode(uint32_t pc) const {
				return (Decode *)&decode_cache[((pc >> 2) % DECODE_CACHE_ENTRIES) * DECODE_OBJ_SIZE];
			}

			inline jit::GuestBasicBlock *get_block(uint32_t pc) const {
				return (jit::GuestBasicBlock *)&block_cache[((pc >> 2) % BLOCK_CACHE_ENTRIES) * BLOCK_OBJ_SIZE];
			}

			bool check_safepoint();
			bool run_interp();
			bool run_block_jit();
			bool run_region_jit();

			jit::GuestBasicBlock *get_basic_block(uint32_t block_addr);
			bool compile_basic_block(uint32_t block_addr, jit::GuestBasicBlock *block);

			bool handle_pending_action(uint32_t action);
			bool interpret_block();

			void analyse_regions();
			void compile_region(profile::Region& rgn);
		};
	}
}

#endif	/* CPU_H */

