#!/bin/bash

set -e

. conf/riscv64.config

#
# test environment
#
for var in ARCH ABI CROSS_COMPILE BUSYBOX_VERSION \
    DROPBEAR_VERSION LINUX_KERNEL_VERSION; do
    if [ -z "${!var}" ]; then
        echo "${!var} not set" && exit 1
    fi
done

#
# find executables
#
for prog in ${CROSS_COMPILE}gcc sudo nproc curl openssl rsync; do
    if [ -z $(which ${prog}) ]; then
        echo "error: ${prog} not found in PATH" && exit 1
    fi
done

#
# download busybox, dropbear and linux
#
export MAKEFLAGS=-j4
test -d archives || mkdir archives
test -f archives/busybox-${BUSYBOX_VERSION}.tar.bz2 || \
    curl -L -o archives/busybox-${BUSYBOX_VERSION}.tar.bz2 \
        https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2
test -f archives/dropbear-${DROPBEAR_VERSION}.tar.bz2 || \
    curl -L -o archives/dropbear-${DROPBEAR_VERSION}.tar.bz2 \
        https://matt.ucc.asn.au/dropbear/releases/dropbear-${DROPBEAR_VERSION}.tar.bz2
test -f archives/linux-${LINUX_KERNEL_VERSION}.tar.gz || \
    curl -L -o archives/linux-${LINUX_KERNEL_VERSION}.tar.gz \
        https://git.kernel.org/torvalds/t/linux-${LINUX_KERNEL_VERSION}.tar.gz

#
# extract busybox, dropbear and linux
#
test -d build || mkdir build
test -d build/busybox-${BUSYBOX_VERSION} || \
    tar -C build -xjf archives/busybox-${BUSYBOX_VERSION}.tar.bz2
test -d build/dropbear-${DROPBEAR_VERSION} || \
    tar -C build -xjf archives/dropbear-${DROPBEAR_VERSION}.tar.bz2
test -d build/linux-${LINUX_KERNEL_VERSION} || \
    tar -C build -xzf archives/linux-${LINUX_KERNEL_VERSION}.tar.gz

#
# set default configurations
#
cp conf/busybox.config build/busybox-${BUSYBOX_VERSION}/.config
cp conf/linux.config build/linux-${LINUX_KERNEL_VERSION}/.config

#
# build busybox, dropbear and linux
#
test -x build/busybox-${BUSYBOX_VERSION}/busybox || (
    cd build/busybox-${BUSYBOX_VERSION}
    make ARCH=riscv CROSS_COMPILE=${CROSS_COMPILE} oldconfig
    make ARCH=riscv CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
)
test -x build/dropbear-${DROPBEAR_VERSION}/dropbear || (
    cd build/dropbear-${DROPBEAR_VERSION}
    ./configure --host=${CROSS_COMPILE%-} --disable-zlib
    make -j$(nproc) PROGRAMS="dropbear dbclient dropbearkey dropbearconvert scp"
)
test -x build/linux-${LINUX_KERNEL_VERSION}/vmlinux || (
    cd build/linux-${LINUX_KERNEL_VERSION}
    make ARCH=riscv CROSS_COMPILE=${CROSS_COMPILE} olddefconfig
    make -j$(nproc) ARCH=riscv CROSS_COMPILE=${CROSS_COMPILE} vmlinux
)

#
# build bbl
#
test -d build/riscv-pk || mkdir build/riscv-pk
test -x build/riscv-pk/bbl || (
    cd build/riscv-pk
    ../../src/riscv-pk/configure \
        --host=${CROSS_COMPILE%-} \
        --with-payload=../linux-${LINUX_KERNEL_VERSION}/vmlinux
    make -j$(nproc)
)

#
# create filesystem image
#
sudo env PATH=${PATH} UID=$(id -u) GID=$(id -g) \
./scripts/image.sh
