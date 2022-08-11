# 2022操作系统大赛决赛第一阶段

决赛使用[musl](https://musl.libc.org/)和[libc-test](http://nsz.repo.hu/git/?p=libc-test)作为赛题。与初赛相同，编译好的elf文件存放在SD卡根目录中，需要内核初始化完成后主动依次运行。测试样例程序名和参数请参考release中run-static.sh和run-dynamic.sh两个文件内容。

本次大赛新增对动态链接功能的考察，程序所依赖的动态链接库也一并放在了SD卡根目录中。entry-dynamic.exe文件依赖程序解释器/lib/ld-musl-riscv64-sf.so.1，但它仅是对libc.so的符号链接，由于fat32文件系统不支持符号链接功能，因此尚未提供该文件，请各参赛队伍自行将对该文件的访问转发至libc.so。

Riscv交叉编译：
1. 修改Makefile中的MUSL_LIB和PREFIX为恰当的值
2. make disk
3. 在disk目录中生成riscv可执行文件和动态链接库

X86本机测试：
1. export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:src/functional:src/regression
2. 修改Makefile中CC=musl-gcc，OBJCOPY=objcopy
3. make run

如果 x86 本机测试失败，可尝试以下步骤：

1. 复制以下脚本到当前目录

```py
# fix.py
file_path = "./entry.h"
with open( file_path,'r') as open_file:
    content = open_file.read()
    content = content.replace("exe\n\"", "exe\"")
with open(file_path, 'w') as open_file:
    open_file.write(content)

with open( file_path,'r') as open_file:
    content = open_file.read()
    content = content.replace(".exe\n_main", "_main")
with open(file_path, 'w') as open_file:
    open_file.write(content)

with open( file_path,'r') as open_file:
    content = open_file.read()
    content = content.replace(".exe\"", "\"")
with open(file_path, 'w') as open_file:
    open_file.write(content)
```

2. 修改 Makefile

```diff
+ .PHONY: entry.h

so: $(DSO_SOS)

entry.c: entry.h

entry.h: dynamic.txt static.txt
-	printf "#ifdef STATIC\n" >> entry.h
+	printf "#ifdef STATIC\n" > entry.h
	cat static.txt | xargs -I AA basename AA .exe | sed 's/-/_/g' | xargs -I BB printf "int %s_main(int, char **);\n" BB >> entry.h
	printf "struct {const char *name; int (*func)(int, char**);} table [] = {\n" >> entry.h
	cat static.txt | xargs -I AA basename AA .exe | sed 's/-/_/g' | xargs -I BB printf "\t{\"%s\", %s_main},\n" BB BB >> entry.h
	printf "\t{0, 0}\n" >> entry.h
	printf "};\n" >> entry.h
	printf "#endif\n\n" >> entry.h

	printf "#ifdef DYNAMIC\n" >> entry.h
	cat dynamic.txt | xargs -I AA basename AA .exe | sed 's/-/_/g' | xargs -I BB printf "int %s_main(int, char **);\n" BB >> entry.h
	printf "struct {const char *name; int (*func)(int, char**);} table [] = {\n" >> entry.h
	cat dynamic.txt | xargs -I AA basename AA .exe | sed 's/-/_/g' | xargs -I BB printf "\t{\"%s\", %s_main},\n" BB BB >> entry.h
	printf "\t{0, 0}\n" >> entry.h
	printf "};\n" >> entry.h
	printf "#endif\n\n" >> entry.h

+	python3.8 fix.py
```

然后再重新运行 `make run` 即可
