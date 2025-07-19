#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

ssize_t splice(int fd_in, off_t *off_in,
               int fd_out, off_t *off_out,
               size_t len, unsigned int flags);

int main(int argc, char **argv) {
    int fd_file, pipefd[2];
    char buf[128];
    ssize_t ret;
    off_t offset;

    int num = atoi(argv[1]);
    // 创建临时文件
    fd_file = open("testfile.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    assert(fd_file >= 0);
    write(fd_file, "hello world", 11);  // 文件内容11字节

    // 创建管道
    assert(pipe(pipefd) == 0);
    if (num == 1) {
      // ============= 边界测试1: off_in 为负值，返回 -1 =============
      off_t neg_offset = -1;
      ret = splice(fd_file, &neg_offset, pipefd[1], NULL, 5, 0);
      printf("Test 1: Expected -1, got %zd (errno=%d)\n", ret, errno);
      assert(ret == -1);
      return 0;
    }

    if (num == 2) {
      // ============= 边界测试2: off_in 超过文件大小，返回 0 =============
      offset = 20;  // 超过11字节
      ret = splice(fd_file, &offset, pipefd[1], NULL, 5, 0);
      printf("Test 2: Expected 0, got %zd\n", ret);
      assert(ret == 0);
      return 0;
    }

    // ============= 边界测试3: 文件剩余小于len，只能拷贝剩余部分 =============
    if (num == 3) {
      offset = 6; // 剩余5字节 ("world")
      ret = splice(fd_file, &offset, pipefd[1], NULL, 10, 0);
      printf("Test 3: Expected 5, got %zd\n", ret);
      assert(ret == 5);

      // 读取管道确认内容

      memset(buf, 0, sizeof(buf));
      read(pipefd[0], buf, 5);
      printf("Test 3 read: %s\n", buf);
      assert(strcmp(buf, "world") == 0);
      return 0;
    }

    // ============= 边界测试4: 管道->文件 数据不足len，但不可返回0 =============
    // 往管道写入3字节
    if (num == 4) {
      write(pipefd[1], "abc", 3);

      offset = 0;
      ret = splice(pipefd[0], NULL, fd_file, &offset, 10, 0);
      printf("Test 4: Expected 3, got %zd\n", ret);
      assert(ret == 3);
      return 0;
    }
    // ============= 边界测试5: off_out为负值 返回 -1 =============

    if (num == 5) {
      off_t out_neg = -1;
      write(pipefd[1], "xyz", 3);
      ret = splice(pipefd[0], NULL, fd_file, &out_neg, 3, 0);
      printf("Test 5: Expected -1, got %zd (errno=%d)\n", ret, errno);
      assert(ret == -1);

      close(fd_file);
      close(pipefd[0]);
      close(pipefd[1]);
      unlink("testfile.txt");
    }
      //printf("All edge cases passed.\n");
    return 0;
}
