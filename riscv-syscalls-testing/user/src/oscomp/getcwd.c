#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"

/*
 * 测试通过时输出：
 * "getcwd OK."
 * 测试失败时输出：
 * "getcwd ERROR."
 */
void test_getcwd(void){
   TEST_START(__func__);
    char *cwd = NULL;
    char buf[128] = {0};
    cwd = getcwd(buf, 128);
    if(cwd != NULL) printf("getcwd: %s successfully!\n", buf);
    else printf("getcwd ERROR.\n");
   TEST_END(__func__);
}

int main(void){
    test_getcwd();
    return 0;
}
