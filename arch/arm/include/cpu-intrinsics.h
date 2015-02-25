/*
 * File:   cpu-intrinsics.h
 * Author: spink
 *
 * Created on 11 February 2015, 14:21
 */

#ifndef CPU_INTRINSICS_H
#define	CPU_INTRINSICS_H

#define TRACE

#include <printf.h>
#include <env.h>
#include <mmu.h>

extern volatile captive::arch::MMU::access_type mem_access_type;

#ifdef TRACE

static inline uint32_t trace_read_reg(captive::arch::CPU& cpu, const char *id, uint32_t val)
{
	if (cpu.trace().recording()) cpu.trace().add_reg_read(id, val);
	return val;
}

static inline uint32_t trace_read_reg_bank(captive::arch::CPU& cpu, const char *bank, uint32_t idx, uint32_t val)
{
	if (cpu.trace().recording()) cpu.trace().add_reg_bank_read(bank, idx, val);
	return val;
}

static inline uint32_t trace_write_reg(captive::arch::CPU& cpu, const char *id, uint32_t val)
{
	if (cpu.trace().recording()) cpu.trace().add_reg_write(id, val);
	return val;
}

static inline uint32_t trace_write_reg_bank(captive::arch::CPU& cpu, const char *bank, uint32_t idx, uint32_t val)
{
	if (cpu.trace().recording()) cpu.trace().add_reg_bank_write(bank, idx, val);
	return val;
}

#define read_register(_id) (trace_read_reg(cpu, #_id, cpu.state.regs._id))
#define read_register_nt(_id) read_register(_id)
#define read_register_bank(_bank, _id) (trace_read_reg_bank(cpu, #_bank, _id, cpu.state.regs._bank[_id]))
#define read_register_bank_nt(_bank, _id) read_register_bank(_bank, _id)

#define write_register(_id, _value) (trace_write_reg(cpu, #_id, (cpu.state.regs._id = (_value))))
#define write_register_nt(_id, _value) write_register(_id, _value)
#define write_register_bank(_bank, _id, _value) (trace_write_reg_bank(cpu, #_bank, _id, (cpu.state.regs._bank[_id] = (_value))))
#define write_register_bank_nt(_bank, _id, _value) write_register_bank(_bank, _id, _value)

#else

#define read_register(_id) cpu.state.regs._id
#define read_register_nt(_id) read_register(_id)
#define read_register_bank(_bank, _id) cpu.state.regs._bank[_id]
#define read_register_bank_nt(_bank, _id) read_register_bank(_bank, _id)

#define write_register(_id, _value) cpu.state.regs._id = _value
#define write_register_nt(_id, _value) write_register(_id, _value)
#define write_register_bank(_bank, _id, _value) cpu.state.regs._bank[_id] = _value
#define write_register_bank_nt(_bank, _id, _value) write_register_bank(_bank, _id, _value)

#endif

#define enter_user_mode() cpu.kernel_mode(false)
#define enter_kernel_mode() cpu.kernel_mode(true)

#define push_interrupt(_l)
#define pop_interrupt()
#define pend_interrupt()

#define set_cpu_mode(_v) cpu.state.isa_mode = _v
#define get_cpu_mode() cpu.state.isa_mode

#define trap() do { printf("trap: " __FILE__ " at %d \n", __LINE__); asm volatile ("out %0, $0xff\n" :: "a"(2)); } while(0);
#define halt_cpu() asm volatile ("out %0, $0xff\n" :: "a"(2))

#define write_device(a, b, c) cpu.env().write_core_device(cpu, a, b, c)
#define read_device(a, b, c) cpu.env().read_core_device(cpu, a, b, c)

#define flush_dtlb_entry(v)
#define flush_itlb_entry(v)

static inline uint32_t mem_read_8(uint32_t addr, uint32_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ;
	data = *((uint8_t*)((uint64_t)addr));
	return 0;
}

static inline uint32_t mem_read_16(uint32_t addr, uint32_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ;
	data = *((uint16_t*)((uint64_t)addr));
	return 0;
}

static inline uint32_t mem_read_32(uint32_t addr, uint32_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ;
	data = *((uint32_t*)((uint64_t)addr));
	return 0;
}

static inline uint32_t mem_read_64(uint32_t addr, uint64_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ;
	data = *((uint64_t*)((uint64_t)addr));
	return 0;
}

/*#define mem_read_8(_addr, _data) (_data = *((uint8_t*)((uint64_t)_addr)), page_fault_code)
#define mem_read_16(_addr, _data) (_data = *((uint16_t*)((uint64_t)_addr)), page_fault_code)
#define mem_read_32(_addr, _data) (_data = *((uint32_t*)((uint64_t)_addr)), page_fault_code)
#define mem_read_64(_addr, _data) (_data = *((uint64_t*)((uint64_t)_addr)), page_fault_code)*/

static inline uint32_t mem_write_64(uint32_t addr, uint64_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE;
	*((uint64_t*)((uint64_t)addr)) = ((uint64_t)data);
	return 0;
}

static inline uint32_t mem_write_32(uint32_t addr, uint32_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE;
	*((uint32_t*)((uint64_t)addr)) = ((uint32_t)data);
	return 0;
}

static inline uint32_t mem_write_16(uint32_t addr, uint16_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE;
	*((uint16_t*)((uint64_t)addr)) = ((uint16_t)data);
	return 0;
}

static inline uint32_t mem_write_8(uint32_t addr, uint8_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE;
	*((uint8_t*)((uint64_t)addr)) = ((uint8_t)data);
	return 0;
}

/*#define mem_write_8(_addr, _data) (*((uint8_t*)((uint64_t)_addr)) = ((uint8_t)_data), page_fault_code)
#define mem_write_16(_addr, _data) (*((uint16_t*)((uint64_t)_addr)) = ((uint16_t)_data), page_fault_code)
#define mem_write_32(_addr, _data) (*((uint32_t*)((uint64_t)_addr)) = ((uint32_t)_data), page_fault_code)
#define mem_write_64(_addr, _data) (*((uint64_t*)((uint64_t)_addr)) = ((uint64_t)_data), page_fault_code)*/

static inline uint32_t mem_read_8_user(uint32_t addr, uint32_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ_USER;
	data = *((uint8_t*)((uint64_t)addr));
	return 0;
}

static inline uint32_t mem_read_16_user(uint32_t addr, uint32_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ_USER;
	data = *((uint16_t*)((uint64_t)addr));
	return 0;
}

static inline uint32_t mem_read_32_user(uint32_t addr, uint32_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ_USER;
	data = *((uint32_t*)((uint64_t)addr));
	return 0;
}

static inline uint32_t mem_read_64_user(uint32_t addr, uint64_t& data)
{
	mem_access_type = captive::arch::MMU::ACCESS_READ_USER;
	data = *((uint64_t*)((uint64_t)addr));
	return 0;
}

/*#define mem_read_8_user(_addr, _data) mem_read_8(_addr, _data)
#define mem_read_16_user(_addr, _data) mem_read_16(_addr, _data)
#define mem_read_32_user(_addr, _data) mem_read_32(_addr, _data)
#define mem_read_64_user(_addr, _data) mem_read_64(_addr, _data)*/

static inline uint32_t mem_write_64_user(uint32_t addr, uint64_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE_USER;
	*((uint64_t*)((uint64_t)addr)) = ((uint64_t)data);
	return 0;
}

static inline uint32_t mem_write_32_user(uint32_t addr, uint32_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE_USER;
	*((uint32_t*)((uint64_t)addr)) = ((uint32_t)data);
	return 0;
}

static inline uint32_t mem_write_16_user(uint32_t addr, uint16_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE_USER;
	*((uint16_t*)((uint64_t)addr)) = ((uint16_t)data);
	return 0;
}

static inline uint32_t mem_write_8_user(uint32_t addr, uint8_t data)
{
	mem_access_type = captive::arch::MMU::ACCESS_WRITE_USER;
	*((uint8_t*)((uint64_t)addr)) = ((uint8_t)data);
	return 0;
}

/*#define mem_write_8_user(_addr, _data) mem_write_8(_addr, _data)
#define mem_write_16_user(_addr, _data) mem_write_16(_addr, _data)
#define mem_write_32_user(_addr, _data) mem_write_32(_addr, _data)
#define mem_write_64_user(_addr, _data) mem_write_64(_addr, _data)*/

#endif	/* CPU_INTRINSICS_H */

