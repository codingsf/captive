top-dir := $(CURDIR)
export inc-dir := $(top-dir)/include
export cmn-dir := $(top-dir)/common

export make := make
export rm := rm
export cc := gcc
export cxx := g++

archs := arm

arch-outs := $(patsubst %,$(top-dir)/%.arch,$(archs))
arch-cleans := $(patsubst %,$(top-dir)/%.arch.clean,$(archs))

all: $(arch-outs)

clean: $(arch-cleans)

%.arch: .FORCE
	$(q)$(make) arch-dir=$(basename $@) arch-out=$@ -C $(basename $@)

%.arch.clean: .FORCE
	$(q)$(make) arch-dir=$(basename $@) arch-out=$@ -C $(basename $@) clean

.FORCE:
.PHONY: .FORCE
