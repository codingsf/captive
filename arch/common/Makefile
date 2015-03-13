out := common.a

top-dir := $(CURDIR)
inc-dir := $(top-dir)/include

src := $(patsubst %,$(top-dir)/%,$(shell find | grep -E "\.c(|pp)"))
obj := $(src:.cpp=.o) trap.o safepoint.o priv.o
dep := $(src:.cpp=.d)

export cflags := -I$(inc-dir) $(common-cflags)
export cxxflags := -I$(inc-dir) $(common-cflags)
export asflags := -nostdlib -g -mcmodel=large

all: $(out)

clean: .FORCE
	$(rm) -f $(out)
	$(rm) -f $(obj)
	$(rm) -f $(dep)

$(out): $(obj)
	@echo "  AR      $(notdir $@)"
	$(q)$(ar) r $@ $(obj)

%.o: %.c
	@echo "  CC      $(notdir $@)"
	$(q)$(cc) -c -o $@ $(cflags) $<

%.o: %.cpp
	@echo "  C++     $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(cxxflags) $<

%.o: %.S
	@echo "  AS      $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(asflags) $<

%.d: %.cpp
	$(q)$(cxx) -M -MT $(@:.d=.o) -o $@ $(cxxflags) $<

.FORCE:
.PHONY: $(PHONY) .FORCE

-include $(dep)
