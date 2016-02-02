#!/usr/bin/make

TMBUILD ?= tinystm

include ../common/$(TMBUILD)/Makefile.common

BUILDDIR = ../../$(TMBUILD)

TARGET = $(BUILDDIR)/conflict

CFLAGS += -DNUMBER_OF_TRANSACTIONS=2

.PHONY:	all clean

all: TM_BUILD_DIR $(TARGET)

TM_BUILD_DIR:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/conflict.o:	conflict.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -c -o $@ $<

$(TARGET):	%:	%.o $(TMLIB)
	$(LD) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(BUILDDIR)/*.o
