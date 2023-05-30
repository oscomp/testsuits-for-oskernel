MUSL_LIB?=/opt/riscv64--musl--bleeding-edge-2020.08-1/riscv64-buildroot-linux-musl/sysroot/lib64/
PREFIX?=riscv64-buildroot-linux-musl-
CC=$(PREFIX)gcc
OBJCOPY=$(PREFIX)objcopy
# CC=musl-gcc
# OBJCOPY=objcopy

STATIC_EXES = $(shell cat static.txt)
STATIC_OBJS = $(STATIC_EXES:%.exe=%.obj)
STATIC_PREFIXED_OBJS = $(STATIC_EXES:%.exe=%_prefixed.obj)

DYNAMIC_EXES = $(shell cat dynamic.txt)
DYNAMIC_OBJS = $(DYNAMIC_EXES:%.exe=%.obj)
DYNAMIC_PREFIXED_OBJS = $(DYNAMIC_EXES:%.exe=%_prefixed.obj)

COMMON_SRC_FILES = fdfill.c memfill.c mtest.c path.c print.c rand.c setrlim.c utf8.c vmfill.c
COMMON_SRCS = $(addprefix src/common/,  $(COMMON_SRC_FILES))
COMMON_OBJS = $(COMMON_SRCS:%.c=%.obj)

DSO_SRCS = src/functional/tls_align_dso.c src/functional/tls_init_dso.c src/functional/dlopen_dso.c src/regression/tls_get_new-dtv_dso.c
DSO_SOS = $(DSO_SRCS:%.c=%.so)


CFLAGS += -pipe -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wno-unused-function -Wno-missing-braces -Wno-unused -Wno-overflow
CFLAGS += -Wno-unknown-pragmas -fno-builtin -frounding-math
CFLAGS += -Werror=implicit-function-declaration -Werror=implicit-int -Werror=pointer-sign -Werror=pointer-arith
CFLAGS += -Os -s
CFLAGS += -Isrc/common/
LDLIBS += -Os -s -lpthread -lm -lrt

# CFLAGS += -D_FILE_OFFSET_BITS=64
# LDLIBS += -lcrypt -ldl -lresolv -lutil -lpthread

all: static dynamic runtest

so: $(DSO_SOS)

entry.c: entry.h

entry.h: dynamic.txt static.txt
	printf "#ifdef STATIC\n" >> entry.h
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

runtest: src/common/runtest.obj $(COMMON_OBJS)
	$(CC) $(LDFLAGS) $^ -static -o runtest.exe

static: $(COMMON_OBJS) $(STATIC_PREFIXED_OBJS) entry.c src/functional/tls_align_dso.obj
	$(CC) $(LDFLAGS) -DSTATIC $^ -static -o entry-static.exe

dynamic: $(DYNAMIC_PREFIXED_OBJS) $(DSO_SOS) $(COMMON_OBJS) entry.c
	# $(CC) $(LDFLAGS) entry.c -DDYNAMIC $(COMMON_OBJS) $(DYNAMIC_PREFIXED_OBJS) -Lsrc/functional -Lsrc/regression -ltls_align_dso -ltls_init_dso -ldlopen_dso -ltls_get_new-dtv_dso -o entry-dynamic.exe
	$(CC) $(LDFLAGS) entry.c -DDYNAMIC $(COMMON_OBJS) $(DYNAMIC_PREFIXED_OBJS) -Lsrc/functional -Lsrc/regression  -o entry-dynamic.exe -rdynamic

common: $(COMMON_OBJS)

%_prefixed.obj: %.obj
	$(OBJCOPY) --redefine-sym main=$(subst -,_,$(notdir $(basename $^)))_main $^ $@

%.obj : %.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.so : %.c
	$(CC) $(CFLAGS) -shared -fPIC $^ -o $@ #$(dir $@)/lib$(notdir $@)

run-all.sh: static dynamic runtest
	cat static.txt | xargs -I AA basename AA .exe | sed 's/-/_/g' | xargs -I BB printf "./runtest.exe -w entry-static.exe %s\n" BB > run-static.sh
	cat dynamic.txt | xargs -I AA basename AA .exe | sed 's/-/_/g' | xargs -I BB printf "./runtest.exe -w entry-dynamic.exe %s\n" BB > run-dynamic.sh
	echo "./run-static.sh" > run-all.sh
	echo "./run-dynamic.sh" >> run-all.sh
	chmod +x run-*.sh

run: run-all.sh
	- ./run-all.sh

disk: run-all.sh
	mkdir -p disk
	cp entry-dynamic.exe disk
	cp entry-static.exe disk
	cp runtest.exe disk
	cp src/functional/*.so src/regression/*.so disk
	cp $(MUSL_LIB)/libc.so disk
	cp run-*.sh disk
	# ls -lh disk/
	# dd if=/dev/zero of=sdcard.img bs=1024K count=4
	# mkdir -p mnt
	# mkfs.vfat -F 32 sdcard.img
	# sudo mount sdcard.img mnt
	# sudo cp -r disk/* mnt/
	# sudo umount mnt
	# rm -rf mnt
	# gzip sdcard.img


clean:
	- find src -name '*.obj' | xargs -t rm
	- find src -name '*.exe' | xargs -t rm
	- find src -name '*.so' | xargs -t rm
	- rm *.exe
	- rm entry.h
	- rm -rf disk
	- rm run-*.sh
