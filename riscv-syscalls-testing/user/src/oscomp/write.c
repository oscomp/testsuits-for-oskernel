#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

void test_write(){
	TEST_START(__func__);
	const char *str = "Hello operating system contest.\n";
	int str_len = strlen(str);
	assert(write(STDOUT, str, str_len) == str_len);
	TEST_END(__func__);
}

int main(void) {
	test_write();
	return 0;
}
