
target = libibmmalloc.so

CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -Wall -O3 -pthread

all: $(target)

$(target): ibmmalloc.c
	$(CC) $(CPPFLAGS) -shared -fPIC $(CFLAGS) -o $@ $^
	

clean:
	$(RM) $(target) *.o
