DOCKER ?= docker.educg.net/cg/os-contest:20250714

all: sdcard

build-all: build-rv build-la

build-rv:
	make -f Makefile.sub clean
	mkdir -p sdcard/riscv/
	make -f Makefile.sub PREFIX=riscv64-linux-gnu- DESTDIR=/code/sdcard/riscv/glibc
	rm -fr /code/sdcard/riscv/glibc/usr/libexec/git-core
	mkdir -p /code/sdcard/riscv/musl
	cp -fr /code/sdcard/riscv/glibc/* /code/sdcard/riscv/musl
	cp scripts/git_testcode.sh /code/sdcard/riscv/glibc
	cp scripts/git_testcode.sh /code/sdcard/riscv/musl
	mkdir -p /code/sdcard/riscv/musl/lib
	mkdir -p /code/sdcard/riscv/glibc/lib
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-musl ####/g' sdcard/riscv/musl/*_testcode.sh
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-glibc ####/g' sdcard/riscv/glibc/*_testcode.sh

build-la:
	make -f Makefile.sub clean
	mkdir -p sdcard/loongarch/
	make -f Makefile.sub PREFIX=loongarch64-linux-gnu- DESTDIR=/code/sdcard/loongarch/glibc
	rm -fr /code/sdcard/loongarch/glibc/usr/libexec/git-core
	mkdir -p /code/sdcard/loongarch/musl
	cp -fr /code/sdcard/loongarch/glibc/* /code/sdcard/loongarch/musl
	mkdir -p /code/sdcard/loongarch/musl/lib
	mkdir -p /code/sdcard/loongarch/glibc/lib
	cp scripts/git_testcode.sh /code/sdcard/loongarch/glibc
	cp scripts/git_testcode.sh /code/sdcard/loongarch/musl
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-musl ####/g' sdcard/loongarch/musl/*_testcode.sh
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-glibc ####/g' sdcard/loongarch/glibc/*_testcode.sh

sdcard: build-all .PHONY
	dd if=/dev/zero of=sdcard-rv.img count=512 bs=1M
	mkfs.ext4 sdcard-rv.img
	mkdir -p mnt
	mount sdcard-rv.img mnt
	cp -rL sdcard/riscv/* mnt
	umount mnt
	gzip sdcard-rv.img

	dd if=/dev/zero of=sdcard-la.img count=512 bs=1M
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
