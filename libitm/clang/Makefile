target = libitm.so

CC = gcc
CXX = g++

ARCH = $(shell uname -m)

DEFINES = -D_GNU_SOURCE -D${BACKEND}

CPPFLAGS = $(DEFINES) -I. -Iconfig -I../../NOrec/include
CXXFLAGS = -Wall -Wextra -Wno-ignored-attributes -mavx -g -O3 -fPIC
LDFLAGS = -Wl,-soname,libitm.so -lrt -L../../NOrec -lnorec

ifeq (${BACKEND}, PHASEDTM)
  CPPFLAGS += -I../../htm -I../../phasedTM
  CXXFLAGS += -mrtm
  LDFLAGS += -L../../phasedTM -lphTM
endif

SRCS = $(wildcard *.cpp) config/common.cpp

OBJS = $(SRCS:.cpp=.cpp.o)

all: $(target)

%.cpp.o:	%.cpp
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

$(target): $(OBJS) 
	$(CXX) $(CPPFLAGS) -shared -fPIC $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) $(target) $(OBJS)
