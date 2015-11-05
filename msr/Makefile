#!/usr/bin/make

CFLAGS = -O3 -Wall -I.

libmsr.a:	msr.o pmu.o
	$(AR) cr $@ $^

clean:
	$(RM) *.o *.a
