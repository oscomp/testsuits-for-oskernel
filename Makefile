DOCKER ?= docker.educg.net/cg/os-contest:20250614

all: sdcard

build-all: build-rv build-la

build-rv:
	make -f Makefile.sub clean
	mkdir -p sdcard/riscv/musl
	make -f Makefile.sub PREFIX=riscv64-buildroot-linux-musl- DESTDIR=/code/sdcard/riscv/musl
	RV_MUSL_LIBC="$$(riscv64-buildroot-linux-musl-gcc -print-file-name=libc.so)"; \
	test -f "$$RV_MUSL_LIBC"; \
	cp "$$RV_MUSL_LIBC" sdcard/riscv/musl/lib
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-musl ####/g' sdcard/riscv/musl/*_testcode.sh

	make -f Makefile.sub clean
	mkdir -p sdcard/riscv/glibc
	make -f Makefile.sub PREFIX=riscv64-linux-gnu- DESTDIR=/code/sdcard/riscv/glibc
	RV_GLIBC_LIBC="$$(riscv64-linux-gnu-gcc -print-file-name=libc.so.6)"; \
	RV_GLIBC_LIBM="$$(riscv64-linux-gnu-gcc -print-file-name=libm.so.6)"; \
	RV_GLIBC_LD="$$(riscv64-linux-gnu-gcc -print-file-name=ld-linux-riscv64-lp64d.so.1)"; \
	test -f "$$RV_GLIBC_LIBC"; \
	test -f "$$RV_GLIBC_LIBM"; \
	test -f "$$RV_GLIBC_LD"; \
	cp "$$RV_GLIBC_LIBC" sdcard/riscv/glibc/lib/libc.so; \
	cp "$$RV_GLIBC_LIBC" sdcard/riscv/glibc/lib/; \
	cp "$$RV_GLIBC_LIBM" sdcard/riscv/glibc/lib/libm.so; \
	cp "$$RV_GLIBC_LIBM" sdcard/riscv/glibc/lib/; \
	cp "$$RV_GLIBC_LD" sdcard/riscv/glibc/lib/ld-linux-riscv64-lp64d.so.1
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-glibc ####/g' sdcard/riscv/glibc/*_testcode.sh

build-la:
	make -f Makefile.sub clean
	mkdir -p sdcard/loongarch/musl
	make -f Makefile.sub PREFIX=loongarch64-linux-musl- DESTDIR=/code/sdcard/loongarch/musl
	LA_MUSL_LIBC="$$(loongarch64-linux-musl-gcc -print-file-name=libc.so)"; \
	test -f "$$LA_MUSL_LIBC"; \
	cp "$$LA_MUSL_LIBC" sdcard/loongarch/musl/lib
	sed -E -i 's/#### OS COMP TEST GROUP ([^ ]+) ([^ ]+) ####/#### OS COMP TEST GROUP \1 \2-musl ####/g' sdcard/loongarch/musl/*_testcode.sh

	make -f Makefile.sub clean
	mkdir -p sdcard/loongarch/glibc
	make -f Makefile.sub PREFIX=loongarch64-linux-gnu- DESTDIR=/code/sdcard/loongarch/glibc
	LA_GLIBC_LIBC="$$(loongarch64-linux-gnu-gcc -print-file-name=libc.so.6)"; \
	LA_GLIBC_LIBM="$$(loongarch64-linux-gnu-gcc -print-file-name=libm.so.6)"; \
	LA_GLIBC_LD="$$(loongarch64-linux-gnu-gcc -print-file-name=ld-linux-loongarch-lp64d.so.1)"; \
	test -f "$$LA_GLIBC_LIBC"; \
	test -f "$$LA_GLIBC_LIBM"; \
	test -f "$$LA_GLIBC_LD"; \
	cp "$$LA_GLIBC_LIBC" sdcard/loongarch/glibc/lib; \
	cp "$$LA_GLIBC_LIBM" sdcard/loongarch/glibc/lib; \
	cp "$$LA_GLIBC_LD" sdcard/loongarch/glibc/lib
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
