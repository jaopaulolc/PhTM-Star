TMBUILD ?= seq

PROG := genome

SRCS += \
	gene.c \
	genome.c \
	segments.c \
	sequencer.c \
	table.c

LIBSRCS += \
	bitmap.c \
	hash.c \
	hashtable.c \
	pair.c \
	random.c \
	list.c \
	mt19937ar.c \
	thread.c \
	vector.c

OBJS := ${SRCS:.c=.o} ${LIBSRCS:%.c=lib_%.o}

CFLAGS += -DNUMBER_OF_TRANSACTIONS=5

CFLAGS += -DLIST_NO_DUPLICATES

ifeq ($(TMBUILD), powerTM)
  CFLAGS += -DCHUNK_STEP1=2
else
  ifeq ($(TMBUILD), rtm)
    CFLAGS += -DCHUNK_STEP1=2
  else
    CFLAGS += -DCHUNK_STEP1=12
  endif
endif

include ../common/$(TMBUILD)/Makefile.common


