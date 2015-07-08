/*
 * File:   cpu-intrinsics.h
 * Author: spink
 *
 * Created on 11 February 2015, 14:21
 */

#if defined(DEFINE_INTRINSICS) && !defined(INTRINSICS_DEFINED)
#define INTRINSICS_DEFINED

//#define TRACE

#include <printf.h>
#include <env.h>
#include <mmu.h>
#include <priv.h>

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

static inline void trap() { printf("trap: " __FILE__ " at %d \n", __LINE__); asm volatile ("out %0, $0xff\n" :: "a"(2)); }

#define halt_cpu() asm volatile ("out %0, $0xff\n" :: "a"(2))

#define write_device(a, b, c) cpu.env().write_core_device(cpu, a, b, c)
#define read_device(a, b, c) cpu.env().read_core_device(cpu, a, b, c)

static inline void flush_itlb_entry(uint32_t v) { asm volatile ("int $0x82\n" :: "D"(4), "S"(v)); }
static inline void flush_dtlb_entry(uint32_t v) { asm volatile ("int $0x82\n" :: "D"(5), "S"(v)); }
static inline void flush_itlb() { asm volatile ("int $0x82\n" :: "D"(2)); }
static inline void flush_dtlb() { asm volatile ("int $0x82\n" :: "D"(3)); }

#define __mem_read(_addr, _type) (*((_type *)((uint64_t)(_addr))))
#define __mem_write(_addr, _data, _type) *((_type *)((uint64_t)(_addr))) = (_data)

#define mem_read_8(_addr) __mem_read(_addr, uint8_t)
#define mem_read_16(_addr) __mem_read(_addr, uint16_t)
#define mem_read_32(_addr) __mem_read(_addr, uint32_t)
#define mem_read_64(_addr) __mem_read(_addr, uint64_t)

#define mem_write_8(_addr, _data) __mem_write(_addr, _data, uint8_t)
#define mem_write_16(_addr, _data) __mem_write(_addr, _data, uint16_t)
#define mem_write_32(_addr, _data) __mem_write(_addr, _data, uint32_t)
#define mem_write_64(_addr, _data) __mem_write(_addr, _data, uint64_t)

#define define_mem_read_user_func(_size, _type) static inline _type __mem_read_##_size ## _user(captive::arch::CPU& cpu, uint32_t addr) \
{ \
	if (cpu.kernel_mode()) { switch_to_user_mode(); } \
	_type value = mem_read_##_size(addr); \
	if (cpu.kernel_mode()) { switch_to_kernel_mode(); } \
	return value; \
}

define_mem_read_user_func(8, uint8_t)
define_mem_read_user_func(16, uint16_t)
define_mem_read_user_func(32, uint32_t)
define_mem_read_user_func(64, uint64_t)

#define mem_read_8_user(_addr) __mem_read_8_user(cpu, _addr)
//#define mem_read_8_user(_addr) mem_read_8(_addr)
#define mem_read_16_user(_addr) __mem_read_16_user(cpu, _addr)
//#define mem_read_16_user(_addr) mem_read_16(_addr)
#define mem_read_32_user(_addr) __mem_read_32_user(cpu, _addr)
//#define mem_read_32_user(_addr) mem_read_32(_addr)
#define mem_read_64_user(_addr) __mem_read_64_user(cpu, _addr)
//#define mem_read_64_user(_addr) mem_read_64(_addr)

#define define_mem_write_user_func(_size, _type) static inline void __mem_write_##_size ## _user(captive::arch::CPU& cpu, uint32_t addr, _type data) \
{ \
	if (cpu.kernel_mode()) { switch_to_user_mode(); } \
	mem_write_##_size(addr, data); \
	if (cpu.kernel_mode()) { switch_to_kernel_mode(); } \
}

define_mem_write_user_func(8, uint8_t)
define_mem_write_user_func(16, uint16_t)
define_mem_write_user_func(32, uint32_t)
define_mem_write_user_func(64, uint64_t)

#define mem_write_8_user(_addr, _data) __mem_write_8_user(cpu, _addr, _data)
//#define mem_write_8_user(_addr, _data) mem_write_8(_addr, _data)
#define mem_write_16_user(_addr, _data) __mem_write_16_user(cpu, _addr, _data)
//#define mem_write_16_user(_addr, _data) mem_write_16(_addr, _data)
#define mem_write_32_user(_addr, _data) __mem_write_32_user(cpu, _addr, _data)
//#define mem_write_32_user(_addr, _data) mem_write_32(_addr, _data)
#define mem_write_64_user(_addr, _data) __mem_write_64_user(cpu, _addr, _data)
//#define mem_write_64_user(_addr, _data) mem_write_64(_addr, _data)

#endif

#ifdef UNDEFINE_INTRINSICS
#undef read_register
#undef read_register_bank
#undef write_register
#undef write_register_bank

#undef enter_user_mode
#undef enter_kernel_mode

#undef push_interrupt
#undef pop_interrupt
#undef pend_interrupt

#undef set_cpu_mode
#undef get_cpu_mode

#undef halt_cpu

#undef write_device
#undef read_device

#endif
