#!/bin/bash

elf=$1

riscv64-unknown-elf-gcc -I../../include -march=rv64imac -mabi=lp64 -mcmodel=medany -fno-builtin -nostdinc -fno-stack-protector -ggdb -Wall -O3 -DNDEBUG  -nostdlib -T ../../lib/arch/riscv/user.ld -Ttext 0x1000 $elf -o $elf.bin \
../../build/libulib.a

