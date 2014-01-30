#!/usr/bin/make

libmsr.a:	msr.c
	gcc -c -o msr.o msr.c
	$(AR) cr libmsr.a msr.o

clean:
	$(RM) *.o *.a
