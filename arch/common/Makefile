out := common.a

top-dir := $(CURDIR)
inc-dir := $(top-dir)/include

src := $(patsubst %,$(top-dir)/%,$(shell find | grep -E "\.c(|pp)"))
obj := $(src:.cpp=.o)
dep := $(src:.cpp=.d)

cflags := -I$(inc-dir) -nostdlib -fPIC -g -O3 -fno-builtin
cxxflags := -I$(inc-dir) -nostdlib -fPIC -fno-rtti -fno-exceptions -g -std=gnu++11 -O3 -fno-builtin -fno-delete-null-pointer-checks

all: $(out)

clean: .FORCE
	$(rm) -f $(out)
	$(rm) -f $(obj)

$(out): $(obj)
	@echo "  AR    $(notdir $@)"
	$(q)$(ar) r $@ $(obj)

%.o: %.c
	@echo "  CC    $(notdir $@)"
	$(q)$(cc) -c -o $@ $(cflags) $<

%.o: %.cpp
	@echo "  C++   $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(cxxflags) $<

%.d: %.cpp
	$(q)$(cxx) -M -MT $(@:.d=.o) -o $@ $(cxxflags) $<

.FORCE:
.PHONY: $(PHONY) .FORCE

-include $(dep)
