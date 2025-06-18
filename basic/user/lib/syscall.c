#include <stddef.h>
#include <unistd.h>

#include "syscall.h"

int open(const char *path, int flags)
{
    return syscall(SYS_openat, AT_FDCWD, path, flags, O_RDWR);
}

int openat(int dirfd, const char *path, int flags)
{
    return syscall(SYS_openat, dirfd, path, flags, 0600);
}

int close(int fd)
{
    return syscall(SYS_close, fd);
}

ssize_t read(int fd, void *buf, size_t len)
{
    return syscall(SYS_read, fd, buf, len);
}

ssize_t write(int fd, const void *buf, size_t len)
{
    return syscall(SYS_write, fd, buf, len);
}

pid_t getpid(void)
{
    return syscall(SYS_getpid);
}

pid_t getppid(void)
{
    return syscall(SYS_getppid);
}

int sched_yield(void)
{
    return syscall(SYS_sched_yield);
}

pid_t fork(void)
{
    return syscall(SYS_clone, SIGCHLD, 0);
}

pid_t clone(int (*fn)(void *arg), void *arg, void *stack, size_t stack_size, unsigned long flags)
{
    if (stack)
        stack += stack_size;

    return __clone(fn, stack, flags, NULL, NULL, NULL);
    // return syscall(SYS_clone, fn, stack, flags, NULL, NULL, NULL);
}
void exit(int code)
{
    syscall(SYS_exit, code);
}

int waitpid(int pid, int *code, int options)
{
    return syscall(SYS_wait4, pid, code, options, 0);
}

int exec(char *name)
{
    return syscall(SYS_execve, name);
}

int execve(const char *name, char *const argv[], char *const argp[])
{
    return syscall(SYS_execve, name, argv, argp);
}

clock_t times(void *mytimes)
{
    return syscall(SYS_times, mytimes);
}

int64 get_time()
{
    TimeVal time;
    int err = sys_get_time(&time, 0);
    if (err == 0)
    {
        return ((time.sec & 0xffff) * 1000 + time.usec / 1000);
    }
    else
    {
        return -1;
    }
}

int sys_get_time(TimeVal *ts, int tz)
{
    return syscall(SYS_gettimeofday, ts, tz);
}

int sleep(unsigned long long time)
{
    TimeVal tv = {.sec = time, .usec = 0};
    if (syscall(SYS_nanosleep, &tv, &tv))
        return tv.sec;
    return 0;
}

int set_priority(int prio)
{
    return syscall(SYS_setpriority, prio);
}

void *mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
    return syscall(SYS_mmap, start, len, prot, flags, fd, off);
}

int munmap(void *start, size_t len)
{
    return syscall(SYS_munmap, start, len);
}

int wait(int *code)
{
    return waitpid((int)-1, code, 0);
}

int fstat(int fd, struct kstat *st)
{
#ifdef SYS_fstat
    return syscall(SYS_fstat, fd, st);
#else
// Based on https://github.com/kraj/musl/blob/kraj/master/src/stat/fstat.c#L10
#define AT_EMPTY_PATH 0x1000
#define makedev(x, y) (             \
    (((x) & 0xfffff000ULL) << 32) | \
    (((x) & 0x00000fffULL) << 8) |  \
    (((y) & 0xffffff00ULL) << 12) | \
    (((y) & 0x000000ffULL)))

    struct statx stx;
    int res = syscall(SYS_statx, fd, "", AT_EMPTY_PATH, 0x77, &stx);
    if (res < 0)
        return res;

    *st = (struct kstat){
        .st_dev = makedev(stx.stx_dev_major, stx.stx_dev_minor),
        .st_ino = stx.stx_ino,
        .st_mode = stx.stx_mode,
        .st_nlink = stx.stx_nlink,
        .st_uid = stx.stx_uid,
        .st_gid = stx.stx_gid,
        .st_rdev = makedev(stx.stx_rdev_major, stx.stx_rdev_minor),
        .st_size = stx.stx_size,
        .st_blksize = stx.stx_blksize,
        .st_blocks = stx.stx_blocks,
        .st_atime_sec = stx.stx_atime.tv_sec,
        .st_atime_nsec = stx.stx_atime.tv_nsec,
        .st_mtime_sec = stx.stx_mtime.tv_sec,
        .st_mtime_nsec = stx.stx_mtime.tv_nsec,
        .st_ctime_sec = stx.stx_ctime.tv_sec,
        .st_ctime_nsec = stx.stx_ctime.tv_nsec,
    };
    return res;
#endif
}

int sys_linkat(int olddirfd, char *oldpath, int newdirfd, char *newpath, unsigned int flags)
{
    return syscall(SYS_linkat, olddirfd, oldpath, newdirfd, newpath, flags);
}

int sys_unlinkat(int dirfd, char *path, unsigned int flags)
{
    return syscall(SYS_unlinkat, dirfd, path, flags);
}

int link(char *old_path, char *new_path)
{
    return sys_linkat(AT_FDCWD, old_path, AT_FDCWD, new_path, 0);
}

int unlink(char *path)
{
    return sys_unlinkat(AT_FDCWD, path, 0);
}

int uname(void *buf)
{
    return syscall(SYS_uname, buf);
}

void *brk(void *addr)
{
    return syscall(SYS_brk, addr);
}

char *getcwd(char *buf, size_t size)
{
    return syscall(SYS_getcwd, buf, size);
}

int chdir(const char *path)
{
    return syscall(SYS_chdir, path);
}

int mkdir(const char *path, mode_t mode)
{
    return syscall(SYS_mkdirat, AT_FDCWD, path, mode);
}

int getdents(int fd, struct linux_dirent64 *dirp64, unsigned long len)
{
    // return syscall(SYS_getdents64, fd, dirp64, len);
    return syscall(SYS_getdents64, fd, dirp64, len);
}

int pipe(int fd[2])
{
    return syscall(SYS_pipe2, fd, 0);
}

int dup(int fd)
{
    return syscall(SYS_dup, fd);
}

int dup2(int old, int new)
{
    return syscall(SYS_dup3, old, new, 0);
}

int mount(const char *special, const char *dir, const char *fstype, unsigned long flags, const void *data)
{
    return syscall(SYS_mount, special, dir, fstype, flags, data);
}

int umount(const char *special)
{
    return syscall(SYS_umount2, special, 0);
}
