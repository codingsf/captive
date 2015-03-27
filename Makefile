MAKEFLAGS += -rR --no-print-directory
export q := @

top-dir := $(CURDIR)
src-dir := $(top-dir)/src
inc-dir := $(top-dir)/inc
bin-dir := $(top-dir)/bin
bios-dir := $(top-dir)/bios
arch-dir := $(top-dir)/arch

export shared-dir := $(top-dir)/shared

out := $(bin-dir)/captive
src := $(patsubst src/%,$(src-dir)/%,$(shell find src/ | grep -e "\.cpp"))
obj := $(src:.cpp=.o) $(src-dir)/util/spin-lock.o
dep := $(src:.cpp=.d)

bios := $(bios-dir)/bios.bin.o

common-cflags := -I$(inc-dir) -I$(shared-dir) -g -Wall -O0 -pthread
cflags   := $(common-cflags)
cxxflags := $(common-cflags) -std=gnu++11
asflags  := -g
ldflags  := -pthread -Wl,--no-as-needed -lSDL2 -ldl -lz -lncurses

cc  := gcc
cxx := g++
as  := as
ld  := ld
rm  := rm
make := +make

all: $(out) .FORCE
	$(q)$(make) -C $(arch-dir)

clean: .FORCE
	$(q)$(make) -C $(bios-dir) clean
	$(q)$(make) -C $(arch-dir) clean
	$(rm) -f $(obj)
	$(rm) -f $(dep)
	$(rm) -f $(out)

$(bios): .FORCE
	$(q)$(make) -C $(bios-dir)

$(out): $(dep) $(obj) $(bios)
	@echo "  LD      $(patsubst $(bin-dir)/%,%,$@)"
	$(q)$(cxx) -o $@ $(ldflags) $(obj) $(bios) `llvm-config --libs engine ipo x86 mcjit`

%.o: %.cpp
	@echo "  C++     $(patsubst $(src-dir)/%,%,$@)"
	$(q)$(cxx) -c -o $@ $(cxxflags) $<

%.o: %.S
	@echo "  AS      $(patsubst $(src-dir)/%,%,$@)"
	$(q)$(as) -o $@ $(asflags) $<

#$(src-dir)/jit/jit.o: $(src-dir)/jit/jit.cpp
#	@echo "  C++ [L] $(patsubst $(src-dir)/%,%,$@)"
#	$(q)$(cxx) -c -o $@ $(cxxflags) `llvm-config --cxxflags` $<
#
#$(src-dir)/jit/llvm.o: $(src-dir)/jit/llvm.cpp
#	@echo "  C++ [L] $(patsubst $(src-dir)/%,%,$@)"
#	$(q)$(cxx) -c -o $@ $(cxxflags) `llvm-config --cxxflags` $<
#
#$(src-dir)/jit/llvm-mm.o: $(src-dir)/jit/llvm-mm.cpp
#	@echo "  C++ [L] $(patsubst $(src-dir)/%,%,$@)"
#	$(q)$(cxx) -c -o $@ $(cxxflags) `llvm-config --cxxflags` $<

%.d: %.cpp
	$(q)$(cxx) -M -MT $(@:.d=.o) -o $@ $(cxxflags) $<

.FORCE:
.PHONY: $(PHONY) .FORCE

-include $(dep)
