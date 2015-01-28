top-dir := $(CURDIR)
src-dir := $(top-dir)/src
inc-dir := $(top-dir)/inc
bin-dir := $(top-dir)/bin

out := $(bin-dir)/captive
src := $(patsubst src/%,$(src-dir)/%,$(shell find src/ | grep -e "\.cpp"))
obj := $(src:.cpp=.o)
dep := $(src:.cpp=.d)

common-cflags := -I$(inc-dir) -g -Wall -O0
cflags   := $(common-cflags)
cxxflags := $(common-cflags) -std=gnu++11
ldflags  :=

cc  := gcc
cxx := g++
ld  := ld
rm  := rm

all: $(out)
	
clean:
	$(rm) -f $(obj)
	$(rm) -f $(dep)
	$(rm) -f $(out)

$(out): $(dep) $(obj)
	$(q)$(cxx) -o $@ $(ldflags) $(obj)

%.o: %.cpp
	$(q)$(cxx) -c -o $@ $(cxxflags) $<

%.d: %.cpp
	$(q)$(cxx) -M -MT $(@:.d=.o) -o $@ $(cxxflags) $<

.FORCE:
.PHONY: $(PHONY) .FORCE

-include $(dep)
