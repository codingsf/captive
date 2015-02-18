/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:38
 */

#ifndef ARMCPU_H
#define	ARMCPU_H

#include <define.h>
#include <cpu.h>

#define DECODE_CACHE_SIZE	8192
#define DECODE_OBJ_SIZE		128
#define DECODE_CACHE_ENTRIES	(DECODE_CACHE_SIZE / DECODE_OBJ_SIZE)

namespace captive {
	namespace arch {
		namespace devices {
			class CoCo;
		}

		namespace arm {
			class ArmEnvironment;
			class ArmInterp;
			class ArmDecode;
			class ArmMMU;

			class ArmCPU : public CPU
			{
				friend class ArmInterp;
				friend class ArmMMU;
				friend class devices::CoCo;

			public:
				ArmCPU(ArmEnvironment& env);
				virtual ~ArmCPU();

				bool init(unsigned int ep) override;
				bool run() override;

				uint32_t read_pc() const override { return state.regs.RB[15]; }

				uint32_t write_pc(uint32_t value) override {
					uint32_t tmp = state.regs.RB[15];
					state.regs.RB[15] = value;
					return tmp;
				}

				uint32_t inc_pc(uint32_t delta) override {
					uint32_t tmp = state.regs.RB[15];
					state.regs.RB[15] += delta;
					return tmp;
				}

				void dump_state() const;

				virtual MMU& mmu() const override { return (MMU&)*_mmu; }

				void handle_angel_syscall();

				struct cpu_state {
					uint32_t last_exception_action;
					uint32_t isa_mode;

					struct {
						uint32_t RB[16];
						uint32_t RB_usr[17];
						uint32_t RB_fiq[17];
						uint32_t RB_irq[17];
						uint32_t RB_svc[17];
						uint32_t RB_abt[17];
						uint32_t RB_und[17];

						uint8_t C, V, Z, N, X, M, F, I, cpV;
						uint32_t SPSR;
					} regs;
				};

			private:
				ArmMMU *_mmu;

				char decode_cache[DECODE_CACHE_SIZE];

				inline ArmDecode *get_decode(uint32_t pc) const {
					return (ArmDecode *)&decode_cache[(pc % DECODE_CACHE_ENTRIES) * DECODE_OBJ_SIZE];
				}

				unsigned int _ep;
				cpu_state state;
			};
		}
	}
}

#endif	/* CPU_H */

