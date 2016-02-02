
target = libibmmalloc.so

CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -Wall -O3 -shared -fPIC -pthread
LDFLAGS = -ldl

all: $(target)

$(target): ibmmalloc.c
	$(CC) $(CPPFLAGS) -shared -fPIC $(CFLAGS) $^ -o $@ $(LDFLAGS)
	

clean:
	$(RM) $(target) *.o
