NPROC ?= 8
ARCH ?= x86_64
MUSL_PREFIX ?= $(ARCH)-linux-musl
MUSL_GCC = $(MUSL_PREFIX)-gcc
MUSL_STRIP = $(MUSL_PREFIX)-strip

# CC="musl-gcc -static" ./configure --prefix=$HOME/musl
# ./configure --host=x86_64-linux-musl --with-sysroot=/path/to/sysroot
# make TARGET=x86_64-linux-musl install

build_all: busybox lua lmbench libctest iozone libc-bench netperf iperf unix-bench cyclictest time-test test_all true copy-file-range-test interrupts-test

busybox: .PHONY
	cp busybox-config busybox/.config
	make -C busybox CC="$(MUSL_GCC) -static" CFLAGS_busybox="-no-pie" STRIP=$(MUSL_STRIP) -j$(NPROC)
	$(MUSL_STRIP) busybox/busybox
	cp busybox/busybox sdcard/
	cp scripts/busybox/* sdcard/

lua: .PHONY
	make -C lua CC="$(MUSL_GCC) -static" -j $(NPROC)
	$(MUSL_STRIP) lua/src/lua
	cp lua/src/lua sdcard/
	cp scripts/lua/* sdcard/

lmbench: .PHONY
	make -C lmbench build CC="$(ARCH)-linux-gnu-gcc -static" OS=$(ARCH) -j $(NPROC)
	$(ARCH)-linux-gnu-strip lmbench/bin/$(ARCH)/lmbench_all
	# riscv64-linux-gnu-strip lmbench/bin/riscv64/hello
	cp lmbench/bin/$(ARCH)/lmbench_all sdcard/
	cp lmbench/bin/$(ARCH)/hello sdcard/
	cp scripts/lmbench/* sdcard/

libctest: .PHONY
	make -C libc-test PREFIX=$(MUSL_PREFIX) disk -j $(NPROC)
	cp libc-test/disk/* sdcard/
	mv sdcard/run-all.sh sdcard/libctest_testcode.sh

iozone: .PHONY
	make -C iozone linux CC="$(MUSL_GCC) -static" -j $(NPROC)
	$(MUSL_STRIP) iozone/iozone
	cp iozone/iozone sdcard/
	cp scripts/iozone/* sdcard/

libc-bench: .PHONY
	make -C libc-bench CC="$(MUSL_GCC) -static" -j $(NPROC)
	$(MUSL_STRIP) libc-bench/libc-bench
	cp libc-bench/libc-bench sdcard/libc-bench

unix-bench: .PHONY
	make -C UnixBench CC=$(MUSL_GCC) ARCH=$(ARCH) -j $(NPROC) all
	cp UnixBench/pgms/* sdcard
	cp scripts/unixbench/*.sh sdcard

netperf: .PHONY
	cd netperf && ./autogen.sh
	cd netperf && ac_cv_func_setpgrp_void=yes ./configure --host=$(MUSL_PREFIX) CC=$(MUSL_GCC) CFLAGS="-static"
	cd netperf && make -j $(NPROC)
	cp netperf/src/netperf netperf/src/netserver sdcard/
	cp scripts/netperf/* sdcard/

iperf: .PHONY
	cd iperf &&	./configure --host=$(MUSL_PREFIX) CC=$(MUSL_GCC) --enable-static-bin --without-sctp --without-openssl && make
	cp iperf/src/iperf3 sdcard/
	cp scripts/iperf/iperf_testcode.sh sdcard/

cyclictest: .PHONY
	make -C rt-tests PREFIX=$(MUSL_PREFIX) NUMA=0 cyclictest hackbench
	cp rt-tests/cyclictest rt-tests/hackbench sdcard/
	cp scripts/cyclictest/cyclictest_testcode.sh sdcard/

time-test: .PHONY
	make CC=$(MUSL_GCC) -C time-test all
	cp time-test/time-test sdcard

test_all: .PHONY
	cp scripts/test_all.sh sdcard/test_all.sh

true: .PHONY
	make CC=$(MUSL_GCC) -C true
	$(MUSL_STRIP) true/true
	mkdir -p sdcard/bin
	cp true/true sdcard/bin/

copy-file-range-test: .PHONY
	make CC=$(MUSL_GCC) -C $@
	$(MUSL_STRIP) $@/$@-*
	cp $@/$@-* sdcard/

interrupts-test: .PHONY
	make CC=$(MUSL_GCC) -C $@
	$(MUSL_STRIP) $@/$@-*
	cp $@/$@-* sdcard/

sdcard: build_all .PHONY
	dd if=/dev/zero of=sdcard.img count=62768 bs=1K
	mkfs.vfat -F 32 sdcard.img
	mkdir -p mnt
	mount -t vfat -o user,umask=000,utf8=1 --source sdcard.img --target mnt
	cp -r sdcard/* mnt
	umount mnt
	dd if=/dev/zero of=disk.img count=12K bs=1K
	mkfs.vfat -F 32 disk.img
	mkdir -p mnt
	mount -t vfat -o user,umask=000,utf8=1 --source disk.img --target mnt
	cp -r sdcard/* mnt
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
	make -C netperf clean
	make -C iperf clean
	make -C UnixBench clean
	make -C time-test clean
	make -C rt-tests clean
	make -C true clean
	make -C copy-file-range-test clean
	make -C interrupts-test clean
	- rm -rf sdcard/*
	- rm sdcard.img
	- rm sdcard.img.gz

.PHONY:
