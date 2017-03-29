src  := $(shell ls *.c)
objs := $(patsubst %.c, %.o, $(src))
CC = gcc
CPPFLAGS = -I.
CPPFLAGS += -Wall
LDFLAGS = -lssl -lz -lcrypto -lhttp_parser -lgumbo -lhiredis -llua

crawler: $(objs)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o crawler a.out
