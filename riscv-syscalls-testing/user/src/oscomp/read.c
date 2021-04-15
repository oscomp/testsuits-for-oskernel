#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"

void test_read() {
	TEST_START(__func__);
	int fd = open("./text.txt", 0);
	char buf[256];
	int size = read(fd, buf, 256);
	assert(size >= 0);

	write(STDOUT, buf, size);
	close(fd);
	TEST_END(__func__);
}

int main(void) {
	test_read();
	return 0;
}
