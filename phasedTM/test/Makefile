#!/usr/bin/make

include Makefile.common

TARGET = main

CPPFLAGS += -DNUMBER_OF_TRANSACTIONS=1

.PHONY:	all clean

all: $(TARGET)

$(TARGET):	%:	%.o $(LIBDEPS)
	$(LD) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) *.o
