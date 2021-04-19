# fat32 fs info
- [FAT on osdev](https://wiki.osdev.org/FAT) 中的fat32相关的内容
- [FAT32 File System Specification](http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc) 有示例的fat32文档规范（fat12/16等不用关注）

在github上搜索fat32 关键字，可查询到不少fat32的参考实现

---
附: Linux系统中格式化sdcard为fat32文件系统
1. 通过读卡器把sdcard插入Linux系统机器；
2. 使用工具fdisk操作sdcard的盘号：/dev/sdx，盘号会根据实际机器不同而改变:
```
fdisk /dev/sdx
o #创建分区表
n #新建分区
p #设置为主分区
1 #设置为分区1
  #后一路回车确认
  
p #查看已创建的分区，这应该会显示有/dev/sdx1

t #修改分区类型，选择分区1
c #设置为fat32类型

w #最后把修改写入sdcard盘，后退出
```
3.最后可查看sdcard盘的分区信息 `fdisk -l /dev/sdx`
