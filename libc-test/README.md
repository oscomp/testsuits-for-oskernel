# 2022操作系统大赛决赛第一阶段

决赛使用[musl](https://musl.libc.org/)和[libc-test](http://nsz.repo.hu/git/?p=libc-test)作为赛题。与初赛相同，编译好的elf文件存放在SD卡根目录中，需要内核初始化完成后主动依次运行。测试样例程序名和参数请参考release中run-static.sh和run-dynamic.sh两个文件内容。

本次大赛新增对动态链接功能的考察，程序所依赖的动态链接库也一并放在了SD卡根目录中。entry-dynamic.exe文件依赖程序解释器/lib/ld-musl-riscv64-sf.so.1，但它仅是对libc.so的符号链接，由于fat32文件系统不支持符号链接功能，因此尚未提供该文件，请各参赛队伍自行将对该文件的访问转发至libc.so。

Riscv交叉编译：
1. 修改Makefile中的MUSL_LIB和PREFIX为恰当的值
2. make disk
3. 在disk目录中生成riscv可执行文件和动态链接库

X86本机测试：
1. export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:src/functional:src/regression
2. 修改Makefile中CC=musl-gcc，OBJCOPY=objcopy
3. make run
