q   := @

out := bios.bin.o
bin := bios.bin
elf := bios.elf
obj := entry.o tramp.o

ld := ld
cc := gcc
as := as
rm := rm
objcopy := objcopy

cflags := -g -Os -ffreestanding -m64 -Wall -fno-asynchronous-unwind-tables
ldflags := -static -Tlinker.ld -nostdlib
	#-melf_i386

all: $(out)

clean:
	$(rm) -f $(out)
	$(rm) -f $(elf)
	$(rm) -f $(obj)
	$(rm) -f $(bin)

$(out): $(bin) bios.bin.S
	@echo "  CC      $@"
	$(q)$(cc) -c -o $@ bios.bin.S

$(bin): $(elf)
	@echo "  OBJCOPY $@"
	$(q)$(objcopy) -S -O binary --gap-fill 0x00 $(elf) $@

$(elf): $(obj) linker.ld
	@echo "  LD      $@"
	$(q)$(ld) -o $@ $(ldflags) $(obj)

%.o: %.c
	@echo "  CC      $@"
	$(q)$(cc) -c -o $@ $(cflags) $<

%.o: %.S
	@echo "  AS      $@"
	$(q)$(cc) -c -o $@ $(cflags) $<
