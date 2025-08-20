# 2025年全国大学生OS比赛--内核赛道--现场赛测例

## 现场赛题目

### 目标

在四种平台运行环境下，验证自己设计的操作系统内核能够访问文件系统，正确执行Linux软件。
### 比赛时间段
**2025年8月20日8:00AM~18:00PM**

### 注意事项

- **今天（8月20日）可以继续提交线上决赛的两大类测例（线上决赛测例和初赛测例），展示截至时间是今天下午18：00，代码&文档提交截至时间是今天下午20:00。这样线上决赛成绩=MAX（今天的最高成绩，8月17日截止期的成绩）。
为确保今天最高成绩的成果有效性和可复现（可复查）性，要求参赛队不能清空/删除每次提交的git记录，特别是与最高成绩相关的git记录一定不能删除。参赛队的仓库可继续保持private状态。**
- **本次现场比赛不设自动评分，各队伍完成下列任意题目中的小题后，登录[比赛提交网站](https://course.educg.net)，在相应题目中提交完成截图，并通过微信等方式呼叫现场评审老师进行检查，通过检查后即可得到分数。**

- 四种平台运行环境如下：

  - QEMU RISC-V64 with virtio-net/virtio-block

  - QEMU LoongArch64 with virtio-net/virtio-block

  - RISC-V64 星光二代开发板 with 物理存储设备/物理网络设备

  - LoongArch64 2K1000开发板 with 物理存储设备/物理网络设备

- **如涉及块设备I/O操作，QEMU-RV/LA虚拟环境需基于 virtio-block，物理开发板需基于物理存储设备（非ramdisk）；如涉及网络设备I/O操作，QEMU-RV/LA虚拟环境需基于 virtio-net，物理开发板需基于物理网络设备(非loopback)； 允许参考/重用/改进已有的设备驱动程序。**
- 下面题目涉及的主要Linux软件：git, vim, gcc, rustc，以及常用linux软件：busybox, bash等。包含这些程序的文件系统镜像如下：

  - [包含上述程序的riscv64 linux ext4fs镜像压缩包](https://github.com/LearningOS/rust-based-os-comp2025/releases/download/alpine-linux-riscv64-ext4fs/alpine-linux-riscv64-ext4fs.img.xz)

  - [包含上述程序的loongarch64 linux ext4fs镜像压缩包](https://github.com/LearningOS/rust-based-os-comp2025/releases/download/alpine-linux-loongarch64-ext4fs/alpine-linux-loongarch64-ext4fs.img.xz)

- **参赛队可以使用自己制作的ext4fs镜像，但是要保证git、vim、gcc和rustc的与上述镜像中的[linux应用文件](https://github.com/oscomp/testsuits-for-oskernel/blob/on-site-final-2025/soft-info.txt)是一致的。（不建议用源代码编译生成，开销太大）**


## 第一题：git功能实现，共3小题 (共55分)

### 1.1 Task0 git -h (加载运行) ===============（5分）

```
#相关命令参考
git help
```

### 1.2 Task1 （文件系统相关功能） =============== （20分）

文件镜像（包含 proj repo dir）

```
#相关命令参考
cd proj
git init (5分)  
cat >README.md
git add . (5分)
git commit -m"add README.md" (5分)
git log (5分)
```

### 1.3 Task2 （网络相关功能） =================（30分）

从如下之一的远程git repo 执行clone操作，修改README，把更新后的本地仓库上传/下载一个对应远程网站的一个自己的远程仓库中

远程仓库列表：

-  https://github.com/oscomp/xv6-riscv
- https://gitlink.org.cn/oscomp/xv6-riscv
- https://gitee.com/oscomp/xv6-riscv

```
#相关命令参考
git config --global user.name "youname"  
git config --global user.email "alice@example.com"
git clone git@github.com:oscomp/xv6-riscv.git  my-folder（10分）
修改REDAME文件，git add ; git commit -m"update README"  
# 在对应远程网站上创建一个自己的远程仓库 YOURREPO 
git remote add me git@github.com:your/xv6-riscv.git
git push me （10分）
# 在对应远程网站的一个自己的远程仓库 YOURREPO 上远程修改README
git pull me (10分)
```

## 第二题：vim功能实现，共2小题（15分）

### 2.1 vim -h (加载运行)  ================= 5分

### 2.2 vim hello.c(正常编辑存储)    ================= 10分

## 第三题：gcc功能实现，共2小题 (15分)

### 3.1 gcc --h  (加载运行) ================(5分）

### 3.2 gcc hello.c && ./a.out （正确编译并运行 ）==============(10分)

- helloworld.c
```c
#include <stdio.h>

int main(void) {
    printf("Hello, World!\n");
    return 0;
}
```

## 第四题：rustc功能实现，共2小题 (15分)

### 4.1 rustc -h  (加载运行)  5分

#### 4.2 rustc helloworld.rs && ./helloworld （正确编译并运行 ）(10分)

- helloworld.rs

```rust
fn main() {
    println!("Hello, World!");
}
```


## 比赛成绩占比说明&具体评分细则

请仔细阅读[全国大学生OS比赛官网](https://os.educg.net/)上公布的“2025-OS全国赛-技术方案”文档
