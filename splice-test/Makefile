# Makefile for test_splice

CFLAGS := -Wall -Wextra -g
TARGET := test_splice
SRC := test_splice.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -static -O3 -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
