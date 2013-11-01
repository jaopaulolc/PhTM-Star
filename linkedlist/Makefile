CFLAGS  := -O3 -Wall
LDFLAGS := -pthread

TARGET = intset-seq intset-tm intset-coarse intset-fine intset-hle intset-rtm

SOURCE_SEQ = intset.c linkedlist-seq.c
SOURCE_TM = intset.c linkedlist-tm.c
SOURCE_COARSE = intset.c linkedlist-coarse.c
SOURCE_FINE = intset.c linkedlist-fine.c
SOURCE_HLE = intset.c linkedlist-hle.c
SOURCE_RTM = intset.c linkedlist-rtm.c

ALL: $(TARGET)

intset-seq: $(SOURCE_SEQ)
	$(CC) $(CFLAGS) $(SOURCE_SEQ) -o $@ $(LDFLAGS)

intset-tm: $(SOURCE_TM)
	$(CC) $(CFLAGS) -fgnu-tm $(SOURCE_TM) -o $@ $(LDFLAGS)

intset-coarse: $(SOURCE_COARSE)
	$(CC) $(CFLAGS) $(SOURCE_COARSE) -o $@ $(LDFLAGS)

intset-fine: $(SOURCE_FINE)
	$(CC) $(CFLAGS) $(SOURCE_FINE) -o $@ $(LDFLAGS)

intset-hle: $(SOURCE_HLE)
	$(CC) $(CFLAGS) -I ../tsx/hle/ $(SOURCE_HLE) -o $@ $(LDFLAGS) -mhle

intset-rtm: $(SOURCE_RTM) librtm
	$(CC) $(CFLAGS) -DTSX_RTM -I ../tsx/rtm/ $(SOURCE_RTM) -o $@ $(LDFLAGS) -L ../tsx/rtm -lrtm -mrtm

librtm:
	$(MAKE) -C ../tsx/rtm clean && $(MAKE) CFLAGS=-DRTM_ABORT_DEBUG -C ../tsx/rtm

clean:
	$(RM) intset-seq intset-tm intset-coarse intset-fine intset-hle intset-lightlock intset-rtm *.o 
