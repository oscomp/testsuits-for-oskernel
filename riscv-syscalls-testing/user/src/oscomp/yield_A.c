#include <unistd.h>
#include <stdio.h>

const int WIDTH = 10;
const int HEIGHT = 5;

/*
理想结果：三个程序交替输出 ABC

测试时，让A, B, C程序按顺序并同时运行
*/

int test_yield()
{
    //TEST_START(__func__);
    for (int i = 0; i < HEIGHT; ++i)
    {
        char buf[WIDTH + 1];
        for (int j = 0; j < WIDTH; ++j)
            buf[j] = 'A';
        buf[WIDTH] = 0;
        printf("%s [%d/%d]\n", buf, i + 1, HEIGHT);
        sched_yield();
    }
    //puts("Test write A OK!");
    //TEST_END(__func__);
}

int main(void) {
	test_yield();
	return 0;
}
