DOCKER ?= docker.educg.net/cg/os-contest:20250614

all: sdcard

build-all: build-rv build-la

build-rv:
	make -f Makefile.sub clean
	mkdir -p sdcard/riscv/musl
	make -f Makefile.sub PREFIX=riscv64-buildroot-linux-musl- DESTDIR=/code/sdcard/riscv/musl
	cp /opt/riscv64--musl--bleeding-edge-2020.08-1/riscv64-buildroot-linux-musl/sysroot/lib/libc.so sdcard/riscv/musl/lib
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-musl ####/g' sdcard/riscv/musl/*_testcode.sh

	make -f Makefile.sub clean
	mkdir -p sdcard/riscv/glibc
	make -f Makefile.sub PREFIX=riscv64-linux-gnu- DESTDIR=/code/sdcard/riscv/glibc
	cp /usr/riscv64-linux-gnu/lib/libc.so.6 sdcard/riscv/glibc/lib/libc.so
	cp /usr/riscv64-linux-gnu/lib/libc.so.6 sdcard/riscv/glibc/lib/
	cp /usr/riscv64-linux-gnu/lib/libm.so.6 sdcard/riscv/glibc/lib/libm.so
	cp /usr/riscv64-linux-gnu/lib/libm.so.6 sdcard/riscv/glibc/lib/
	cp /usr/riscv64-linux-gnu/lib/ld-linux-riscv64-lp64d.so.1 sdcard/riscv/glibc/lib/ld-linux-riscv64-lp64d.so.1
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-glibc ####/g' sdcard/riscv/glibc/*_testcode.sh

build-la:
	make -f Makefile.sub clean
	mkdir -p sdcard/loongarch/musl
	make -f Makefile.sub PREFIX=loongarch64-linux-musl- DESTDIR=/code/sdcard/loongarch/musl
	cp /opt/loongarch64-linux-musl-cross/loongarch64-linux-musl/lib/libc.so sdcard/loongarch/musl/lib
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-musl ####/g' sdcard/loongarch/musl/*_testcode.sh

	make -f Makefile.sub clean
	mkdir -p sdcard/loongarch/glibc
	make -f Makefile.sub PREFIX=loongarch64-linux-gnu- DESTDIR=/code/sdcard/loongarch/glibc
	cp /opt/gcc-13.2.0-loongarch64-linux-gnu/sysroot/usr/lib64/libc.so.6 sdcard/loongarch/glibc/lib
	cp /opt/gcc-13.2.0-loongarch64-linux-gnu/sysroot/usr/lib64/libm.so.6 sdcard/loongarch/glibc/lib
	cp /opt/gcc-13.2.0-loongarch64-linux-gnu/sysroot/usr/lib64/ld-linux-loongarch-lp64d.so.1 sdcard/loongarch/glibc/lib
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-glibc ####/g' sdcard/loongarch/glibc/*_testcode.sh

sdcard: build-all .PHONY
	dd if=/dev/zero of=sdcard-rv.img count=4096 bs=1M
	mkfs.ext4 sdcard-rv.img
	mkdir -p mnt
	mount sdcard-rv.img mnt
	cp -rL sdcard/riscv/* mnt
	cp mnt/musl/lib/dlopen_dso.so mnt/musl
	cp mnt/musl/lib/tls_get_new-dtv_dso.so mnt/musl
	cp mnt/glibc/lib/dlopen_dso.so mnt/glibc
	cp mnt/glibc/lib/tls_get_new-dtv_dso.so mnt/glibc
	umount mnt
	xz sdcard-rv.img

	dd if=/dev/zero of=sdcard-la.img count=4096 bs=1M
	mkfs.ext4 sdcard-la.img
	mkdir -p mnt
	mount sdcard-la.img mnt
	cp -rL sdcard/loongarch/* mnt
	cp mnt/musl/lib/dlopen_dso.so mnt/musl
	cp mnt/musl/lib/tls_get_new-dtv_dso.so mnt/musl
	cp mnt/glibc/lib/dlopen_dso.so mnt/glibc
	cp mnt/glibc/lib/tls_get_new-dtv_dso.so mnt/glibc
	umount mnt
	xz sdcard-la.img


clean:
	make -f Makefile.sub clean
	rm -rf sdcard/riscv/*
	rm -rf sdcard/loongarch/*
	rm -f sdcard-la.img.xz
	rm -f sdcard-rv.img.xz

docker:
	docker run --rm -it -v .:/code --entrypoint bash -w /code --privileged $(DOCKER)


.PHONY:
