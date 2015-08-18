
target = test

src = test.c
obj = test.o

CFLAGS  = -g -O0 -Wall -fgnu-tm -pthread -I. -I../ -I../NOrec/include/
LDFLAGS = -L../ -litm -lrt

all: $(target)

%.o:	%.c
	$(CXX) $(CFLAGS) -fgnu-tm -c $^ -o $@

$(target):	$(obj)
	$(CXX) $(CFLAGS) -fgnu-tm $^ $(LDFLAGS) -o $@

run:
	../../../HTMBench/IntelPCM/pcm-tsx.x "LD_PRELOAD=../libitm.so ./test"

clean:
	$(RM) *.o *.so test
