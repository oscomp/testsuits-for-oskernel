# Testsuits for OS Kernel

## 决赛阶段（6月21日~8月18日）

### 基本要求

- - 参赛的OS kernel需要尽力支持busybox, lua, lmbench三个程序的多种执行方式形成的测试用例。
- 分阶段进行支持。
  - 第一阶段（6月21日~7月20日）：支持 busybox和lua （侧重功能实现）
  - 第二阶段（7月21日~8月18日）：支持 lmbench （侧重性能优化）
- 在这两个阶段中，会在此仓库添加基于这三个程序的测试用例。
- 要求参赛的OS kernel能够在QEMU for 双核RV64 模拟器和K210开发板上运行并通过测试用例。
- 要求参赛的OS kernel要考虑内核设计的合理性和安全可靠性，经得起工具的安全检查或压力测试。

### 测试用例

- busybox 相关：[scripts/busybox](scripts/busybox)
- busybox+lua相关：[scripts/lua](scripts/lua)

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

**！注：不要求支持网络，所以和socket相关的系统调用不必支持。**

## 程序所包含的系统调用
文档每行中前半部分`li a7,[NUM]`是二进制文件里使用系统调用时，系统调用号所对应的那条指令。后半部分`__NR_xxx [NUM]` 是头文件`unistd.h`里系统调用的名称及编号。

[busybox使用的系统调用](docs/busybox_musl_static_syscall.txt)

[lua使用的系统调用](docs/lua_musl_static_syscalls.txt)

[lmbench使用的系统调用](docs/lmbench_libc_syscall.txt)

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
>
> 执行`./run.sh`进入系统，登陆用户名：root，密码：root

## 程序编译过程

编译环境的准备

```bash
# 安装必要的软件
sudo apt install build-essential
sudo apt install musl-tools
apt-get install libncurses5-dev libncursesw5-dev
```

编译busybox

```bash
# 需要包含一些linux头文件
ln -s /usr/include/linux /usr/include/riscv64-linux-musl/linux
ln -s /usr/include/asm-generic /usr/include/riscv64-linux-musl/asm
ln -s /usr/include/mtd /usr/include/riscv64-linux-musl/mtd
cp /usr/include/riscv64-linux-gnu/asm/byteorder.h /usr/include/asm-generic

# 编译busybox
 cd busybox
make menuconfig	# 默认动态编译，如需静态编译则设置CONFIG_STATIC=y
make CC="musl-gcc"
```

编译lua

```bash
cd lua
make posix CC="musl-gcc"	# 动态编译。由于之前的准备，现在gcc就是musl-gcc
make posix CC="musl-gcc -static"	# 静态编译
```

编译lmbench

```bash
cd lmbench
make build	# 动态编译并执行
make build CC="gcc -static"	# 静态编译并执行
```

## 运行测试代码

```bash
# busybox
cd scripts/busybox
cp ../../busybox/busybox .
./busybox_testcode.sh	# 运行busybox的测试代码。如某条命令执行失败，结果会输出到文件result.txt

# lua
cd scripts/lua
cp ../../lua/src/lua .
./test.sh <file>	# 运行lua脚本。

# lmbench
cd lmbench
make oscomp
```

