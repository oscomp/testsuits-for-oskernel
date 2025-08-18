# 2025年全国大学生OS比赛--内核赛道--现场赛测例

## 现场赛题目(预先公布)
能正确运行如下程序，运行环境如下：
- QEMU RISC-V64 with virtio-net/virtio-block
- QEMU LoongArch64 with virtio-net/virtio-block
- RISC-V64 星光二代开发板 with 物理存储设备/物理网络设备
- LoongArch64 2K1000开发板 with 物理存储设备/物理网络设备


1. 支持运行git工具在本地文件系统和网络中的基本操作
   
  ```bash
  # basic
  git help
  # FS related
  git init
  cat >README.md
  git commit -m"add README.md"
  git log
  # NET related
  git clone ...
  git pull ...
  git push ...
  ```
注：如涉及块设备I/O操作，QEMU-*虚拟环境需基于 virtio-block，物理开发板需基于物理存储设备；如涉及网络设备I/O操作，QEMU-*虚拟环境需基于 virtio-net，物理开发板需基于物理网络设备；
允许参考/重用/改进已有的设备驱动程序。

## 现场赛题目（当天公布）
**现场赛的比赛当天公布**


## 比赛成绩占比说明&具体评分细则

请仔细阅读[全国大学生OS比赛官网](https://os.educg.net/)上公布的“2025-OS全国赛-技术方案”文档
