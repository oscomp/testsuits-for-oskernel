#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
/*
 * 成功测试时父进程的输出：
 * "  parent process."
 * 成功测试时子进程的输出：
 * "  child process."
 */
static int fd[2];

void test_fork(void){
    TEST_START(__func__);
    int cpid, wstatus;
    cpid = fork();
    assert(cpid != -1);

    if(cpid > 0){
	wait(&wstatus);
	printf("  parent process. wstatus:%d\n", wstatus);
    }else{
	printf("  child process.\n");
	exit(0);
    }
    TEST_END(__func__);
}

int main(void){
    test_fork();
    return 0;
}
