# Testsuits for OS Kernel for On-Site Final Competition 2023

**3 道题总分 200 分，task 1 和 task 2 各占 50 分，task 3 占 100 分。**

## task 1: /proc/interrupts （[interrupts-test](https://github.com/oscomp/testsuits-for-oskernel/)）

本题中，我们需要在内核中记录自系统启动以来所有外部（PLIC）中断和时钟中断的处理次数，并创建一个路径为 `/proc/interrupts` 的虚拟文件。

当用户读取该文件时，应当得到一个包含中断号和对应的中断处理次数的列表，其中每行包含一个中断号和对应的处理次数，均以十进制表示，之间用一个冒号和任意个空格分隔；每行应以一个换行符结尾，各行包含的中断号递增且不重复，例如

```plaintext
5:        8188
8:        1162
10:        397
```

当用户尝试写入、删除或移动该文件时，应当返回错误。

若存在处理的外部中断的中断号与时钟中断重复，则应当只保留时钟中断的处理次数。

### 测试点（50 分）

1. 虚拟文件 `/proc/interrupts` 能够被读取，且不能被写入、删除和移动（25 分）
2. 虚拟文件 `/proc/interrupts` 能够正确统计中断处理次数（25 分）

## task 2: copy_file_range （[copy-file-range-test](https://github.com/oscomp/testsuits-for-oskernel/)）

本题目需要我们实现一个系统调用 `copy_file_range`，用于将打开的文件中指定范围的数据复制到另一个文件中，其对应用户库函数的声明为：

```c
#include <unistd.h>

ssize_t copy_file_range(int fd_in, off_t *off_in,
                        int fd_out, off_t *off_out,
                        size_t len, unsigned int flags);
```

该系统调用应复制文件描述符 `fd_in` 中的至多 `len` 个字节到文件描述符 `fd_out` 中。

若 `off_in` 为 `NULL`，则复制时应从文件描述符 `fd_in` 本身的文件偏移处开始读取，并将其文件偏移增加成功复制的字节数；否则，从 `*off_in` 指定的文件偏移处开始读取，不改变 `fd_in` 的文件偏移，而是将 `*off_in` 增加成功复制的字节数。

参数 `off_out` 的行为类似：若 `off_out` 为 `NULL`，则复制时从文件描述符 `fd_out` 本身的文件偏移处开始写入，并将其文件偏移增加成功复制的字节数；否则，从 `*off_out` 指定的文件偏移处开始写入，不改变 `fd_out` 的文件偏移，而是将 `*off_out` 增加成功复制的字节数。

该系统调用的返回值为成功复制的字节数，出现错误时返回负值。若读取 `fd_in` 时的文件偏移超过其大小，则直接返回 0，不进行复制。

本题中，`fd_in` 和 `fd_out` 总指向文件系统中两个不同的普通文件；`flags` 总为 0，没有实际作用。

### 测试点（50 分）

1. 测试传入的 `off_in` 和 `off_out` 总为 `NULL`，不包含部分边界情况（12.5 分）
2. 测试传入的 `off_in` 和 `off_out` 总为 `NULL`（12.5 分）
3. 测试不包含部分边界情况（12.5 分）
4. 无特殊约束（12.5 分）

### 参考

- [copy_file_range(2)](https://man7.org/linux/man-pages/man2/copy_file_range.2.html)

## task 3

### 题目

支持syzkaller for linux内核fuzzing测试工具

### 描述

目前内核赛道参赛队实现的OS能够支持Linux应用，所以需要在参赛队自己写的内核上支持运行[syzkaller for linux](https://github.com/google/syzkaller)这个内核fuzzing测试工具。

### 对内核的具体要求

参考[syzkaller内核fuzzing测试工具的工作流程](https://github.com/google/syzkaller/blob/master/docs/internals.md)列出如下内核功能需支持的要求。硬件环境是qemu for riscv64。总分100分。

1. 【10分】支持sshd for linux运行
2. 【10分】支持syz-fuzzer for linux运行
3. 【10分】支持syz-executor for linux运行
4. 【10分】参考/sys/kernel/debug/kcov的输出，直接给出类似格式的输出(不需要实现kcov功能)
5. 【10分】实现linux的kcov功能支持，能够产生类似/sys/kernel/debug/kcov的输出
6. 【40分】支持完整运行syzaller(注意，为此可能还需要支持上面没有列出的Linux应用或Linux系统调用)
7. 【10分】完成设计实现与执行过程分析技术报告

### 参考

- [kcov](https://www.kernel.org/doc/html/latest/dev-tools/kcov.html)
- [How syzkaller works](https://github.com/google/syzkaller/blob/master/docs/internals.md)
- [Adding new OS support for syzkaller](https://github.com/google/syzkaller/blob/master/docs/adding_new_os_support.md)
- [Setup: Debian/Ubuntu host, QEMU vm, riscv64 kernel](https://github.com/google/syzkaller/blob/master/docs/linux/setup_linux-host_qemu-vm_riscv64-kernel.md)
- Syzkaller 源码分析(1)-(5)：[1](https://xz.aliyun.com/t/5079)、[2](https://xz.aliyun.com/t/5098)、[3](https://xz.aliyun.com/t/5154)、[4](https://xz.aliyun.com/t/5223)、[5](https://xz.aliyun.com/t/5401)
- [syzkaller on freebsd](https://freebsdfoundation.org/wp-content/uploads/2021/01/Kernel-Fuzzing.pdf)
- [从0到1开始使用syzkaller进行Linux内核漏洞挖掘](https://bbs.kanxue.com/thread-265405.htm)
- [fuzzing-tutorial经典论文/书籍/博客等](https://github.com/liyansong2018/fuzzing-tutorial)


