#ifndef COSMOPOLITAN_LIBC_CALLS_SYSCALLS_H_
#define COSMOPOLITAN_LIBC_CALLS_SYSCALLS_H_

#define _POSIX_VERSION  200809L
#define _POSIX2_VERSION _POSIX_VERSION
#define _XOPEN_VERSION  700

#define EOF      -1         /* end of file */
#define WEOF     -1u        /* end of file (multibyte) */
#define _IOFBF   0          /* fully buffered */
#define _IOLBF   1          /* line buffered */
#define _IONBF   2          /* no buffering */
#define SEEK_SET 0          /* relative to beginning */
#define SEEK_CUR 1          /* relative to current position */
#define SEEK_END 2          /* relative to end */
#define __WALL   0x40000000 /* Wait on all children, regardless of type */
#define __WCLONE 0x80000000 /* Wait only on non-SIGCHLD children */

#define SIG_ERR ((void (*)(int))(-1))
#define SIG_DFL ((void *)0)
#define SIG_IGN ((void *)1)

#define MAP_FAILED ((void *)-1)

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)

#define WCOREDUMP(s)    (0x80 & (s))
#define WEXITSTATUS(s)  ((0xff00 & (s)) >> 8)
#define WIFCONTINUED(s) ((s) == 0xffff)
#define WIFEXITED(s)    (!WTERMSIG(s))
#define WIFSIGNALED(s)  ((0xffff & (s)) - 1u < 0xffu)
#define WIFSTOPPED(s)   ((short)(((0xffff & (s)) * 0x10001) >> 8) > 0x7f00)
#define WSTOPSIG(s)     WEXITSTATUS(s)
#define WTERMSIG(s)     (127 & (s))
#define W_STOPCODE(s)   ((s) << 8 | 0177)

#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_
/*───────────────────────────────────────────────────────────────────────────│─╗
│ cosmopolitan § system calls                                              ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

typedef int sig_atomic_t;

bool fileexists(const char *);
bool isdirectory(const char *);
bool isexecutable(const char *);
bool isregularfile(const char *);
bool issymlink(const char *);
bool32 isatty(int) nosideeffect;
bool32 ischardev(int) nosideeffect;
char *commandv(const char *, char *, size_t);
char *get_current_dir_name(void) dontdiscard;
char *getcwd(char *, size_t);
char *realpath(const char *, char *);
char *replaceuser(const char *) dontdiscard;
char *ttyname(int);
int access(const char *, int) dontthrow;
int arch_prctl();
int chdir(const char *);
int chmod(const char *, uint32_t);
int chown(const char *, uint32_t, uint32_t);
int chroot(const char *);
int clone(void *, void *, size_t, int, void *, int *, void *, int *);
int close(int);
int close_range(unsigned, unsigned, unsigned);
int closefrom(int);
int creat(const char *, uint32_t);
int dup(int);
int dup2(int, int);
int dup3(int, int, int);
int eaccess(const char *, int);
int euidaccess(const char *, int);
int execl(const char *, const char *, ...) nullterminated();
int execle(const char *, const char *, ...) nullterminated((1));
int execlp(const char *, const char *, ...) nullterminated();
int execv(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execvp(const char *, char *const[]);
int execvpe(const char *, char *const[], char *const[]);
int faccessat(int, const char *, int, uint32_t);
int fadvise(int, uint64_t, uint64_t, int);
int fchdir(int);
int fchmod(int, uint32_t) dontthrow;
int fchmodat(int, const char *, uint32_t, int);
int fchown(int, uint32_t, uint32_t);
int fchownat(int, const char *, uint32_t, uint32_t, int);
int fcntl(int, int, ...);
int fdatasync(int);
int filecmp(const char *, const char *);
int flock(int, int);
int fork(void);
int fsync(int);
int ftruncate(int, int64_t);
int getdents(unsigned, void *, unsigned, long *);
int getdomainname(char *, size_t);
int getegid(void) nosideeffect;
int geteuid(void) nosideeffect;
int getgid(void) nosideeffect;
int gethostname(char *, size_t);
int getloadavg(double *, int);
int getpgid(int) libcesque;
int getpgrp(void) nosideeffect;
int getpid(void) nosideeffect libcesque;
int getppid(void);
int getpriority(int, unsigned);
int getresgid(uint32_t *, uint32_t *, uint32_t *);
int getresuid(uint32_t *, uint32_t *, uint32_t *);
int getsid(int) nosideeffect libcesque;
int gettid(void) libcesque;
int getuid(void) libcesque;
int ioprio_get(int, int);
int ioprio_set(int, int, int);
int issetugid(void);
int kill(int, int);
int killpg(int, int);
int link(const char *, const char *) dontthrow;
int linkat(int, const char *, int, const char *, int);
int madvise(void *, uint64_t, int);
int mincore(void *, size_t, unsigned char *);
int mkdir(const char *, uint32_t);
int mkdirat(int, const char *, uint32_t);
int mkfifo(const char *, uint32_t);
int mkfifoat(int, const char *, uint32_t);
int mknod(const char *, uint32_t, uint64_t);
int mknodat(int, const char *, int32_t, uint64_t);
int mlock(const void *, size_t);
int mlock2(const void *, size_t, int);
int mlockall(int);
int munlock(const void *, size_t);
int munlockall(void);
int nice(int);
int open(const char *, int, ...);
int openat(int, const char *, int, ...);
int pause(void);
int personality(uint64_t);
int pipe(int[hasatleast 2]);
int pipe2(int[hasatleast 2], int);
int pivot_root(const char *, const char *);
int pledge(const char *, const char *);
int posix_fadvise(int, uint64_t, uint64_t, int);
int posix_madvise(void *, uint64_t, int);
int prctl(int, ...);
int raise(int);
int reboot(int);
int remove(const char *);
int rename(const char *, const char *);
int renameat(int, const char *, int, const char *);
int renameat2(long, const char *, long, const char *, int);
int rmdir(const char *);
int sched_getaffinity(int, uint64_t, void *);
int sched_setaffinity(int, uint64_t, const void *);
int sched_yield(void);
int seccomp(unsigned, unsigned, void *);
int setegid(uint32_t);
int seteuid(uint32_t);
int setfsgid(int);
int setfsuid(int);
int setgid(int);
int setpgid(int, int);
int setpgrp(void);
int setpriority(int, unsigned, int);
int setregid(uint32_t, uint32_t);
int setresgid(uint32_t, uint32_t, uint32_t);
int setresuid(uint32_t, uint32_t, uint32_t);
int setreuid(uint32_t, uint32_t);
int setsid(void);
int setuid(int);
int sigignore(int);
int siginterrupt(int, int);
int symlink(const char *, const char *);
int symlinkat(const char *, int, const char *);
int sync_file_range(int, int64_t, int64_t, unsigned);
int sysctl(const int *, unsigned, void *, size_t *, void *, size_t);
int tgkill(int, int, int);
int tkill(int, int);
int touch(const char *, uint32_t);
int truncate(const char *, uint64_t);
int ttyname_r(int, char *, size_t);
int umask(int);
int unlink(const char *);
int unlink_s(const char **);
int unlinkat(int, const char *, int);
int unveil(const char *, const char *);
int vfork(void) returnstwice;
int wait(int *);
int waitpid(int, int *, int);
intptr_t syscall(int, ...);
long ptrace(int, ...);
size_t GetFileSize(const char *);
ssize_t copy_file_range(int, long *, int, long *, size_t, uint32_t);
ssize_t copyfd(int, int64_t *, int, int64_t *, size_t, uint32_t);
ssize_t getfiledescriptorsize(int);
ssize_t lseek(int, int64_t, unsigned);
ssize_t pread(int, void *, size_t, int64_t);
ssize_t pwrite(int, const void *, size_t, int64_t);
ssize_t read(int, void *, size_t);
ssize_t readansi(int, char *, size_t);
ssize_t readlink(const char *, char *, size_t);
ssize_t readlinkat(int, const char *, char *, size_t);
ssize_t splice(int, int64_t *, int, int64_t *, size_t, uint32_t);
ssize_t write(int, const void *, size_t);
void sync(void);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_CALLS_SYSCALLS_H_ */
