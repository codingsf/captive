local-inc-dir := $(arch-dir)/include

obj := env.o

all: $(arch-out)

clean:
	$(rm) -f $(arch-out)

$(arch-out): $(obj) $(cmn-out)
	@echo "  LD    $(notdir $@)"
	$(q)$(ld) -o $@ -Ttext-segment=0x100000000 -e start $(obj) $(cmn-out)

%.o: %.cpp
	@echo "  C++   $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(cxxflags) $<