# Linux rootfs for RISC-V

This is a tiny RISC-V Linux root filesystem image that targets
the `virt` machine in riscv-qemu.

The root image is intended to demonstrate virtio-net and virtio-block in
riscv-qemu and features a dropbear ssh server which allows out-of-the-box
ssh access to a RISC-V virtual machine.

[运行syscalls测试用例](#运行syscalls测试用例)
## Dependencies

- [riscv-gnu-toolchain](https://github.com/riscv/riscv-gnu-toolchain) (RISC-V Linux toolchain, recommended with Linux-elf/glibc)
- [busybox](https://busybox.net/) (downloaded automatically)
- [dropbear](https://matt.ucc.asn.au/dropbear/dropbear.html) (downloaded automatically)
- sudo, curl, openssl and rsync

## Build

The build process downloads busybox and dropbear, compiles them and prepares
a root filesystem image to the file `riscv64-rootfs.bin`.

```
export PATH=/path/to/your/riscv/toolchain:$PATH
make
```
which runs executes this script: `scripts/build.sh`

## Running

Requires the riscv-qemu `virt` board with virtio-block and virtio-net devices.

The following command starts riscv64 Linux:

```
make run
```

which runs executes this command:

```
sudo qemu-system-riscv64 -nographic -machine virt \
  -kernel bbl -append "root=/dev/vda ro console=ttyS0" \
  -drive file=riscv64-rootfs.bin,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -netdev type=tap,script=scripts/ifup.sh,downscript=scripts/ifdown.sh,id=net0 \
  -device virtio-net-device,netdev=net0
```

After booting the virtual machine you should be able to ssh into it.

```
$ ssh root@192.168.100.2
```

## 运行syscalls测试用例
用户名root密码oscomp登入Qemu虚拟机
<br>
`fdisk -l`可看到磁盘分区/dev/vda2是fat32格式，用于模拟比赛的sdcard fat32环境，
请确认将此分区挂载到/mnt中；
```
mount -t vfat /dev/vda2 /mnt
```

在交叉编译完[syscalls测试用例](../riscv-syscalls-testing)，可以通过`scp`程序，把测试用例传入Qemu虚拟机中
<br>
执行测试
```
cd /mnt/riscv64
./run-all.sh
```

---
如需快速试跑测例，您也可以直接使用已编译好的[syscalls测试环境镜像](https://cloud.tsinghua.edu.cn/d/0b7eeedccacc44939e3c/)
