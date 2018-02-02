SRC_PATH := ./src

src  := $(shell ls $(SRC_PATH)/*.c)
objs := $(patsubst %.c, %.o, $(src))
CC = gcc
CPPFLAGS = -I$(SRC_PATH)
CPPFLAGS += -Wall
LDFLAGS = -lssl -lz -lcrypto -lhttp_parser -lgumbo -lhiredis -llua

crawler: $(objs)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_PATH)/*.o crawler 
