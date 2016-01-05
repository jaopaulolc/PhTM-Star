#!/usr/bin/make

target = libphTM.a

DEFINES = -D_GNU_SOURCE -DNDEBUG -DPHASEDTM

ifdef MAX_CAPACITY_ABORTS
  DEFINES += -DMAX_CAPACITY_ABORTS=$(MAX_CAPACITY_ABORTS)
endif

ifdef PROFILING
  DEFINES += -D$(PROFILING)
endif

CPPFLAGS = $(DEFINES)
CFLAGS = -O3 -Wall -I. -I../htm

ARCH = $(shell uname -m)

ifeq ($(ARCH), x86_64)
  # Intel RTM
  CFLAGS += -mrtm
else
  ifeq ($(ARCH), ppc64le)
    # IBM PowerTM
    CFLAGS += -mhtm
  else
	  $(error unsupported architecture)
  endif
endif

all: $(target)

$(target): phTM.o
	$(AR) cr $@ $^


clean:
	$(RM) *.a *.o
