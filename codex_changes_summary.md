# 本次修改总结

本文档记录了为解决 `sudo make docker` 后在容器内执行 `make all` 过程中出现的交叉编译错误而做的修改。
以下错误可能是因为docker-image版本不同，脚本默认的image[docker.educg.net/cg/os-contest:20250614]无法获取，采用官网的image版本：DOCKER="zhouzhouyi/os-contest:20260104"；需要先sudo docker pull zhouzhouyi/os-contest:20260104, 然后运行sudo make docker DOCKER="zhouzhouyi/os-contest:20260104",进入docker环境后运行 make

## 1. 修复 lmbench 交叉编译错误

### 问题现象

构建 `lmbench` 时，RISC-V musl 交叉编译器报错：

```text
riscv64-buildroot-linux-musl-gcc: ERROR: unsafe header/library path used in cross-compilation: '-I/usr/include/tirpc'
```

### 根因

`lmbench_src/src/Makefile` 中把主机头文件目录 `/usr/include/tirpc` 硬编码进了编译参数。  
这在交叉编译场景下会被 GCC 视为“不安全的宿主机头文件路径”。

### 修改内容

修改文件：

- `lmbench_src/src/Makefile`

将原来的：

```make
COMPILE=$(CC) $(CFLAGS) -I/usr/include/tirpc  $(CPPFLAGS) $(LDFLAGS)
```

改为：

```make
TIRPC_DIR ?= ../libtirpc-1.3.6
TIRPC_CPPFLAGS ?= -I$(TIRPC_DIR)/tirpc
COMPILE=$(CC) $(CFLAGS) $(TIRPC_CPPFLAGS) $(CPPFLAGS) $(LDFLAGS)
```

### 修改效果

- 不再依赖宿主机 `/usr/include/tirpc`
- 改为使用仓库内本地构建的 `libtirpc`
- 消除 `unsafe header/library path used in cross-compilation` 报错

## 2. 修复顶层 Makefile 中写死的工具链库路径

### 问题现象

在完成前面阶段后，顶层构建在复制运行时库时失败：

```text
cp: cannot stat '/opt/riscv64--musl--bleeding-edge-2020.08-1/.../libc.so': No such file or directory
```

### 根因

顶层 `Makefile` 中将若干交叉工具链的库文件路径硬编码为特定 `/opt/...` 目录。  
容器中的工具链版本已经变化，原路径不存在，因此复制失败。

### 修改内容

修改文件：

- `Makefile`

将这些硬编码路径替换为通过对应交叉编译器动态查询实际库文件位置，例如：

```sh
riscv64-buildroot-linux-musl-gcc -print-file-name=libc.so
```

涉及的工具链包括：

- `riscv64-buildroot-linux-musl-gcc`
- `riscv64-linux-gnu-gcc`
- `loongarch64-linux-musl-gcc`
- `loongarch64-linux-gnu-gcc`

### 修改效果

- 不再依赖具体 `/opt/...` 版本目录
- 适配当前容器中实际安装的交叉工具链
- 降低后续镜像升级导致构建脚本失效的概率

## 3. 关闭 BusyBox 中无法通过当前头文件编译的 tc applet

### 问题现象

BusyBox 构建时在 `networking/tc.c` 失败，典型报错包括：

```text
error: 'TCA_CBQ_MAX' undeclared
error: 'TCA_CBQ_RATE' undeclared
error: invalid application of 'sizeof' to incomplete type 'struct tc_cbq_lssopt'
```

### 根因

BusyBox 的 `tc` applet 依赖一组较旧的 CBQ 内核头文件定义。  
当前交叉工具链附带的 Linux 头文件中没有这些宏和结构体定义，因此源码无法通过编译。

### 修改内容

修改文件：

- `config/busybox-config-riscv64`
- `config/busybox-config-loongarch64`

将：

```config
CONFIG_TC=y
CONFIG_FEATURE_TC_INGRESS=y
```

改为：

```config
# CONFIG_TC is not set
# CONFIG_FEATURE_TC_INGRESS is not set
```

### 修改效果

- BusyBox 不再编译 `networking/tc.c`
- 避开当前工具链头文件与 BusyBox `tc` 实现之间的兼容性问题
- 保留 BusyBox 其余大部分功能不受影响

## 当前已修改文件列表

- `Makefile`
- `lmbench_src/src/Makefile`
- `config/busybox-config-riscv64`
- `config/busybox-config-loongarch64`

## 建议的后续验证步骤

在 Docker 容器中重新执行：

```sh
make all
```

如果仍有新的错误，建议把最后 100 行日志保存下来继续定位。
