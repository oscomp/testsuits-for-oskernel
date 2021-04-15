all: riscv64-rootfs.bin

clean:
	rm -fr build riscv64-rootfs.bin

distclean: clean
	rm -fr archives

riscv64-rootfs.bin: scripts/build.sh
	./scripts/build.sh

run: scripts/start-qemu.sh
	./scripts/start-qemu.sh

archive:
	tar --exclude build --exclude archives --exclude riscv64-rootfs.bin \
	    -C .. -cjf ../riscv64-linux.tar.bz2 $(shell basename $(PWD))
