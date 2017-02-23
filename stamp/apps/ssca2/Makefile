TMBUILD ?= seq

PROG := ssca2

SRCS += \
	alg_radix_smp.c \
	computeGraph.c \
	genScalData.c \
	cutClusters.c \
	findSubGraphs.c \
	getStartLists.c \
	createPartition.c \
	getUserParameters.c \
	globals.c \
	ssca2.c

LIBSRCS += \
	mt19937ar.c \
	random.c \
	thread.c

OBJS := ${SRCS:.c=.o} ${LIBSRCS:%.c=lib_%.o}

CFLAGS += -DSSCA2 -DNUMBER_OF_TRANSACTIONS=3

#CFLAGS += -DUSE_PARALLEL_DATA_GENERATION
#CFLAGS += -DWRITE_RESULT_FILES
CFLAGS += -DENABLE_KERNEL1
#CFLAGS += -DENABLE_KERNEL2 -DENABLE_KERNEL3
#CFLAGS += -DENABLE_KERNEL4

LDFLAGS += -lm

include ../common/$(TMBUILD)/Makefile.common

