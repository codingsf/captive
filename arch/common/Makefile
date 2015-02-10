out := common.a

top-dir := $(CURDIR)
inc-dir := $(top-dir)/include

src := $(patsubst %,$(top-dir)/%,$(shell find | grep -E "\.c(|pp)"))
obj := $(src:.cpp=.o)
obj := $(obj:.c=.o)
dep := $(src:.cpp=.d)

cflags := -I$(inc-dir) -nostdlib -fPIC
cxxflags := -I$(inc-dir) -nostdlib -fPIC

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

.FORCE:
.PHONY: .FORCE

# -include $(dep)
