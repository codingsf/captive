MAKEFLAGS += -rR --no-print-directory
q := @

top-dir := $(CURDIR)
src-dir := $(top-dir)/src
inc-dir := $(top-dir)/inc
bin-dir := $(top-dir)/bin
bios-dir := $(top-dir)/bios
engines-dir := $(top-dir)/engines

out := $(bin-dir)/captive
src := $(patsubst src/%,$(src-dir)/%,$(shell find src/ | grep -e "\.cpp"))
obj := $(src:.cpp=.o)
dep := $(src:.cpp=.d)

bios := $(bios-dir)/bios.bin.o

common-cflags := -I$(inc-dir) -g -Wall -O0
cflags   := $(common-cflags)
cxxflags := $(common-cflags) -std=gnu++11
ldflags  :=

cc  := gcc
cxx := g++
ld  := ld
rm  := rm
make := make

all: $(out)
	$(q)$(make) -C $(engines-dir)

clean:
	$(q)$(make) -C $(bios-dir) clean
	$(q)$(make) -C $(engines-dir) clean
	$(rm) -f $(obj)
	$(rm) -f $(dep)
	$(rm) -f $(out)

$(bios): .FORCE
	$(q)$(make) -C $(bios-dir)

$(out): $(dep) $(obj) $(bios)
	@echo "  LD      $(patsubst $(bin-dir)/%,%,$@)"
	$(q)$(cxx) -o $@ $(ldflags) $(obj) $(bios)

%.o: %.cpp
	@echo "  C++     $(patsubst $(src-dir)/%,%,$@)"
	$(q)$(cxx) -c -o $@ $(cxxflags) $<

%.d: %.cpp
	$(q)$(cxx) -M -MT $(@:.d=.o) -o $@ $(cxxflags) $<

.FORCE:
.PHONY: $(PHONY) .FORCE

-include $(dep)
