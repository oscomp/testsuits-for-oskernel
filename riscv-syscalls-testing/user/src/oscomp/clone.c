#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

size_t stack[1024] = {0};
static int child_pid;

static int child_func(void){
    printf("  Child says successfully!\n");
    return 0;
}

void test_clone(void){
    TEST_START(__func__);
    int wstatus;
    child_pid = clone(child_func, NULL, stack, 1024, SIGCHLD);
    assert(child_pid != -1);
    if (child_pid == 0){
	    printf("child pid = %d", child_pid);
    }else{
	    wait(&wstatus);
	    printf("clone process successfully.\npid:%d\n", child_pid);
    }

    TEST_END(__func__);
}

int main(void){
    test_clone();
    return 0;
}
