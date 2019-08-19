target = libitm.a

CC = gcc
CXX = g++

ARCH = $(shell uname -m)

DEFINES = -D_GNU_SOURCE -DBACKEND_${BACKEND}

CPPFLAGS = $(DEFINES) -I. -Iconfig -I../../NOrec/include
CXXFLAGS = -Wall -Wextra -Wno-ignored-attributes -mavx -g -O3
LDFLAGS = -L../../NOrec -lnorec

ifeq (${BACKEND}, PHASEDTM)
  CPPFLAGS += -I../../htm -I../../phasedTM
  CXXFLAGS += -mrtm
  LDFLAGS += -L../../phasedTM -lphTM
endif

SRCS = $(wildcard *.cpp) config/common.cpp

OBJS = $(SRCS:.cpp=.cpp.o) sjlj.S.o

EXT_OBJS = $(wildcard ../../NOrec/*.o)
ifeq (${BACKEND}, PHASEDTM)
  EXT_OBJS += $(wildcard ../../phasedTM/*.o)
endif

all: $(target)

%.S.o: config/$(ARCH)/%.S
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

%.cpp.o:	%.cpp
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

$(target): $(OBJS) $(EXT_OBJS)
	$(AR) csr $@ $^
	ranlib $@

clean:
	$(RM) $(target) $(OBJS)
