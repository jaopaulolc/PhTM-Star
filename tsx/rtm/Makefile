#!/usr/bin/make

target = librtm.a

override CFLAGS  += -O3 -Wall -I. -I../ -mrtm

all: $(target)

librtm.a: rtm.o
	$(AR) cr $@ $<

rtm.o: rtm.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) *.a *.o
