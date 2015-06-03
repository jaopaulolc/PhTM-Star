
target = libitm.so

CFLAGS  = -O3 -Wall -fPIC -pthread

HTM ?= haswell
HTM_PATH = haswell

ifeq ($(HTM),power8)
  HTM_PATH  = ?????
  CFLAGS    = ?????
else
  CFLAGS   += -mrtm -DSIMPLE_LOCK
endif

CFLAGS += -I. -I$(HTM_PATH) -INOrec/include

#srcs = $(wildcard *.c) $(wildcard $(HTM_PATH)/*.c)
srcs = $(wildcard *.c)
objs = $(srcs:.c=.o) NOrec/libnorec.a

all: $(target)

%.o:	%.c
	$(CXX) $(CFLAGS) -c $^ -o $@

$(target): $(objs)
	$(CXX) -shared -Wl,-soname,libitm.so $(objs) -o $@

clean:
	$(RM) *.so *.o
