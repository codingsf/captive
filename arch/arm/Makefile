top-dir := $(CURDIR)
local-inc-dir := $(top-dir)/include

src := $(patsubst %,$(top-dir)/%,$(shell find | grep -E "\.c(|pp)"))
obj := $(src:.cpp=.o)
dep := $(src:.cpp=.d)

cflags := -I$(cmn-inc-dir) -I$(local-inc-dir) -nostdlib -fPIC -g 
cxxflags := -I$(cmn-inc-dir) -I$(local-inc-dir) -nostdlib -fPIC -fno-rtti -fno-exceptions -g -std=gnu++11 

all: $(arch-out)

clean:
	$(rm) -f $(arch-out)
	$(rm) -f $(obj)

$(arch-out): $(obj) $(cmn-out)
	@echo "  LD    $(notdir $@)"
	$(q)$(ld) -o $@ -Ttext-segment=0x100000000 -e start $(obj) $(cmn-out)

%.o: %.cpp
	@echo "  C++   $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(cxxflags) $<
	
%.d: %.cpp
	$(q)$(cxx) -M -MT $(@:.d=.o) -o $@ $(cxxflags) $<

.FORCE:
.PHONY: $(PHONY) .FORCE

-include $(dep)
