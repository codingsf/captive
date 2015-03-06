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

export common-cflags := -I$(shared-dir) -nostdlib -g -fno-builtin -fno-omit-frame-pointer -mno-sse
export common-cflags += -mno-mmx -mno-sse2 -O3 -mcmodel=kernel -fno-rtti -fno-exceptions -std=gnu++11
export common-cflags += -fno-delete-null-pointer-checks -mno-red-zone

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
