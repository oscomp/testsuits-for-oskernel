DOCKER ?= docker.educg.net/cg/os-contest:20250714

all: sdcard

build-all: build-rv build-la

build-rv:
	make -f Makefile.sub clean
	mkdir -p sdcard/riscv/
	make -f Makefile.sub PREFIX=riscv64-linux-gnu- DESTDIR=/code/sdcard/riscv/glibc
	cp -fr /code/sdcard/riscv/glibc /code/sdcard/riscv/musl

build-la:
	make -f Makefile.sub clean
	mkdir -p sdcard/loongarch/
	make -f Makefile.sub PREFIX=loongarch64-linux-gnu- DESTDIR=/code/sdcard/loongarch/glibc
	cp -fr /code/sdcard/loongarch/glibc /code/sdcard/loongarch/musl 		

sdcard: build-all .PHONY
	dd if=/dev/zero of=sdcard-rv.img count=128 bs=1M
	mkfs.ext4 sdcard-rv.img
	mkdir -p mnt
	mount sdcard-rv.img mnt
	cp -rL sdcard/riscv/* mnt
	umount mnt
	gzip sdcard-rv.img

	dd if=/dev/zero of=sdcard-la.img count=128 bs=1M
	mkfs.ext4 sdcard-la.img
	mkdir -p mnt
	mount sdcard-la.img mnt
	cp -rL sdcard/loongarch/* mnt
	umount mnt
	gzip sdcard-la.img


clean:
	make -f Makefile.sub clean
	rm -rf sdcard/riscv/*
	rm -rf sdcard/loongarch/*
	rm -f sdcard-la.img.gz
	rm -f sdcard-rv.img.gz

docker:
	docker run --rm -it -v .:/code --entrypoint bash -w /code --privileged $(DOCKER)


.PHONY:
