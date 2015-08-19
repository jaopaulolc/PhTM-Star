
target = libitm.so

CFLAGS  = -O3 -fPIC -pthread

HTM ?= haswell
HTM_PATH = haswell

ifeq ($(HTM),power8)
  HTM_PATH  = ?????
  CFLAGS    = ?????
else
  CFLAGS   += -mrtm -DSIMPLE_LOCK
endif

CFLAGS += -I. -I$(HTM_PATH) -I../NOrec/include -Istm_specific/NOrec

#srcs = $(wildcard *.c) $(wildcard $(HTM_PATH)/*.c)
c_srcs   = $(wildcard *.c) 
cpp_srcs = $(wildcard stm_specific/NOrec/*.cpp)
s_srcs   = $(wildcard *.S)
objs     = $(c_srcs:.c=.o) $(s_srcs:.S=.o) $(cpp_srcs:.cpp=.o) ../NOrec/libnorec.a

all:	$(target)

%.o:	%.S
	$(CXX) $(CFLAGS) -c $^ -o $@

%.o:	%.cpp
	$(CXX) $(CFLAGS) -c $^ -o $@

%.o:	%.c
	$(CXX) $(CFLAGS) -c $^ -o $@

$(target):	$(objs)
	$(CXX) $(CFLAGS) $^ -shared -Wl,-soname,libitm.so -lrt -o $@

clean:
	$(RM) *.so *.o stm_specific/NOrec/*.o
