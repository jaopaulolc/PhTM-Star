
target = test

CPPFLAGS = -D_GNU_SOURCE -I.
CFLAGS = -Wall -O3 -pthread
LDFLAGS = -ldl -lpthread

.PHONY: all clean run

all: ../libibmmalloc.so $(target)

$(target):

../libibmmalloc.so:
	make -C ../ clean && make -C ../

run: ../libibmmalloc.so
	LD_PRELOAD=$^ ./test

clean:
	$(RM) $(target) *.o
