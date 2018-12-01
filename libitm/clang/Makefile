target = libitm.so

CC = gcc
CXX = g++

ARCH = $(shell uname -m)

DEFINES = -D_GNU_SOURCE

CPPFLAGS = $(DEFINES) -I. -I../../NOrec/include -Iconfig 
CXXFLAGS = -Wall -Wextra -Wno-ignored-attributes -mavx -g -O3 -fPIC
LDFLAGS = -Wl,-soname,libitm.so -lrt -L../../NOrec -lnorec

SRCS = $(wildcard *.cpp) config/common.cpp

OBJS = $(SRCS:.cpp=.cpp.o)

all: $(target)

%.cpp.o:	%.cpp
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

$(target): $(OBJS) 
	$(CXX) $(CPPFLAGS) -shared -fPIC $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) $(target) $(OBJS)
