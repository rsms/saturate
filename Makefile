sources  := src/sat.cc src/str.cc src/expr.cc

CXX = clang
CC  = clang
LD  = clang

XFLAGS   := -MMD -Wall
SUFFIX   :=
ifneq ($(DEBUG),)
	SFLAGS := -O0 -g
	SUFFIX := -g
else
	SFLAGS := -O3 -DNDEBUG
endif
CFLAGS   += $(XFLAGS) -std=c11
CXXFLAGS += $(XFLAGS) -std=c++11 -stdlib=libc++
LDFLAGS  += -stdlib=libc++ -lc++ -dead_strip

objdir   := .obj$(SUFFIX)
objects  := $(patsubst %.cc,$(objdir)/%.o,$(sources:.c=$(objdir)/.o))
uniqdirs := $(sort $(foreach fn,$(objects),$(dir $(fn))))

all: sat$(SUFFIX)
pre:
	mkdir -p $(uniqdirs)
sat$(SUFFIX): pre $(objects)
	$(LD) $(LDFLAGS) -o $@ $(objects)

test:
	@echo test/run.sh test_\*.cc
	@cd test && CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" SFLAGS="$(SFLAGS)" \
	  LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC)" CXX="$(CXX)" \
	  bash run.sh test_*.cc

$(objdir)/%.o: %.cc
	$(CXX) $(CXXFLAGS) $(SFLAGS) -c $< -o $@
$(objdir)/%.o: %.c
	$(CC) $(CFLAGS) $(SFLAGS) -c $< -o $@

clean:
	rm -rf sat sat-g .obj .obj-g test/*.bin test/*.dSYM

-include ${objects:.o=.d}
.PHONY: clean pre test
