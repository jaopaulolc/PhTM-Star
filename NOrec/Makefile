#!/usr/bin/make

TARGET = libnorec.a

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

CPPFLAGS = -I./include
CXXFLAGS = -O3 -mrtm -Wall


$(TARGET):	$(OBJS)
	$(AR) cr $@ $^
	ranlib $@


clean:
	$(RM) *.o *.a
