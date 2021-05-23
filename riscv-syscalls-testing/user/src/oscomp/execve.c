#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

/*
 * 测试成功则输出：
 * "  I am test_echo."
 * 测试失败则输出：
 * "  execve error."
 */
void test_execve(void){
    TEST_START(__func__);
    char *newargv[] = {"test_echo", NULL};
    char *newenviron[] = {NULL};
    execve("test_echo", newargv, newenviron);
    printf("  execve error.\n");
    //TEST_END(__func__);
}

int main(void){
    test_execve();
    return 0;
}
