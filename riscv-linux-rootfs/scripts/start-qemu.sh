#!/bin/bash
set -x

. conf/riscv64.config

while [[ $# -gt 0 ]] && [[ ."$1" = .--* ]] ;
do
  opt="$1"
  shift
  case "${opt}" in
    "--" )
      break 2
      ;;
    "--no-exec" )
      NOEXEC=true
      ;;
    *)
      echo >&2 "$0: unknown option: ${opt}";
      exit 1
      ;;
  esac
done

QEMU_NETDEV="type=tap,script=./scripts/ifup.sh,downscript=./scripts/ifdown.sh"

# locate QEMU
QEMU_SYSTEM_BIN=$(which qemu-system-${ARCH})
if [ -z ${QEMU_SYSTEM_BIN} ]; then
  echo "Cannot locate qemu-system-${ARCH}"
  exit 1
fi

# construct command
cmd="${QEMU_SYSTEM_BIN} -nographic -machine virt \
	-kernel build/riscv-pk/bbl \
	-append \"root=/dev/vda1 ro console=ttyS0\" \
	-drive file=riscv64-rootfs.bin,format=raw,id=hd0 \
	-device virtio-blk-device,drive=hd0 \
	-netdev ${QEMU_NETDEV},id=net0 \
	-device virtio-net-device,netdev=net0"

# print or execute command
if [ "${NOEXEC}" = "true" ] ; then
  echo ${cmd} $*
else
  eval "exec sudo ${cmd} $*"
fi
