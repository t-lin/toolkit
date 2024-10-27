LDFLAGS += -lgtest -lpthread
CFLAGS += -std=gnu17 -O0 -g -Wall

BINNAME = bounded-fifo

all:  bounded-fifo.c
	$(CC) $(CFLAGS) $< -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME)

