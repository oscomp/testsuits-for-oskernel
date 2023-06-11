# Testsuits for OS Kernel 2023

### 测试用例

- busybox 相关：[scripts/busybox](scripts/busybox)
- busybox+lua相关：[scripts/lua](scripts/lua)
- lmbench相关: [scripts/lua](scripts/lmbench)
- iozone相关：[scripts/iozone](scripts/iozone)

### 注意

- `lua`脚本和其他测试脚本要依赖`busybox`的`sh`功能。所以OS kernel首先需要支持`busybox`的`sh`功能。
- 部分脚本会需要特定的OS功能（syscall, device file等），OS kernel需要一步一步地添加功能，以支持不同程序的不同执行方式。

###  程序描述

[busybox cmd](scripts/busybox/busybox_cmd.txt)里给出了常用或容易支持的命令操作的列表，可以先试着支持这部分命令。这些命令所使用的系统调用请参考[这个目录](docs/busybox_cmd_syscalls)所对应的文件，这些命令所依赖的系统调用都可用`strace -f -c <CMD>`  得到。

获取lua所使用的系统调用的两个例子：

```
# 例子一，让lua执行一个字符串：
strace -f -c -o lua_print_syscall.txt ./lua -e 'print("Hello World!")'

# 例子二，让lua执行一个脚本：
strace -f -c -o lua_print_syscall.txt ./lua print.lua
```

print.lua的内容如下：

> print("Hello World!")

lmbench依次执行了一些小程序来测试系统的性能，可分析`bin/lmbench`来依次执行这些小程序，以达到对lmbench逐步的支持。

iozone测试了文件系统的读写性能，以及多线程情况下的吞吐量。

**！注：不要求支持网络，所以和socket相关的系统调用不必支持。**

## 程序所包含的系统调用
文档每行中前半部分`li a7,[NUM]`是二进制文件里使用系统调用时，系统调用号所对应的那条指令。后半部分`__NR_xxx [NUM]` 是头文件`unistd.h`里系统调用的名称及编号。

[busybox使用的系统调用](docs/busybox_musl_static_syscall.txt)

[lua使用的系统调用](docs/lua_musl_static_syscalls.txt)

[lmbench使用的系统调用](docs/lmbench_libc_syscall.txt)

[大赛使用的lmbench各子命令所使用的系统调用](docs/lmbench_cmd_syscalls)

```
# 从二进制文件中获取系统调用号
objdump -d objfile | grep -B 9 ecall | grep "li.a7" | tee syscall.txt
```

> 关于系统调用的详细信息，请参考[Linux手册](https://man7.org/linux/man-pages/man2/syscalls.2.html)。

## 程序的运行环境
示例程序的运行环境是Debian on Qemu RV64，搭建过程如下：

1. 在[https://people.debian.org/~gio/dqib/](https://people.debian.org/~gio/dqib/)点击[Images for riscv64-virt](https://gitlab.com/api/v4/projects/giomasce%2Fdqib/jobs/artifacts/master/download?job=convert_riscv64-virt)下载artifacts.zip。
2. 解压。`unzip artifacts.zip`
3. 安装`qemu-sysstem-riscv64`，`opensib`和`u-boot-qemu`。
4. 参考`artifacts/readme.txt`里的指令启动debian。

> 也可选择使用搭建好的镜像，下载地址：[debian-oscomp](https://cloud.tsinghua.edu.cn/f/1ffc4bc9149645a896ea/?dl=1)
> 执行`./run.sh`进入系统，登陆用户名：root，密码：root

下载好镜像之后，将oscomp-debian放在本目录下，修改oscomp-debina/run.sh，在其中加入`-drive file=../sdcard.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0`参数。执行make qemu进入系统。

进入系统后将/dev/sdb挂载至/mnt，随后可以进入/mnt目录运行测试程序。

## 构建测试用例

docker run --rm -it -v $(pwd):/code --privileged --entrypoint make alphamj/os-contest:v7.5 -C /code sdcard
