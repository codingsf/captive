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

#include <map>

#define DECODE_CACHE_SIZE	8192
#define DECODE_OBJ_SIZE		128
#define DECODE_CACHE_ENTRIES	(DECODE_CACHE_SIZE / DECODE_OBJ_SIZE)

namespace captive {
	namespace shared {
		struct RegionWorkUnit;
		struct TranslationBlock;
	}

	namespace arch {
		namespace jit {
			class TranslationContext;
			typedef uint32_t (*block_txln_fn)(void *);
		}

		class Environment;
		class MMU;
		class Interpreter;
		class Decode;
		class JumpInfo;
		class JIT;

		class CPU
		{
		public:
			CPU(Environment& env, PerCPUData *per_cpu_data);
			virtual ~CPU();

			virtual bool init() = 0;
			bool run();

			bool device_write(uint32_t address, uint8_t length, uint64_t value);
			bool device_read(uint32_t address, uint8_t length, uint64_t& value);

			virtual MMU& mmu() const = 0;
			virtual Interpreter& interpreter() const = 0;
			virtual JIT& jit() const = 0;

			inline Environment& env() const { return _env; }
			inline Trace& trace() const { return *_trace; }
			inline PerCPUData& cpu_data() const { return *_per_cpu_data; }

			inline uint32_t read_pc() const { return *_pc_reg_ptr; }
			inline uint32_t write_pc(uint32_t new_pc_val) { uint32_t tmp = *_pc_reg_ptr; *_pc_reg_ptr = new_pc_val; return tmp; }
			inline uint32_t inc_pc(uint32_t delta) { uint32_t tmp = *_pc_reg_ptr; *_pc_reg_ptr += delta; return tmp; }

			virtual void dump_state() const = 0;

			inline uint64_t get_insns_executed() const { return cpu_data().insns_executed; }

			inline bool kernel_mode() const { return local_state._kernel_mode; }

			inline void kernel_mode(bool km) {
				if (local_state._kernel_mode != km) {
					local_state._kernel_mode = km;
					ensure_privilege_mode();
				}
			}

			inline bool executing_translation() const { return _exec_txl; }

			void invalidate_executed_page(pa_t phys_page_base_addr, va_t virt_page_base_addr);

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

			void clear_block_cache();

			void tlb_flush();

		protected:
			volatile uint32_t *_pc_reg_ptr;
			
			virtual bool decode_instruction_virt(gva_t addr, Decode *insn) = 0;
			virtual bool decode_instruction_phys(gpa_t addr, Decode *insn) = 0;
			virtual JumpInfo get_instruction_jump_info(Decode *insn) = 0;

			virtual bool interrupts_enabled() const = 0;
			
			inline void inc_insns_executed() {
				cpu_data().insns_executed++;
			}

			Trace *_trace;

			struct {
				volatile bool _kernel_mode;
				uint32_t last_exception_action;
			} local_state;

			struct {
				void *cpu;							
				void *registers;					
				uint32_t registers_size;			
				void **region_chaining_table;		
				uint64_t *insn_counter;				
			} jit_state packed;

		private:
			inline void assert_privilege_mode()
			{
				assert((kernel_mode() && in_kernel_mode()) || (!kernel_mode() && in_user_mode()));
			}

			inline void ensure_privilege_mode()
			{
				// Switch x86 privilege mode, to match the mode of the emulated processor
				if (kernel_mode()) {
					//printf("cpu: km=%d, ring=%d switching to ring0\n", kernel_mode(), current_ring());
					switch_to_kernel_mode();
				} else if (!kernel_mode()) {
					//printf("cpu: km=%d, ring=%d switching to ring3\n", kernel_mode(), current_ring());
					switch_to_user_mode();
				}
			}

			static CPU *current_cpu;

			Environment& _env;
			PerCPUData *_per_cpu_data;

			bool _exec_txl;

			uint8_t decode_cache[DECODE_CACHE_SIZE];

			inline Decode *get_decode(uint32_t pc) const {
				return (Decode *)&decode_cache[((pc >> 2) % DECODE_CACHE_ENTRIES) * DECODE_OBJ_SIZE];
			}

			struct block_txln_cache_entry {
				uint32_t tag;
				uint32_t count;
				jit::block_txln_fn fn;
			};

			struct block_txln_cache_entry *block_txln_cache;
			const uint32_t block_txln_cache_size;

			inline struct block_txln_cache_entry *get_block_txln_cache_entry(gpa_t phys_addr) const
			{
				return &block_txln_cache[((uint32_t)phys_addr >> 1) % block_txln_cache_size];
			}

			bool run_interp();
			bool run_interp_safepoint();
			bool run_block_jit();
			bool run_block_jit_safepoint();
			bool run_test();

			bool handle_pending_action(uint32_t action);
			bool interpret_block();

			void analyse_blocks();
			jit::block_txln_fn compile_block(gpa_t pa);
			bool translate_block(jit::TranslationContext& ctx, gpa_t pa);
		};
	}
}

#endif	/* CPU_H */

