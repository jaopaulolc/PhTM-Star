
target = libitm.a

ARCH = $(shell uname -m)

DEFINES = -D_GNU_SOURCE

CPPFLAGS = $(DEFINES) -I. -Iconfig -Iconfig/$(ARCH)
CXXFLAGS = -Wall -Wextra -g -O3

SRCS = $(wildcard *.cpp) config/common.cpp

OBJS = $(SRCS:.cpp=.cpp.o) beginTransaction.S.o

all: $(target)

%.S.o: config/$(ARCH)/%.S
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@

%.cpp.o:	%.cpp
	$(CXX) $(CPPFLAGS) -c $^ $(CXXFLAGS) -o $@
	
$(target): $(OBJS)
	$(AR) csr $@ $^
	ranlib $@

clean:
	$(RM) $(target) $(OBJS)
