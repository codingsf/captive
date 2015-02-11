MAKEFLAGS += -rR --no-print-directory
export q := @

top-dir := $(CURDIR)
export cmn-dir := $(top-dir)/common
export cmn-inc-dir := $(cmn-dir)/include
export cmn-out := $(cmn-dir)/common.a

export make := make
export rm := rm
export cc := gcc
export cxx := g++
export ar := ar
export ld := ld

archs := arm

arch-outs := $(patsubst %,$(top-dir)/%.arch,$(archs))
arch-cleans := $(patsubst %,$(top-dir)/%.arch.clean,$(archs))

all: $(arch-outs)

clean: $(arch-cleans)
	$(q)$(make) -C $(cmn-dir) clean

$(cmn-out): .FORCE
	$(q)$(make) -C $(cmn-dir)

%.arch: $(cmn-out) .FORCE
	$(q)$(make) arch-dir=$(basename $@) arch-out=$@ -C $(basename $@)

%.arch.clean: .FORCE
	$(q)$(make) arch-out=$(basename $@) -C $(basename $(basename $@)) clean

.FORCE:
.PHONY: .FORCE
