#!/usr/bin/make

CFLAGS = -O3 -Wall -I. -flto

libmsr.a:	msr.o pmu.o
	$(AR) cr $@ $^

clean:
	$(RM) *.o *.a
