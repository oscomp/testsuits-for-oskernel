.PHONY: all clean

all:
	for i in 1 2 3; do \
		$(CC) test.c -static -O3 -Wall -DCASE=$$i -o copy-file-range-test-$$i; \
	done

clean:
	rm -f copy-file-range-test-*
