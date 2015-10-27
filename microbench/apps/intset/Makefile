#!/usr/bin/make

APP     ?= intset_hs
TMBUILD ?= tinystm

include ../common/$(TMBUILD)/Makefile.common

BUILDDIR = ../../$(TMBUILD)

TARGET = $(BUILDDIR)/$(APP)

CFLAGS  += -DNUMBER_OF_TRANSACTIONS=3

LDFLAGS += -lrt

.PHONY:	all clean

all: TM_BUILD_DIR $(TARGET)

TM_BUILD_DIR:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/intset_hs.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_HASHSET -c -o $@ $<

$(BUILDDIR)/intset_ll.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_LINKEDLIST -c -o $@ $<

$(BUILDDIR)/intset_rb.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_RBTREE -c -o $@ $<

$(BUILDDIR)/intset_sl.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_SKIPLIST -c -o $@ $<

$(TARGET):	%:	%.o $(TMLIB)
	$(LD) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(BUILDDIR)/*.o
