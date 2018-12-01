
target = libitm.so

CC = gcc
CXX = g++

ARCH = $(shell uname -m)

DEFINES = -D_GNU_SOURCE

CPPFLAGS = $(DEFINES) -I. -I../../NOrec/include -Iconfig -Iconfig/$(ARCH)
CXXFLAGS = -Wall -Wextra -Wno-ignored-attributes -g -O3 -fPIC
LDFLAGS = -Wl,-soname,libitm.so -lrt -L../../NOrec -lnorec

SRCS = $(wildcard *.cpp) config/common.cpp

OBJS = $(SRCS:.cpp=.cpp.o) beginTransaction.S.o

all: $(target)

%.S.o: config/$(ARCH)/%.S
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

%.cpp.o:	%.cpp
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

$(target): $(OBJS) 
	$(CXX) $(CPPFLAGS) -shared -fPIC $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) $(target) $(OBJS)
