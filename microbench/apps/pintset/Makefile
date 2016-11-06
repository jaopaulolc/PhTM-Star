#!/usr/bin/make

TMBUILD ?= tinystm

include ../common/$(TMBUILD)/Makefile.common

BUILDDIR = ../../$(TMBUILD)

TARGET = $(BUILDDIR)/pintset
SRCS = utils.c hashset.c linkedlist.c skiplist.c rbtree.c pintset.c 
OBJS = $(patsubst  %.c, $(BUILDDIR)/%.o, $(SRCS))

CPPFLAGS += -DNUMBER_OF_TRANSACTIONS=3 -I./includes

LDFLAGS += -lrt


.PHONY:	all clean

all: TM_BUILD_DIR $(TARGET)

TM_BUILD_DIR:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o:	%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -c -o $@ $<


$(TARGET): $(OBJS) $(TMLIB)
	$(LD) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(BUILDDIR)/*.o
