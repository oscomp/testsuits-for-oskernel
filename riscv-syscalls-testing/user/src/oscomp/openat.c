#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"

void test_openat(void) {
    TEST_START(__func__);
    //int fd_dir = open(".", O_RDONLY | O_CREATE);
    int fd_dir = open("./mnt", O_DIRECTORY);
    printf("open dir fd: %d\n", fd_dir);
    int fd = openat(fd_dir, "test_openat.txt", O_CREATE | O_RDWR);
    printf("openat fd: %d\n", fd);
    assert(fd > 0);
    printf("openat success.\n");

    /*(
    char buf[256] = "openat text file";
    write(fd, buf, strlen(buf));
    int size = read(fd, buf, 256);
    if (size > 0) printf("  openat success.\n");
    else printf("  openat error.\n");
    */
    close(fd);	
	
    TEST_END(__func__);
}

int main(void) {
    test_openat();
    return 0;
}
