
SRCS = $(sort $(wildcard *.c))
OBJS = $(SRCS:.c=.o)

CFLAGS = -Os
LDFLAGS = -static
LIBS = -lpthread -lrt -lpthread


all: libc-bench

clean:
	rm -f $(OBJS) libc-bench

test: all
	./libc-bench

libc-bench: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
