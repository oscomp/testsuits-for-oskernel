#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"

/*
 * 测试通过时的输出：
 * "gettimeofday: [num]"
 */
void test_gettimeofday() {
	TEST_START(__func__);

	int test_ret = get_time();
	assert(test_ret >= 0);

	printf("gettimeofday successfully\ntime: %d\n", test_ret);
	TEST_END(__func__);
}

int main(void) {
	test_gettimeofday();
	return 0;
}
