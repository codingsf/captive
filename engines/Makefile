q := @
top-dir := $(CURDIR)

cc := gcc
cxx := g++

cflags := -nostdlib -O3
ldflags := -nostdlib

engines := test
test-obj := start.o

common-obj := printf.o
real-common-obj := $(patsubst %,$(top-dir)/common/%,$(common-obj))
engine-outputs := $(patsubst %,$(top-dir)/%-engine.so,$(engines))

all: $(engine-outputs)

clean:
	@

engine-only = $(patsubst %-engine,%,$(basename $(notdir $@)))
engine-dir = $(top-dir)/$(engine-only)
engine-objs = $(patsubst %,$(engine-dir)/%,$($(engine-only)-obj))

%.so: $(engine-objs) $(real-common-obj)
	@make $(engine-objs)
	$(q)$(cxx) -o $@ -shared $(ldflags) $(engine-objs) $(real-common-obj)

%.o: %.cpp
	$(q)$(cxx) -c -o $@ -fPIC $(cflags) $<

%.o: %.c
	$(q)$(cxx) -c -o $@ -fPIC $(cflags) $<
