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

				void dump_state() const;

				virtual MMU& mmu() const override { return (MMU&)*_mmu; }
				virtual Interpreter& interpreter() const override { return (Interpreter&)*_interp; }
				virtual JIT& jit() const override { return (JIT&)*_jit; }

				struct cpu_state {
					uint32_t isa_mode;

					struct {
						uint32_t RB[16];
						uint32_t RB_usr[17];
						uint32_t RB_fiq[17];
						uint32_t RB_irq[17];
						uint32_t RB_svc[17];
						uint32_t RB_abt[17];
						uint32_t RB_und[17];

						uint8_t C, V, Z, N, X;
						uint32_t SPSR, DACR, TTBR0, TTBR1, FSR, FAR, CP_Status;
						uint8_t M, F, I, cpV;
					} packed regs;
				};

			protected:
				bool decode_instruction_virt(gva_t addr, Decode* insn) override;
				bool decode_instruction_phys(gpa_t addr, Decode* insn) override;
				JumpInfo get_instruction_jump_info(Decode* insn) override;

				bool interrupts_enabled(uint8_t irq_line) const override
				{
					if (irq_line == 0) return !state.regs.F;
					if (irq_line == 1) return !state.regs.I;
					return false;
				}

				void* reg_state() override { return &state.regs; }
				uint32_t reg_state_size() override { return sizeof(state.regs); }

			private:
				ArmMMU *_mmu;
				ArmInterp *_interp;
				ArmJIT *_jit;

				cpu_state state;
			};
		}
	}
}

#endif	/* CPU_H */

