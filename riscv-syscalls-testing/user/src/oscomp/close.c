#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

/*
 * 测试成功则输出：
 * "  close success."
 * 测试失败则输出：
 * "  close error."
 */

void test_close(void) {
    TEST_START(__func__);
    int fd = open("test_close.txt", O_CREATE | O_RDWR);
    //assert(fd > 0);
    const char *str = "  close error.\n";
    int str_len = strlen(str);
    //assert(write(fd, str, str_len) == str_len);
    write(fd, str, str_len);
    int rt = close(fd);	
    assert(rt == 0);
    printf("  close %d success.\n", fd);
	
    TEST_END(__func__);
}

int main(void) {
    test_close();
    return 0;
}
