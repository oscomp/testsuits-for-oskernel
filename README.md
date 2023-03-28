# Testsuits for OS Kernel

这里给出的[syscalls测试用例](riscv-syscalls-testing/)会通过系统调用访问内核实现组开发的OS，得到正确可靠的服务。
系统调用基于部分比较基础的Linux syscalls。从测试用例可以看出，自己开发的OS只需实现Linux syscalls的功能子集即可。

内核实现组可通过[syscalls测试用例 for Linux on Qemu RV64运行环境](riscv-linux-rootfs)了解测试用例在Linux on Qemu for RV64上的执行效果；
然后可以尝试基于Qemu for RV64来开发，并用测试用例来测试自己实现的OS；在SD card上建立加载了测试用例的
[fat32文件系统](./fat32-info.md)，让自己开发的OS能够正确访问并执行sd card上的测试用例。

在本地通过测试后，可提交[OS评测系统](https://os.educg.net/)进行测试。

## syscalls说明
[syscalls说明](oscomp_syscalls.md)

注：初赛阶段，文档中描述不会增加更多的syscall。可能会进一步完善文档内容描述，保证描述的准确性。

## syscalls测试用例
[syscalls测试用例](riscv-syscalls-testing/)

注：测试用例的数量会增加

## syscalls测试用例Qemu运行环境
[syscalls测试用例 for Linux on Qemu RV64运行环境](riscv-linux-rootfs) ： 在Linux上运行测试用例主要是用于对比自己实现的OS


