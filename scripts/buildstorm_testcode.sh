#!/bin/sh
# BuildStorm testcode (contestant-facing reference) -- runs INSIDE the guest,
# on the kernel under test. The guest image is a self-contained rootfs
# (Debian glibc + rust toolchain + tgoskits sources + cargo cache); the student
# mounts it as their rootfs.
#
#
# 输出 (被 judge/judge_buildstorm.py 解析):
#   BUILDSTORM_TOOLCHAIN ok|fail                         (8 分)
#   BUILDSTORM_MINIBUILD ok|fail                         (12 分)
#   BUILDSTORM_COMPILE mode=multi ok=true|false elapsed_s=<s> cores=<n> bytes=<n> arch=<a>
#                                                         (40 + 120 分)

echo "#### OS COMP TEST GROUP START buildstorm ####"

mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
mount -t devtmpfs devtmpfs /dev 2>/dev/null

export PATH=/root/.cargo/bin:/usr/local/bin:/usr/bin:/bin:/sbin:/usr/sbin
export HOME=/root RUSTUP_HOME=/root/.rustup CARGO_HOME=/root/.cargo
export RUSTUP_TOOLCHAIN=nightly-2026-05-28
export CARGO_NET_OFFLINE=true

case "$(uname -m 2>/dev/null)" in
  loongarch64) AXARCH=loongarch64; AXTGT=loongarch64-unknown-linux-musl ;;
  riscv64)     AXARCH=riscv64;     AXTGT=riscv64gc-unknown-linux-musl ;;
  *)           AXARCH=riscv64;     AXTGT=riscv64gc-unknown-linux-musl ;;
esac

if rustc --version && cargo --version; then
    echo "BUILDSTORM_TOOLCHAIN ok"
else
    echo "BUILDSTORM_TOOLCHAIN fail"
fi

rm -rf /tmp/minibuild
if cargo new --vcs none /tmp/minibuild >/dev/null 2>&1 \
   && ( cd /tmp/minibuild && cargo build >/dev/null 2>&1 ) \
   && [ "$(/tmp/minibuild/target/debug/minibuild)" = "Hello, world!" ]; then
    echo "BUILDSTORM_MINIBUILD ok"
else
    echo "BUILDSTORM_MINIBUILD fail"
fi

cd /work/tgoskits 2>/dev/null || {
    echo "BUILDSTORM_COMPILE mode=multi ok=false elapsed_s=0 cores=$(nproc) bytes=0 arch=$AXARCH"
    echo "#### OS COMP TEST GROUP END buildstorm ####"
    exit 1
}

rm -rf "target/$AXTGT"

echo "----- pre-build tg-xtask (untimed) -----"
cargo build -p tg-xtask 2>&1 || true

echo "----- build arceos-helloworld (timed, arch=$AXARCH) -----"
echo "BUILDSTORM_BEGIN mode=multi"
T0=$(cut -d' ' -f1 /proc/uptime 2>/dev/null)
{ timeout 14400 cargo xtask arceos build -p arceos-helloworld --arch "$AXARCH" 2>&1; \
  echo $? > /work/.build.rc; } | tee /work/buildstorm.build.out
RC=$(cat /work/.build.rc 2>/dev/null || echo 1); rm -f /work/.build.rc
T1=$(cut -d' ' -f1 /proc/uptime 2>/dev/null)
ELAPSED=$(awk "BEGIN{printf \"%.2f\", (\"$T1\"+0)-(\"$T0\"+0)}" 2>/dev/null); [ -z "$ELAPSED" ] && ELAPSED=0

ART=$(find target -type f \( -name 'arceos-helloworld' -o -name 'helloworld' \) 2>/dev/null | head -1)
BYTES=0
[ -n "$ART" ] && BYTES=$(wc -c <"$ART")

if [ "$RC" -eq 0 ] && [ -n "$ART" ] && [ "$BYTES" -ge 500000 ]; then
    echo "BUILDSTORM_COMPILE mode=multi ok=true elapsed_s=$ELAPSED cores=$(nproc) bytes=$BYTES arch=$AXARCH"
else
    echo "BUILDSTORM_COMPILE mode=multi ok=false rc=$RC elapsed_s=$ELAPSED cores=$(nproc) bytes=$BYTES arch=$AXARCH"
    echo "----- buildstorm.build.out tail -----"
    tail -25 /work/buildstorm.build.out 2>/dev/null
fi

echo "#### OS COMP TEST GROUP END buildstorm ####"
sync
