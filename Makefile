NPROC = 16
MUSL_PREFIX = riscv64-linux
MUSL_GCC = $(MUSL_PREFIX)-gcc
MUSL_STRIP = $(MUSL_PREFIX)-strip

build_all: busybox lua lmbench libctest iozone libc-bench

busybox: .PHONY
	cp busybox-config busybox/.config
	make -C busybox CC="$(MUSL_GCC) -static" STRIP=$(MUSL_STRIP) -j$(NPROC)
	cp busybox/busybox sdcard/
	cp scripts/busybox/* sdcard/

lua: .PHONY
	make -C lua CC="$(MUSL_GCC) -static" -j $(NPROC)
	cp lua/src/lua sdcard/
	cp scripts/lua/* sdcard/

lmbench: .PHONY
	make -C lmbench build CC="riscv64-linux-gnu-gcc -static" OS=riscv64 -j $(NPROC)
	cp lmbench/bin/riscv64/lmbench_all sdcard/
	cp scripts/lmbench/* sdcard/

libctest: .PHONY
	make -C libc-test disk -j $(NPROC)
	cp libc-test/disk/* sdcard/
	mv sdcard/run-all.sh sdcard/libctest_testcode.sh

iozone: .PHONY
	make -C iozone linux CC="$(MUSL_GCC) -static" -j $(NPROC)
	cp iozone/iozone sdcard/
	cp scripts/iozone/* sdcard/

libc-bench: .PHONY
	make -C libc-bench CC="$(MUSL_GCC) -static" -j $(NPROC)
	cp libc-bench/libc-bench sdcard/libc-bench

sdcard: build_all
	dd if=/dev/zero of=sdcard.img count=62768 bs=1K
	mkfs.vfat -F 32 sdcard.img
	mkdir -p mnt
	guestmount -a sdcard.img -m /dev/sda --rw mnt
	cp sdcard/* mnt
	umount mnt

qemu: .PHONY
	cd oscomp-debian && ./run.sh

clean: .PHONY
	make -C busybox clean
	make -C lua clean
	make -C lmbench clean
	make -C libc-test clean
	make -C iozone clean
	make -C libc-bench clean
	- rm sdcard.img
	- rm sdcard.img.gz

.PHONY:
