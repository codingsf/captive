#include <define.h>
#include <printf.h>
#include <cpu.h>
#include <env.h>
#include <shared-jit.h>
#include <map>

using namespace captive::arch;
using namespace captive::shared;

typedef void (*call_fn_0)(CPU *cpu);
typedef void (*call_fn_1)(CPU *cpu, uint64_t arg0);
typedef void (*call_fn_2)(CPU *cpu, uint64_t arg0, uint64_t arg1);
typedef void (*call_fn_3)(CPU *cpu, uint64_t arg0, uint64_t arg1, uint64_t arg2);
typedef void (*call_fn_4)(CPU *cpu, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3);
typedef void (*call_fn_5)(CPU *cpu, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

typedef std::map<uint32_t, uint64_t> vreg_map_t;

typedef struct _interp_ctx {
	vreg_map_t vregs;
	//uint64_t regs[16];
} interp_ctx_t;

static uint64_t load_value(interp_ctx_t& ctx, IROperand& operand)
{
	uint32_t mask;
	switch (operand.size) {
	case 1: mask = 0xff; break;
	case 2: mask = 0xffff; break;
	case 4: mask = 0xffffffff; break;
	default: fatal("UNHANDLED OPERAND WIDTH\n");
	}
	
	switch (operand.type) {
	case IROperand::CONSTANT:
		return operand.value & mask;
	case IROperand::VREG:
		assert(operand.is_allocated());
		//return ctx.regs[operand.alloc_data] & mask;
		return ctx.vregs[operand.value] & mask;
	default:
		fatal("UNHANDLED OPERAND TYPE\n");
	}
}

static void store_value(interp_ctx_t& ctx, IROperand& operand, uint64_t value)
{
	uint32_t mask;
	switch (operand.size) {
	case 1: mask = 0xff; break;
	case 2: mask = 0xffff; break;
	case 4: mask = 0xffffffff; break;
	default: fatal("UNHANDLED OPERAND WIDTH\n");
	}
	
	switch (operand.type) {
	case IROperand::VREG:
		assert(operand.is_allocated());
		//ctx.regs[operand.alloc_data] = (value & mask);
		ctx.vregs[operand.value] = (value & mask);
		break;
		
	default:
		fatal("UNHANDLED OPERAND TYPE\n");
	}
}

static bool execute_ir(CPU *cpu, IRInstruction *ir, uint32_t count)
{
	interp_ctx_t ctx;
	uint8_t *regs = (uint8_t *)cpu->reg_state();

	IRInstruction *current = ir;
	
	do {
		IROperand& oper0 = current->operands[0];
		IROperand& oper1 = current->operands[1];
		IROperand& oper2 = current->operands[2];
		
		switch (current->type) {
		case IRInstruction::BARRIER:
		case IRInstruction::NOP:
		case IRInstruction::SET_CPU_MODE:
			break;
			
		case IRInstruction::FLUSH:
		case IRInstruction::FLUSH_DTLB:
		case IRInstruction::FLUSH_DTLB_ENTRY:
		case IRInstruction::FLUSH_ITLB:
		case IRInstruction::FLUSH_ITLB_ENTRY:
			cpu->mmu().invalidate_virtual_mappings();
			break;
			
		case IRInstruction::MOV:
			store_value(ctx, oper1, load_value(ctx, oper0));
			break;
			
		case IRInstruction::TRUNC:
			store_value(ctx, oper1, load_value(ctx, oper0));
			break;
		
		case IRInstruction::ZX:
		{
			uint32_t mask;
			switch (current->operands[0].size) {
			case 1: mask = 0xff; break;
			case 2: mask = 0xffff; break;
			case 4: mask = 0xffffffff; break;
			default: fatal("UNHANDLED OPERAND WIDTH FOR TRUNCATE\n");
			}
			
			store_value(ctx, oper1, load_value(ctx, oper0) & mask);
			break;
		}
		
		case IRInstruction::ADC_WITH_FLAGS:
		{
			uint32_t a = load_value(ctx, current->operands[0]);
			uint32_t b = load_value(ctx, current->operands[1]);
			uint8_t c = !!load_value(ctx, current->operands[2]);
			
			asm volatile("mov $0xff, %%al\n"
			"add %4, %%al\n"
			"mov %5, %%eax\n"
			"adc %6, %%eax\n"
			"seto %0\n"
			"sets %1\n"
			"setc %2\n"
			"setz %3" 
			: "=m"(*cpu->tagged_registers().V), "=m"(*cpu->tagged_registers().N), "=m"(*cpu->tagged_registers().C), "=m"(*cpu->tagged_registers().Z)
			: "r"(c), "r"(a), "r"(b) : "eax");
			
			break;
		}
		
		case IRInstruction::ADD:
			store_value(ctx, oper1, load_value(ctx, oper1) + load_value(ctx, oper0));
			break;
			
		case IRInstruction::MUL:
			store_value(ctx, oper1, load_value(ctx, oper1) * load_value(ctx, oper0));
			break;
			
		case IRInstruction::SUB:
			store_value(ctx, oper1, load_value(ctx, oper1) - load_value(ctx, oper0));
			break;
			
		case IRInstruction::DIV:
			store_value(ctx, oper1, load_value(ctx, oper1) / load_value(ctx, oper0));
			break;
			
		case IRInstruction::MOD:
			store_value(ctx, oper1, load_value(ctx, oper1) % load_value(ctx, oper0));
			break;
			
		case IRInstruction::AND:
			store_value(ctx, oper1, load_value(ctx, oper1) & load_value(ctx, oper0));
			break;
			
		case IRInstruction::OR:
			store_value(ctx, oper1, load_value(ctx, oper1) | load_value(ctx, oper0));
			break;
			
		case IRInstruction::XOR:
			store_value(ctx, oper1, load_value(ctx, oper1) ^ load_value(ctx, oper0));
			break;
			
		case IRInstruction::SHL:
			store_value(ctx, oper1, load_value(ctx, oper1) << load_value(ctx, oper0));
			break;

		case IRInstruction::SHR:
			store_value(ctx, oper1, load_value(ctx, oper1) >> load_value(ctx, oper0));
			break;

		case IRInstruction::SAR:
			switch (oper1.size) {
			case 1:
				store_value(ctx, oper1, (int8_t)load_value(ctx, oper1) >> load_value(ctx, oper0));
				break;
			case 2:
				store_value(ctx, oper1, (int16_t)load_value(ctx, oper1) >> load_value(ctx, oper0));
				break;
			case 4:
				store_value(ctx, oper1, (int32_t)load_value(ctx, oper1) >> load_value(ctx, oper0));
				break;
			case 8:
				store_value(ctx, oper1, (int64_t)load_value(ctx, oper1) >> load_value(ctx, oper0));
				break;
			}
			
			break;
			
		case IRInstruction::CLZ:
			store_value(ctx, oper1, __builtin_clz(load_value(ctx, oper0)));
			break;
			
		case IRInstruction::CMPEQ:
			store_value(ctx, oper2, load_value(ctx, oper0) == load_value(ctx, oper1));
			break;
			
		case IRInstruction::CMPNE:
			store_value(ctx, oper2, load_value(ctx, oper0) != load_value(ctx, oper1));
			break;

		case IRInstruction::CMPLT:
			store_value(ctx, oper2, load_value(ctx, oper0) < load_value(ctx, oper1));
			break;

		case IRInstruction::CMPLTE:
			store_value(ctx, oper2, load_value(ctx, oper0) <= load_value(ctx, oper1));
			break;

		case IRInstruction::CMPGT:
			store_value(ctx, oper2, load_value(ctx, oper0) > load_value(ctx, oper1));
			break;

		case IRInstruction::CMPGTE:
			store_value(ctx, oper2, load_value(ctx, oper0) >= load_value(ctx, oper1));
			break;

		case IRInstruction::INCPC:
			cpu->inc_pc(load_value(ctx, oper0));
			break;
			
		case IRInstruction::LDPC:
			store_value(ctx, oper0, cpu->read_pc());
			break;
			
		case IRInstruction::CALL:
		{	
			int nr_args = 0;
			uint64_t args[5];
			
			for (int i = 1; i < 6; i++) {
				if (current->operands[i].is_valid()) {
					args[nr_args] = load_value(ctx, current->operands[i]);
					nr_args++;
				} else {
					break;
				}
			}
			
			switch(nr_args) {
			case 0:
			{
				call_fn_0 fn = (call_fn_0)oper0.value;
				fn(cpu);
				break;
			}
			case 1:
			{
				call_fn_1 fn = (call_fn_1)oper0.value;
				fn(cpu, args[0]);
				break;
			}
			case 2:
			{
				call_fn_2 fn = (call_fn_2)oper0.value;
				fn(cpu, args[0], args[1]);
				break;
			}
			case 3:
			{
				call_fn_3 fn = (call_fn_3)oper0.value;
				fn(cpu, args[0], args[1], args[2]);
				break;
			}
			case 4:
			{
				call_fn_4 fn = (call_fn_4)oper0.value;
				fn(cpu, args[0], args[1], args[2], args[3]);
				break;
			}
			case 5:
			{
				call_fn_5 fn = (call_fn_5)oper0.value;
				fn(cpu, args[0], args[1], args[2], args[3], args[4]);
				break;
			}
			default:
				fatal("UNHANDLED ARGUMENT COUNT %d FOR CALL\n", nr_args);
			}
			
			break;
		}
		
		case IRInstruction::JMP:
			for (uint32_t i = 0; i < count; i++) {
				if (ir[i].ir_block == oper0.value) {
					current = &ir[i];
					break;
				}
			}
			continue;
			
		case IRInstruction::BRANCH:
			IRBlockId target;
			
			if (load_value(ctx, oper0)) {
				target = oper1.value;
			} else {
				target = oper2.value;
			}
			
			for (uint32_t i = 0; i < count; i++) {
				if (ir[i].ir_block == target) {
					current = &ir[i];
					break;
				}
			}
			continue;

		case IRInstruction::RET:
			return true;
			
		case IRInstruction::READ_MEM:
		{
			uint64_t addr = load_value(ctx, oper0) + load_value(ctx, oper1);
			
			switch (oper2.size) {
			case 1: store_value(ctx, oper2, *(uint8_t *)(addr)); break;
			case 2: store_value(ctx, oper2, *(uint16_t *)(addr)); break;
			case 4:	store_value(ctx, oper2, *(uint32_t *)(addr)); break;
			case 8:	store_value(ctx, oper2, *(uint64_t *)(addr)); break;
			}
			
			break;
		}
			
		case IRInstruction::WRITE_MEM:
		{
			uint64_t val = load_value(ctx, oper0), addr = load_value(ctx, oper2) + load_value(ctx, oper1);
			
			switch (oper0.size) {
			case 1:	*(uint8_t *)(addr) = val; break;
			case 2:	*(uint16_t *)(addr) = val; break;
			case 4:	*(uint32_t *)(addr) = val; break;
			case 8:	*(uint64_t *)(addr) = val; break;
			}
			
			break;
		}
			
		case IRInstruction::READ_REG:
			switch (oper1.size) {
			case 1:	store_value(ctx, oper1, regs[load_value(ctx, oper0)]); break;
			case 2:	store_value(ctx, oper1, *(uint16_t *)(&regs[load_value(ctx, oper0)])); break;
			case 4:	store_value(ctx, oper1, *(uint32_t *)(&regs[load_value(ctx, oper0)])); break;
			case 8:	store_value(ctx, oper1, *(uint64_t *)(&regs[load_value(ctx, oper0)])); break;
			}
			
			break;
			
		case IRInstruction::WRITE_REG:
			switch (oper0.size) {
			case 1:	regs[load_value(ctx, oper1)] = load_value(ctx, oper0); break;
			case 2:	*(uint16_t *)(&regs[load_value(ctx, oper1)]) = load_value(ctx, oper0); break;
			case 4:	*(uint32_t *)(&regs[load_value(ctx, oper1)]) = load_value(ctx, oper0); break;
			case 8:	*(uint64_t *)(&regs[load_value(ctx, oper1)]) = load_value(ctx, oper0); break;
			}
			
			break;
			
		case IRInstruction::READ_DEVICE:
		{
			uint32_t data;
			
			cpu->env().read_core_device(*cpu, load_value(ctx, oper0), load_value(ctx, oper1), data);
			store_value(ctx, oper2, data);
			break;
		}
			
		case IRInstruction::WRITE_DEVICE:
			cpu->env().write_core_device(*cpu, load_value(ctx, oper0), load_value(ctx, oper1), load_value(ctx, oper2));
			break;
			
		default:
			fatal("UNHANDLED INSTRUCTION TYPE: %d\n", current->type);
			return false;
		}
		
		current++;
	} while(true);
}

uint32_t interpret_ir(void *_cpu, void *ir, uint32_t count)
{
	//printf("executing @ %08x\n", ((CPU *)_cpu)->read_pc());
	//((CPU *)_cpu)->dump_state();
	
	return execute_ir((CPU *)_cpu, (IRInstruction *)ir, count) ? 0 : 1;
}
