q := @
top-dir := $(CURDIR)

cc := gcc
cxx := g++
rm := rm
engines := test
test-obj := start.o

common-dir := $(top-dir)/common
common-obj := printf.o
real-common-obj := $(patsubst %,$(top-dir)/common/%,$(common-obj))
engine-outputs := $(patsubst %,$(top-dir)/%-engine,$(engines))

cflags := -nostdlib -O3 -fPIC -I$(common-dir)
ldflags := -nostdlib -Wl,-Ttext-segment=0x100000000 -Wl,-eengine_start

all: $(engine-outputs)

clean:
	$(rm) -f $(engine-outputs)
	$(rm) -f $(real-common-obj)


engine-only = $(patsubst %-engine,%,$(basename $(notdir $@)))
engine-dir = $(top-dir)/$(engine-only)
engine-objs = $(patsubst %,$(engine-dir)/%,$($(engine-only)-obj))

%-engine: $(engine-objs) $(real-common-obj)
	@make $(engine-objs)
	$(q)$(cxx) -o $@ $(ldflags) $(engine-objs) $(real-common-obj)

%.o: %.cpp
	@echo "  C++   $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(cflags) $<

%.o: %.c
	@echo "  CC    $(notdir $@)"
	$(q)$(cxx) -c -o $@ $(cflags) $<
