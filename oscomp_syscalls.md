# 系统调用的说明以及调用方式
系统调用方式遵循RISC-V ABI,即调用号存放在a7寄存器中,6个参数分别储存在a0-a5寄存器
中,返回值保存在a0中。

主要参考了Linux 5.10 syscalls，详细请参见：https://man7.org/linux/man-pages/man2/syscalls.2.html
## 文件系统相关
### #define SYS_getcwd 17
功能：获取当前工作目录；

输入：

- char *buf：一块缓存区，用于保存当前工作目录的字符串。当buf设为NULL，由系统来分配缓存区。

- size：buf缓存区的大小。

返回值：成功执行，则返回当前工作目录的字符串的指针。失败，则返回NULL。

```c
char *buf, size_t size;
long ret = syscall(SYS_getcwd, buf, size);
```

### #define SYS_pipe2 59
功能：创建管道；

输入：

- fd[2]：用于保存2个文件描述符。其中，fd[0]为管道的读出端，fd[1]为管道的写入端。

返回值：成功执行，返回0。失败，返回-1。

```
int fd[2];
int ret = syscall(SYS_pipe2, fd, 0);
```

### #define SYS_dup 23
功能：复制文件描述符；

输入：

- fd：被复制的文件描述符。

返回值：成功执行，返回新的文件描述符。失败，返回-1。

```
int fd;
int ret = syscall(SYS_dup, fd);
```

### #define SYS_dup3 24
功能：复制文件描述符，并指定了新的文件描述符；

输入：

- old：被复制的文件描述符。
- new：新的文件描述符。

返回值：成功执行，返回新的文件描述符。失败，返回-1。

```
int old, int new;
int ret = syscall(SYS_dup3, old, new, 0);
```

### #define SYS_chdir 49
功能：切换工作目录；

输入：

- path：需要切换到的目录。

返回值：成功执行，返回0。失败，返回-1。

```
const char *path;
int ret = syscall(SYS_chdir, path);
```

### #define SYS_openat 56
功能：打开或创建一个文件；

输入：

- fd：文件所在目录的文件描述符。

- filename：要打开或创建的文件名。如为绝对路径，则忽略fd。如为相对路径，且fd是AT_FDCWD，则filename是相对于当前工作目录来说的。如为相对路径，且fd是一个文件描述符，则filename是相对于fd所指向的目录来说的。
- flags：必须包含如下访问模式的其中一种：O_RDONLY，O_WRONLY，O_RDWR。还可以包含文件创建标志和文件状态标志。
- mode：文件的所有权描述。详见`man 7 inode `。

返回值：成功执行，返回新的文件描述符。失败，返回-1。

```
int fd, const char *filename, int flags, mode_t mode;
int ret = syscall(SYS_openat, fd, filename, flags, mode);
```

### #define SYS_close 57
功能：关闭一个文件描述符；

输入：

- fd：要关闭的文件描述符。

返回值：成功执行，返回0。失败，返回-1。

```
int fd;
int ret = syscall(SYS_close, fd);
```

### #define SYS_getdents64 61
功能：获取目录的条目;

输入：

- fd：所要读取目录的文件描述符。

- buf：一个缓存区，用于保存所读取目录的信息。缓存区的结构如下：

  ```c
  struct dirent {
      uint64 d_ino;	// 索引结点号
      int64 d_off;	// 到下一个dirent的偏移
      unsigned short d_reclen;	// 当前dirent的长度
      unsigned char d_type;	// 文件类型
      char d_name[];	//文件名
  };
  ```

- len：buf的大小。

返回值：成功执行，返回读取的字节数。当到目录结尾，则返回0。失败，则返回-1。

```
int fd, struct dirent *buf, size_t len
int ret = syscall(SYS_getdents64, fd, buf, len);
```

### #define SYS_read 63
功能：从一个文件描述符中读取；

输入：

- fd：要读取文件的文件描述符。
- buf：一个缓存区，用于存放读取的内容。
- count：要读取的字节数。

返回值：成功执行，返回读取的字节数。如为0，表示文件结束。错误，则返回-1。

```
int fd, void *buf, size_t count;
ssize_t ret = syscall(SYS_read, fd, buf, count);
```

### #define SYS_write 64
功能：从一个文件描述符中写入；

输入：

- fd：要写入文件的文件描述符。
- buf：一个缓存区，用于存放要写入的内容。
- count：要写入的字节数。

返回值：成功执行，返回写入的字节数。错误，则返回-1。

```
int fd, const void *buf, size_t count;
ssize_t ret = syscall(SYS_write, fd, buf, count);
```

### #define SYS_linkat 37
功能：创建文件的链接；

输入：

- olddirfd：原来的文件所在目录的文件描述符。
- oldpath：文件原来的名字。如果oldpath是相对路径，则它是相对于olddirfd目录而言的。如果oldpath是相对路径，且olddirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果oldpath是绝对路径，则olddirfd被忽略。
- newdirfd：新文件名所在的目录。
- newpath：文件的新名字。newpath的使用规则同oldpath。
- flags：在2.6.18内核之前，应置为0。其它的值详见`man 2 linkat`。

返回值：成功执行，返回0。失败，返回-1。

```
int olddirfd, char *oldpath, int newdirfd, char *newpath, unsigned int flags
int ret = syscall(SYS_linkat, olddirfd, oldpath, newdirfd, newpath, flags);
```

### #define SYS_unlinkat 35
功能：移除指定文件的链接(可用于删除文件)；

输入：

- dirfd：要删除的链接所在的目录。
- path：要删除的链接的名字。如果path是相对路径，则它是相对于dirfd目录而言的。如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果path是绝对路径，则dirfd被忽略。
- flags：可设置为0或AT_REMOVEDIR。

返回值：成功执行，返回0。失败，返回-1。

```
int dirfd, char *path, unsigned int flags;
syscall(SYS_unlinkat, dirfd, path, flags);
```

### #define SYS_mkdirat 34
功能：创建目录；

输入：

- dirfd：要创建的目录所在的目录的文件描述符。
- path：要创建的目录的名称。如果path是相对路径，则它是相对于dirfd目录而言的。如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果path是绝对路径，则dirfd被忽略。
- mode：文件的所有权描述。详见`man 7 inode `。

返回值：成功执行，返回0。失败，返回-1。

```
int dirfd, const char *path, mode_t mode;
int ret = syscall(SYS_mkdirat, dirfd, path, mode);
```

### #define SYS_umount2 39
* 功能：卸载文件系统；
* 输入：指定卸载设备，卸载参数；
* 返回值：成功返回0，失败返回-1；
```
const char *special, int flags;
int ret = syscall(SYS_umount2, special, flags);
```

### #define SYS_mount 40
* 功能：挂载文件系统；
* 输入：
    - special: 挂载设备；
    - dir: 挂载点；
    - fstype: 挂载的文件系统类型；
    - flags: 挂载参数；
    - data: 传递给文件系统的字符串参数，可为NULL；
* 返回值：成功返回0，失败返回-1；
```
const char *special, const char *dir, const char *fstype, unsigned long flags, const void *data;
int ret = syscall(SYS_mount, special, dir, fstype, flags, data);
```

### #define SYS_fstat 80
* 功能：获取文件状态；
* 输入：
    - fd: 文件句柄；
    - kst: 接收保存文件状态的指针；
```
struct kstat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	unsigned long __pad;
	off_t st_size;
	blksize_t st_blksize;
	int __pad2;
	blkcnt_t st_blocks;
	long st_atime_sec;
	long st_atime_nsec;
	long st_mtime_sec;
	long st_mtime_nsec;
	long st_ctime_sec;
	long st_ctime_nsec;
	unsigned __unused[2];
};
```
* 返回值：成功返回0，失败返回-1；
```
int fd;
struct kstat kst;
int ret = syscall(SYS_fstat, fd, &kst);
```

## 进程管理相关

### #define SYS_clone 220
* 功能：创建一个子进程；
* 输入：
    - flags: 创建的标志，如SIGCHLD；
    - stack: 指定新进程的栈，可为0；
    - ptid: 父线程ID；
    - tls: TLS线程本地存储描述符；
    - ctid: 子线程ID；
* 返回值：成功则返回子进程的线程ID，失败返回-1；
```
pid_t ret = syscall(SYS_clone, flags, stack, ptid, tls, ctid)
```

### #define SYS_execve 59
* 功能：执行一个指定的程序；
* 输入：
    - path: 待执行程序路径名称，
    - argv: 程序的参数， 
    - envp: 环境变量的数组指针
* 返回值：成功不返回，失败返回-1；
```
const char *path, char *const argv[], char *const envp[];
int ret = syscall(SYS_execve, path, argv, envp);
```

### #define SYS_wait4 260
* 功能：等待进程改变状态;
* 输入：
    - pid: 指定进程ID，可为-1等待任何子进程；
    - status: 接收状态的指针；
    - options: 选项：WNOHANG，WUNTRACED，WCONTINUED；
* 返回值：成功则返回进程ID；如果指定了WNOHANG，且进程还未改变状态，直接返回0；失败则返回-1；
```
pid_t pid, int *status, int options;
pid_t ret = syscall(SYS_wait4, pid, status, options);
```

### #define SYS_exit 93
* 功能：触发进程终止，无返回值；
* 输入：终止状态值；
* 返回值：无返回值；
```
int ec;
syscall(SYS_exit, ec);
```

### #define SYS_getppid 173
* 功能：获取父进程ID；
* 输入：系统调用ID；
* 返回值：成功返回父进程ID；
```
pid_t ret = syscall(SYS_getppid);
```

### #define SYS_getpid 172
* 功能：获取进程ID；
* 输入：系统调用ID；
* 返回值：成功返回进程ID；
```
pid_t ret = syscall(SYS_getpid);
```

## 内存管理相关
### #define SYS_brk 214
* 功能：修改数据段的大小；
* 输入：指定待修改的地址；
* 返回值：成功返回0，失败返回-1;
```
uintptr_t brk;
uintptr_t ret = syscall(SYS_brk, brk);
```

### #define SYS_munmap 215
* 功能：将文件或设备取消映射到内存中；
* 输入：映射的指定地址及区间；
* 返回值：成功返回0，失败返回-1;
```
void *start, size_t len
int ret = syscall(SYS_munmap, start, len);
```

### #define SYS_mmap 222
* 功能：将文件或设备映射到内存中；
* 输入：
    - start: 映射起始位置，
    - len: 长度，
    - prot: 映射的内存保护方式，可取：PROT_EXEC, PROT_READ, PROT_WRITE, PROT_NONE
    - flags: 映射是否与其他进程共享的标志，
    - fd: 文件句柄，
    - off: 文件偏移量；
* 返回值：成功返回已映射区域的指针，失败返回-1;
```
void *start, size_t len, int prot, int flags, int fd, off_t off
long ret = syscall(SYS_mmap, start, len, prot, flags, fd, off);
```

## 其他

### #define SYS_times 153
* 功能：获取进程时间；
* 输入：tms结构体指针，用于获取保存当前进程的运行时间数据；
* 返回值：成功返回已经过去的滴答数，失败返回-1;
```
struct tms *tms;
clock_t ret = syscall(SYS_times, tms);
```

### #define SYS_uname 160
* 功能：打印系统信息；
* 输入：utsname结构体指针用于获得系统信息数据；
* 返回值：成功返回0，失败返回-1;
```
struct utsname *uts;
int ret = syscall(SYS_uname, uts);
```
### #define SYS_sched_yield 124
* 功能：让出调度器；
* 输入：系统调用ID；
* 返回值：成功返回0，失败返回-1;
```
int ret = syscall(SYS_sched_yield);
```

### #define SYS_gettimeofday 169
* 功能：获取时间；
* 输入： timespec结构体指针用于获得时间值；
* 返回值：成功返回0，失败返回-1;
```
struct timespec *ts;
int ret = syscall(SYS_gettimeofday, ts, 0);
```
### #define SYS_nanosleep 101
* 功能：执行线程睡眠，sleep()库函数基于此系统调用；
* 输入：睡眠的时间间隔；
```
struct timespec {
	time_t tv_sec;        /* 秒 */
	long   tv_nsec;       /* 纳秒, 范围在0~999999999 */
};
```
* 返回值：成功返回0，失败返回-1;
```
const struct timespec *req, struct timespec *rem;
int ret = syscall(SYS_nanosleep, req, rem);
```


## 调用
```
static inline _u64 internal_syscall(long n, _u64 _a0, _u64 _a1, _u64 _a2, _u64
		_a3, _u64 _a4, _u64 _a5) {
	register _u64 a0 asm("a0") = _a0;
	register _u64 a1 asm("a1") = _a1;
	register _u64 a2 asm("a2") = _a2;
	register _u64 a3 asm("a3") = _a3;
	register _u64 a4 asm("a4") = _a4;
	register _u64 a5 asm("a5") = _a5;
	register long syscall_id asm("a7") = n;
	asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"
			(a5), "r"(syscall_id));
	return a0;
}
```
