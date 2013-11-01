
targets = rtm-test hle-test

RTM_PATH = ../rtm
HLE_PATH = ../hle

CFLAGS = -O3 -Wall -I $(HLE_PATH) -I $(RTM_PATH)
LDFLAGS = -pthread -L $(HLE_PATH) -L $(RTM_PATH)

all: $(targets)

rtm-test: rtm-test.c librtm
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lrtm

librtm:
	$(MAKE) CFLAGS=-DRTM_ABORT_DEBUG -C $(RTM_PATH)

hle-test: hle-test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -mhle

clean:
	$(MAKE) -C $(RTM_PATH) clean
	$(RM) $(targets)
