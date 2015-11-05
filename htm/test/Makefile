
ARCH = $(shell uname -m)

targets = htm-test
ifeq ($(ARCH), x86_64)
  targets += hle-test
endif

HTM_PATH = ..
HLE_PATH = ../haswell


CFLAGS  = -O3 -Wall -pthread -I$(HLE_PATH) -I$(HTM_PATH) -D_GNU_SOURCE
LDFLAGS = -lpthread -L$(HTM_PATH) -lhtm

ifeq ($(ARCH), x86_64)
  CFLAGS  += -mrtm -DSIMPLE_LOCK
  LDFLAGS += -mrtm
else
  ifeq ($(ARCH), ppc64le)
    CFLAGS += -mhtm
  else
	  $(error unsupported architecture)
  endif
endif

all: $(targets)

htm-test: %:	%.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

hle-test: %:	%.o $(HLE_PATH)/hle.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -mhle

clean:
	$(RM) $(targets) *.o
