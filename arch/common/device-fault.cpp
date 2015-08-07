#include <mmu.h>
/*
   0:   66 67 89 18             mov    %bx,(%eax)
   4:   66 67 8b 40 18          mov    0x18(%eax),%ax
   9:   66 67 8b 40 30          mov    0x30(%eax),%ax
   e:   67 0f b6 10             movzbl (%eax),%edx
  12:   67 0f b7 02             movzwl (%edx),%eax
  16:   67 66 89 38             mov    %di,(%eax)
  1a:   67 88 08                mov    %cl,(%eax)
  1d:   67 88 18                mov    %bl,(%eax)
  20:   67 89 08                mov    %ecx,(%eax)
  23:   67 89 18                mov    %ebx,(%eax)
  26:   67 8a 40 04             mov    0x4(%eax),%al
  2a:   67 8a 40 08             mov    0x8(%eax),%al
  2e:   67 8a 40 10             mov    0x10(%eax),%al
  32:   67 8a 40 18             mov    0x18(%eax),%al

  36:   67 8b 00                mov    (%eax),%eax
  39:   67 8b 11                mov    (%ecx),%edx
  3c:   67 8b 40 60             mov    0x60(%eax),%eax
 */
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
										case 0x40: // mov xx(%eax), %ax
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
						case 0x88:
						{
								switch (*code++) {
									case 0x08: // mov %cl,(%eax)
									{
										__out8(dev_addr, mctx->rcx);
										mctx->rip += 3;
										return true;
									}
									case 0x18: // mov %bl,(%eax)
									{
										__out8(dev_addr, mctx->rbx);
										mctx->rip += 3;
										return true;
									}
									default: return false;
								}
						}
						case 0x89:
						{
							switch (*code++) {
								case 0x08: // mov %ecx,(%eax)
								{
									__out32(dev_addr, mctx->rcx);
									mctx->rip += 3;
									return true;
								}
								case 0x18: // mov %ebx,(%eax)
								{
									__out32(dev_addr, mctx->rbx);
									mctx->rip += 3;
									return true;
								}
								default: return false;
							}
						}
						case 0x8a:
						{
							switch (*code++) {
								case 0x40: // mov XX(%eax), %al
								{
									mctx->rax |= __in8(dev_addr);
									mctx->rip += 4;
									return true;
								}
								default: return false;
							}
						}
						case 0x8b:
						{
							switch (*code++) {
								case 0x00: // mov (%eax),%eax
								{
									mctx->rax = __in32(dev_addr);
									mctx->rip += 3;
									return true;
								}
								case 0x11: // mov (%ecx),%edx
								{
									mctx->rdx = __in32(dev_addr);
									mctx->rip += 3;
									return true;
								}
								case 0x40: // mov xx(%eax), %eax
								{
									mctx->rax = __in32(dev_addr);
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
	}
}
