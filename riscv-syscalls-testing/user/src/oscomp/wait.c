#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

void test_wait(void){
    TEST_START(__func__);
    int cpid, wstatus;
    cpid = fork();
    if(cpid == 0){
	printf("This is child process\n");
        exit(0);
    }else{
	//if(cpid == wait(NULL)) printf("wait success.\n");
	//else printf("wait error.\n");

	pid_t ret = wait(&wstatus);
	assert(ret != -1);

	printf("wait child success.\nwstatus: %d\n", wstatus);
    }
    TEST_END(__func__);
}

int main(void){
    test_wait();
    return 0;
}
