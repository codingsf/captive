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

namespace captive {
	namespace arch {
		namespace devices {
			class CoCo;
		}

		namespace arm {
			class ArmEnvironment;
			class ArmInterp;
			class ArmDecode;
			class ArmJIT;
			class ArmMMU;

			class ArmCPU : public CPU
			{
				friend class ArmInterp;
				friend class ArmMMU;
				friend class devices::CoCo;

			public:
				ArmCPU(ArmEnvironment& env, PerCPUData *per_cpu_data);
				virtual ~ArmCPU();

				bool init() override;

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
				virtual Interpreter& interpreter() const override { return (Interpreter&)*_interp; }
				virtual JIT& jit() const override { return (JIT&)*_jit; }

				struct cpu_state {
					uint32_t isa_mode;

					uint8_t pad[16];

					struct {
						uint32_t RB[16];
						uint32_t RB_usr[17];
						uint32_t RB_fiq[17];
						uint32_t RB_irq[17];
						uint32_t RB_svc[17];
						uint32_t RB_abt[17];
						uint32_t RB_und[17];

						uint8_t C, V, Z, N, X;
						uint32_t SPSR;
						uint8_t M, F, I, cpV;
					} packed regs;
				};

			protected:
				bool decode_instruction(uint32_t addr, Decode* insn) override;
				void* reg_state() override { return &state.regs; }
				uint32_t reg_state_size() override { return sizeof(state.regs); }

			private:
				ArmMMU *_mmu;
				ArmInterp *_interp;
				ArmJIT *_jit;

				cpu_state& state;
			};
		}
	}
}

#endif	/* CPU_H */

