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
#include <shared-jit.h>
#include <txln-cache.h>

#define DECODE_CACHE_SIZE	8192
#define DECODE_OBJ_SIZE		128
#define DECODE_CACHE_ENTRIES	(DECODE_CACHE_SIZE / DECODE_OBJ_SIZE)

extern "C" { void tail_call_ret0_only(); }

namespace captive {
	namespace shared {
		struct BlockTranslation;
		struct RegionTranslation;
		struct RegionWorkUnit;
		struct RegionImage;
	}

	namespace arch {
		namespace jit {
			class TranslationContext;
		}

		namespace profile {
			struct Image;
			struct Region;
			struct Block;
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
					if (km) switch_to_kernel_mode(); else switch_to_user_mode();
				}
			}

			inline bool executing_translation() const { return _exec_txl; }

			static inline CPU *get_active_cpu() {
				return current_cpu;
			}

			static inline void set_active_cpu(CPU* cpu) {
				current_cpu = cpu;
			}

			bool verify_check();

			virtual void *reg_state() = 0;
			virtual uint32_t reg_state_size() = 0;

			void invalidate_virtual_mappings();
			void invalidate_virtual_mapping(gva_t va);
			void invalidate_translations();
			void invalidate_translation(pa_t phys_page_base_addr, va_t virt_page_base_addr);

			void register_region(shared::RegionWorkUnit *rwu);

			void handle_irq_raised(uint8_t irq_line);
			void handle_irq_rescinded(uint8_t irq_line);

		protected:
			volatile uint32_t *_pc_reg_ptr;

			virtual bool decode_instruction_virt(gva_t addr, Decode *insn) = 0;
			virtual bool decode_instruction_phys(gpa_t addr, Decode *insn) = 0;
			virtual JumpInfo get_instruction_jump_info(Decode *insn) = 0;

			virtual bool interrupts_enabled(uint8_t irq_line) const = 0;

			inline void inc_insns_executed() {
				cpu_data().insns_executed++;
			}

			Trace *_trace;

			struct {
				volatile bool _kernel_mode;
				uint32_t last_exception_action;
			} local_state;

			struct block_chain_cache_entry {
				uint32_t tag;
				void *fn;
				
				inline void invalidate() { tag = 1; }
			} packed;

			struct region_chain_cache_entry {
				void *fn;
				
				inline void invalidate() { fn = (void *)&tail_call_ret0_only; }
			} packed;

			struct {
				void *cpu;												// 0
				void *registers;										// 8
				uint64_t registers_size;								// 16
				const struct region_chain_cache_entry *region_txln_cache;		// 24
				const struct block_chain_cache_entry *block_txln_cache;		// 32
				uint64_t *insn_counter;									// 40
				uint8_t exit_chain;										// 48
			} packed jit_state;

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

			typedef Cache<struct region_chain_cache_entry, 0x100000> region_txln_cache_t;
			typedef Cache<struct block_chain_cache_entry, 0x10000> block_txln_cache_t;
			region_txln_cache_t *region_txln_cache;
			block_txln_cache_t  *block_txln_cache;

			bool _exec_txl;

			uint8_t decode_cache[DECODE_CACHE_SIZE];

			inline Decode *get_decode(uint32_t pc) const {
				return (Decode *)&decode_cache[((pc >> 2) % DECODE_CACHE_ENTRIES) * DECODE_OBJ_SIZE];
			}

			profile::Image *image;

			bool run_interp();
			bool run_interp_safepoint();
			bool run_block_jit();
			bool run_block_jit_safepoint();
			bool run_region_jit();
			bool run_region_jit_safepoint();
			bool run_test();

			bool handle_pending_action(uint32_t action);
			bool interpret_block();

			void analyse_blocks();
			void compile_region(profile::Region *rgn, uint32_t region_index);

			enum block_compilation_mode
			{
				MODE_BLOCK,
				MODE_REGION,
			};
			
			captive::shared::block_txln_fn compile_block(profile::Block *blk, gpa_t pa, enum block_compilation_mode mode);
			bool translate_block(jit::TranslationContext& ctx, gpa_t pa);
		};
	}
}

#endif	/* CPU_H */

