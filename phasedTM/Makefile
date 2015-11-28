#!/usr/bin/make

target = libphTM.a

DEFINES = -D_GNU_SOURCE -DNDEBUG

CFLAGS = -O3 -Wall -I. -I../htm $(DEFINES)

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
