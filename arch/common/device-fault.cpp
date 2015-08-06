#include <mmu.h>

static inline uint32_t __in32(uint32_t addr)
{
	uint32_t value;
	asm ("inl $0xf0, %0\n" : "=a"(value) : "d"(addr));
	return value;
}

static inline uint16_t __in16(uint32_t addr)
{
	uint16_t value;
	asm ("inw $0xf0, %0\n" : "=a"(value) : "d"(addr));
	return value;
}

static inline uint8_t __in8(uint32_t addr)
{
	uint8_t value;
	asm ("inb $0xf0, %0\n" : "=a"(value) : "d"(addr));
	return value;
}

static inline void __out32(uint32_t addr, uint32_t val)
{
	asm volatile("outl %0, $0xf0\n" :: "a"(val), "d"(addr));
}

static inline void __out16(uint32_t addr, uint16_t val)
{
	asm volatile("outw %0, $0xf0\n" :: "a"(val), "d"(addr));
}
static inline void __out8(uint32_t addr, uint8_t val)
{
	asm volatile("outb %0, $0xf0\n" :: "a"(val), "d"(addr));
}

namespace captive {
	namespace arch {
		bool fast_handle_device_fault(captive::arch::CPU *core, struct mcontext *mctx, gpa_t dev_addr)
		{
			const uint8_t *code = (const uint8_t *)mctx->rip;

			switch (*code++) {
				case 0x66:
				{
					switch (*code++) {
						case 0x67:
						{
							switch (*code++) {
								case 0x89:
								{
									switch (*code++) {
										case 0x18: // mov %bx,(%eax)
										{
											__out16(dev_addr, mctx->rbx);
											mctx->rip += 4;
											return true;
										}
										default: return false;
									}
								}
								case 0x8b:
								{
									switch (*code++) {
										case 0x40:
										{
											switch (*code++) {
												case 0x18: // mov 0x18(%eax),%ax
												{
													mctx->rax |= __in16(dev_addr);
													mctx->rip += 5;
													return true;
												}
												case 0x30: // mov 0x30(%eax),%ax
												{
													mctx->rax |= __in16(dev_addr);
													mctx->rip += 5;
													return true;													
												}
												default: return false;
											}
										}
										default: return false;
									}
								}
								default: return false;
							}
						}
						default: return false;
					}
				}
				case 0x67:
				{
					switch (*code++) {
						case 0x0f:
						{
							switch (*code++) {
								case 0xb6:
								{
									switch (*code++) {
										case 0x10: // movzbl (%eax),%edx
										{
											mctx->rdx = __in8(dev_addr);
											mctx->rip += 4;
											return true;
										}
										default: return false;
									}
								}
								case 0xb7:
								{
									switch (*code++) {
										case 0x02: // movzwl (%edx),%eax
										{
											mctx->rax = __in16(dev_addr);
											mctx->rip += 4;
											return true;
										}
										default: return false;
									}
								}
								default: return false;
							}
						}
						case 0x66:
						{
							switch (*code++) {
								case 0x89:
								{
									switch (*code++) {
										case 0x38: // mov %di,(%eax)
										{
											__out16(dev_addr, mctx->rdi);
											mctx->rip += 4;
											return true;
										}
										default: return false;
									}
								}
								default: return false;
							}
						}
						default: return false;
					}
				}
				default: return false;
			}
		}
	}
}
