/*
** posix.h
**
** This file should be processed with one of the standard libraries
** (standard.h or strict.h) to produce posix.lcd or posixstrict.lcd.
*/

/*@-nextlinemacros@*/
/*@+allimponly@*/
/*@+globsimpmodifiesnothing@*/

/*
 * LCLint ISO C + POSIX Library
 *
 * $Id: posix.h,v 1.2 2000/06/21 18:54:59 drl7x Exp $
 */

/*
 * In 1988, IEEE Std 1003.1-1988, commonly known as "POSIX" or the
 * "IEEE Portable Operating System Interface for Computing Environments"
 * was published as an American National Standard. In 1990, IEEE Std
 * 1003.1-1990 was published as an International Standard. The two
 * standards differ slightly, and where they do, the 1990 International
 * standard was used for this lclint library. The differences are:
 *
 *  1988: cuserid()
 *  1990: -removed- (but still in this lclint library)
 *
 *  1988:     int read (int, void*, unsigned int)
 *  1990: ssize_t read (int, void*, size_t)
 *
 *  1988:     int write (int, const void*, unsigned int)
 *  1990: ssize_t write (int, const void*, size_t)
 *
 * The other differences are in the semantics of functions.
 */

/*
 * The reference for the ISO C part of this library was
 * Plauger, Brodie's "Standard C - A Reference", Prentice Hall.
 * The reference for the POSIX part of this library was
 * Donald Lewine's "POSIX Programmer's Guide", O'Reilly.
 * Transcription by Jens Schweikhardt <schweikhardt@rus.uni-stuttgart.de>
 */

/*
 * Note that Amendment 1 to ISO C was published in 1995 after POSIX was out.
 * Amendment 1 basically adds support for wide characters and iso 646
 * source character sets. In particular, there are three new headers:
 * <iso646.h>, <wchar.h>, and <wctype.h>
 */

/*
 * Each header has annotations in this order:
 *
 *    1) type definitions (if any)
 *    2) constant definitions (if any)
 *    3) structure definitions (if any)
 *    4) function prototypes and externals (if any)
 *
 *    5) type definitions augmented by POSIX (if any)
 *    6) constant definitions augmented by POSIX (if any)
 *    7) structure definitions augmented by POSIX (if any)
 *    8) function prototypes and externals augmented by POSIX (if any)
 *
 * Builtins are mentioned in the header where they appear according to ISO.
 */

/*
** sys/types.h
*/

typedef /*@integraltype@*/ dev_t;
typedef /*@integraltype@*/ gid_t;
typedef /*@integraltype@*/ ino_t;
typedef /*@integraltype@*/ mode_t;
typedef /*@integraltype@*/ nlink_t;
typedef /*@integraltype@*/ off_t;
typedef /*@integraltype@*/ pid_t;
typedef /*@integraltype@*/ uid_t;

/*
** dirent.h
*/

typedef /*@abstract@*/ /*@mutable@*/ void *DIR;

struct dirent {
  char	d_name[];
};

	extern int
closedir (DIR *dirp)
	/*@modifies errno@*/;

	extern /*@null@*/ DIR *
opendir (const char *dirname)
	/*@modifies errno@*/;

	extern /*@null@*/ struct dirent *
readdir (DIR *dirp)
	/*@modifies errno@*/;

	extern void
rewinddir (DIR *dirp)
	/*@*/;

/*
** errno.h
*/

/*@constant int E2BIG@*/
/*@constant int EACCES@*/
/*@constant int EAGAIN@*/
/*@constant int EBADF@*/
/*@constant int EBUSY@*/
/*@constant int ECHILD@*/
/*@constant int EDEADLK@*/
/*@constant int EEXIST@*/
/*@constant int EFAULT@*/
/*@constant int EFBIG@*/
/*@constant int EINTR@*/
/*@constant int EINVAL@*/
/*@constant int EIO@*/
/*@constant int EISDIR@*/
/*@constant int EMFILE@*/
/*@constant int EMLINK@*/
/*@constant int ENAMETOOLONG@*/
/*@constant int ENFILE@*/
/*@constant int ENODEV@*/
/*@constant int ENOENT@*/
/*@constant int ENOEXEC@*/
/*@constant int ENOLCK@*/
/*@constant int ENOMEM@*/
/*@constant int ENOSPC@*/
/*@constant int ENOSYS@*/
/*@constant int ENOTDIR@*/
/*@constant int ENOTEMPTY@*/
/*@constant int ENOTTY@*/
/*@constant int ENXIO@*/
/*@constant int EPERM@*/
/*@constant int EPIPE@*/
/*@constant int EROFS@*/
/*@constant int ESPIPE@*/
/*@constant int ESRCH@*/
/*@constant int EXDEV@*/

/*
** fcntl.h
*/

/*@constant int FD_CLOEXEC@*/
/*@constant int F_DUPFD@*/
/*@constant int F_GETFD@*/
/*@constant int F_GETFL@*/
/*@constant int F_GETLK@*/
/*@constant int F_RDLCK@*/
/*@constant int F_SETFD@*/
/*@constant int F_SETFL@*/
/*@constant int F_SETLK@*/
/*@constant int F_SETLKW@*/
/*@constant int F_UNLCK@*/
/*@constant int F_WRLCK@*/
/*@constant int O_ACCMODE@*/
/*@constant int O_APPEND@*/
/*@constant int O_CREAT@*/
/*@constant int O_EXCL@*/
/*@constant int O_NOCTTY@*/
/*@constant int O_NONBLOCK@*/
/*@constant int O_RDONLY@*/
/*@constant int O_RDWR@*/
/*@constant int O_TRUNC@*/
/*@constant int O_WRONLY@*/
/*@constant int SEEK_CUR@*/
/*@constant int SEEK_END@*/
/*@constant int SEEK_SET@*/
/*@constant int S_IRGRP@*/
/*@constant int S_IROTH@*/
/*@constant int S_IUSR@*/
/*@constant int S_IWXG@*/
/*@constant int S_IWXO@*/
/*@constant int S_IWXU@*/
/*@constant int S_ISGID@*/
/*@constant int S_ISUID@*/
/*@constant int S_IWGRP@*/
/*@constant int S_IWOTH@*/
/*@constant int S_IWUSR@*/
/*@constant int S_IXGRP@*/
/*@constant int S_IXOTH@*/
/*@constant int S_IXUSR@*/

struct flock {
  short l_type;
  short l_whence;
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
};

	extern int
creat (const char *path, mode_t mode)
	/*@modifies errno@*/;

	extern int
fcntl (int fd, int cmd, ...)
	/*@modifies errno@*/;

	extern int
open (const char *path, int oflag, ...)
	/*@modifies errno@*/;

/*
** grp.h
*/

struct group {
  char *gr_name;
  gid_t gr_gid;
  char **gr_mem;
};

	extern /*@null@*/ struct group *
getgrgid (gid_t gid)
	/*@modifies errno@*/;

	extern /*@null@*/ struct group *
getgrnam (const char *nm)
	/*@modifies errno@*/;

/*
** limits.h
*/

/*@constant long ARG_MAX@*/
/*@constant long CHILD_MAX@*/
/*@constant long LINK_MAX@*/
/*@constant long MAX_CANON@*/
/*@constant long MAX_INPUT@*/
/*@constant long NAME_MAX@*/
/*@constant long NGROUPS_MAX@*/
/*@constant long OPEN_MAX@*/
/*@constant long PIPE_BUF@*/
/*@constant long SSIZE_MAX@*/
/*@constant long STREAM_MAX@*/
/*@constant long TZNAME_MAX@*/
/*@constant long _POSIX_ARG_MAX@*/
/*@constant long _POSIX_CHILD_MAX@*/
/*@constant long _POSIX_LINK_MAX@*/
/*@constant long _POSIX_MAX_CANON@*/
/*@constant long _POSIX_MAX_INPUT@*/
/*@constant long _POSIX_NAME_MAX@*/
/*@constant long _POSIX_NGROUPS_MAX@*/
/*@constant long _POSIX_OPEN_MAX@*/
/*@constant long _POSIX_PATH_MAX@*/
/*@constant long _POSIX_PIPE_BUF@*/
/*@constant long _POSIX_SSIZE@*/
/*@constant long _POSIX_STREAM@*/
/*@constant long _POSIX_TZNAME_MAX@*/

/*
** pwd.h
*/

struct passwd {
  char *pw_name;
  uid_t pw_uid;
  gid_t pw_gid;
  char *pw_dir;
  char *pw_shell;
} ;

	extern /*@observer@*/ /*@null@*/ struct passwd *
getpwnam (const char *)
	/*@modifies errno@*/;

	extern /*@observer@*/ /*@null@*/ struct passwd *
getpwuid (uid_t uid)
	/*@modifies errno@*/;

/*
** setjmp.h
*/

typedef /*@abstract@*/ /*@mutable@*/ void *sigjmp_buf;

	extern /*@mayexit@*/ void
siglongjmp (sigjmp_buf env, int val)
	/*@*/;

	extern int
sigsetjmp (/*@out@*/ sigjmp_buf env, int savemask)
	/*@modifies env@*/;

/*
** signal.h
*/

typedef /*@abstract@*/ sigset_t;

/*@constant int SA_NOCLDSTOP@*/
/*@constant int SIG_BLOCK@*/
/*@constant int SIG_SETMASK@*/
/*@constant int SIG_UNBLOCK@*/
/*@constant int SIGALRM@*/
/*@constant int SIGCHLD@*/
/*@constant int SIGCONT@*/
/*@constant int SIGHUP@*/
/*@constant int SIGKILL@*/
/*@constant int SIGPIPE@*/
/*@constant int SIGQUIT@*/
/*@constant int SIGSTOP@*/
/*@constant int SIGTSTP@*/
/*@constant int SIGTTIN@*/
/*@constant int SIGTTOU@*/
/*@constant int SIGUSR1@*/
/*@constant int SIGUSR2@*/

struct sigaction {
  void (*sa_handler)();
  sigset_t sa_mask;
  int sa_flags;
} ;
 
	extern /*@mayexit@*/ int
kill (pid_t pid, int sig)
	/*@modifies errno@*/;

	extern int
sigaction (int sig, const struct sigaction *act, /*@out@*/ /*@null@*/ struct sigaction *oact)
	/*@modifies *oact, errno, internalState@*/;

	extern int
sigaddset (sigset_t *set, int signo)
	/*@modifies *set, errno@*/;

	extern int
sigdelset (sigset_t *set, int signo)
	/*@modifies *set, errno@*/;

	extern int
sigemptyset (/*@out@*/ sigset_t *set)
	/*@modifies *set, errno@*/;

	extern int
sigfillset (/*@out@*/ sigset_t *set)
	/*@modifies *set, errno@*/;

	extern int
sigismember (const sigset_t *set, int signo)
	/*@modifies errno@*/;

	extern int
sigpending (/*@out@*/ sigset_t *set)
	/*@modifies *set, errno@*/;

	extern int
sigprocmask (int how, /*@null@*/ const sigset_t *set, /*@null@*/ /*@out@*/ sigset_t *oset)
	/*@modifies *oset, errno, internalState@*/;

	extern int
sigsuspend (const sigset_t *sigmask)
	/*@modifies errno, internalState@*/;

/*
** stdio.h
*/

/*@constant int L_ctermid@*/
/*@constant int L_cuserid@*/
/*@constant int STREAM_MAX@*/

	extern /*@null@*/ /*@dependent@*/ FILE *
fdopen (int fd, const char *type)
	/*@modifies errno, fileSystem@*/;

	extern int
fileno (FILE *fp)
	/*@modifies errno@*/;

/*
** sys/stat.h
*/

/*@constant int S_IRGRP@*/
/*@constant int S_IROTH@*/
/*@constant int S_IUSR@*/
/*@constant int S_IWXG@*/
/*@constant int S_IWXO@*/
/*@constant int S_IWXU@*/
/*@constant int S_ISGID@*/
/*@constant int S_ISUID@*/
/*@constant int S_IWGRP@*/
/*@constant int S_IWOTH@*/
/*@constant int S_IWUSR@*/
/*@constant int S_IXGRP@*/
/*@constant int S_IXOTH@*/
/*@constant int S_IXUSR@*/

struct stat {
  mode_t	st_mode;
  ino_t	st_ino;
  dev_t	st_dev;
  nlink_t	st_nlink;
  uid_t	st_uid;
  gid_t	st_gid;
  off_t	st_size;
  time_t	st_st_atime;
  time_t	st_st_mtime;
  time_t	st_st_ctime;
} ;

/*
** POSIX does not require that the S_I* be functions. They're
** macros virtually everywhere. 
*/

# ifdef STRICT
/*@notfunction@*/
# define SBOOLINT lltX_bool /*@alt int@*/
# else
/*@notfunction@*/
# define SBOOLINT lltX_bool           
# endif

extern SBOOLINT S_ISBLK (/*@sef@*/ mode_t m) /*@*/ ;

extern SBOOLINT S_ISCHR (/*@sef@*/ mode_t m) /*@*/ ;

extern SBOOLINT S_ISDIR (/*@sef@*/ mode_t m) /*@*/ ;

extern SBOOLINT S_ISFIFO (/*@sef@*/ mode_t m) /*@*/ ;

extern SBOOLINT S_ISREG (/*@sef@*/ mode_t m) /*@*/ ;

extern int chmod (const char *path, mode_t mode)
   /*@modifies fileSystem, errno@*/ ;
   
extern int fstat (int fd, /*@out@*/ struct stat *buf)
     /*@modifies errno, *buf@*/ ;
     
extern int
mkdir (const char *path, mode_t mode)
	/*@modifies fileSystem, errno@*/;

	extern int
mkfifo (const char *path, mode_t mode)
	/*@modifies fileSystem, errno@*/;

	extern int
stat (const char *path, /*@out@*/ struct stat *buf)
	/*@modifies errno, *buf@*/;

	extern int
umask (mode_t cmask)
	/*@modifies internalState@*/;

/*
** sys/times.h
*/

struct tms {
  clock_t	tms_utime;
  clock_t	tms_stime;
  clock_t	tms_cutime;
  clock_t	tms_cstime;
};

	extern clock_t
times (/*@out@*/ struct tms *tp)
	/*@modifies *tp@*/;

/*
** sys/utsname.h
*/

struct utsname {
  char	sysname[];
  char	nodename[];
  char	release[];
  char	version[];
  char	machine[];
};

	extern int
uname (/*@out@*/ struct utsname *name)
     /*@modifies *name, errno@*/ ;

/*
** sys/wait.h
*/

extern int WEXITSTATUS (int status) /*@*/ ;
extern int WIFEXITED (int status) /*@*/ ;
extern int WIFSIGNALED (int status) /*@*/ ;
extern int WIFSTOPPED (int status) /*@*/ ;
extern int WSTOPSIG (int status) /*@*/ ;
extern int WTERMSIG (int status) /*@*/ ;

/*@constant int WUNTRACED@*/

	extern pid_t
wait (/*@out@*/ /*@null@*/ int *st)
	/*@modifies *st, errno@*/;

	extern pid_t
waitpid (pid_t pid, /*@out@*/ int *st, int opt)
	/*@modifies *st, errno@*/;

/*
** termios.h
*/

typedef unsigned char /*@alt unsigned short@*/ cc_t;
typedef unsigned long /*@alt long@*/ speed_t;
typedef unsigned long /*@alt long@*/ tcflag_t;

/*@constant int B0@*/
/*@constant int B50@*/
/*@constant int B75@*/
/*@constant int B110@*/
/*@constant int B134@*/
/*@constant int B150@*/
/*@constant int B200@*/
/*@constant int B300@*/
/*@constant int B600@*/
/*@constant int B1200@*/
/*@constant int B1800@*/
/*@constant int B2400@*/
/*@constant int B4800@*/
/*@constant int B9600@*/
/*@constant int B19200@*/
/*@constant int B38400@*/
/*@constant int BRKINT@*/
/*@constant int CLOCAL@*/
/*@constant int CREAD@*/
/*@constant int CS5@*/
/*@constant int CS6@*/
/*@constant int CS7@*/
/*@constant int CS8@*/
/*@constant int CSIZE@*/
/*@constant int CSTOPB@*/
/*@constant int ECHO@*/
/*@constant int ECHOE@*/
/*@constant int ECHOK@*/
/*@constant int ECHONL@*/
/*@constant int HUPCL@*/
/*@constant int ICANON@*/
/*@constant int ICRNL@*/
/*@constant int IEXTEN@*/
/*@constant int IGNBRK@*/
/*@constant int IGNCR@*/
/*@constant int IGNPAR@*/
/*@constant int IGNLCR@*/
/*@constant int INPCK@*/
/*@constant int ISIG@*/
/*@constant int ISTRIP@*/
/*@constant int IXOFF@*/
/*@constant int IXON@*/
/*@constant int NCCS@*/
/*@constant int NOFLSH@*/
/*@constant int OPOST@*/
/*@constant int PARENB@*/
/*@constant int PARMRK@*/
/*@constant int PARODD@*/
/*@constant int TCIFLUSH@*/
/*@constant int TCIOFF@*/
/*@constant int TCIOFLUSH@*/
/*@constant int TCION@*/
/*@constant int TCOFLUSH@*/
/*@constant int TCSADRAIN@*/
/*@constant int TCSAFLUSH@*/
/*@constant int TCSANOW@*/
/*@constant int TOSTOP@*/
/*@constant int VEOF@*/
/*@constant int VEOL@*/
/*@constant int VERASE@*/
/*@constant int VINTR@*/
/*@constant int VKILL@*/
/*@constant int VMIN@*/
/*@constant int VQUIT@*/
/*@constant int VSTART@*/
/*@constant int VSTOP@*/
/*@constant int VSUSP@*/
/*@constant int VTIME@*/

struct termios {
  tcflag_t	c_iflag;
  tcflag_t	c_oflag;
  tcflag_t	c_cflag;
  tcflag_t	c_lflag;
  cc_t		c_cc;
} ;

	extern speed_t
cfgetispeed (const struct termios *p)
	/*@*/;

	extern speed_t
cfgetospeed (const struct termios *p)
	/*@*/;

	extern int
cfsetispeed (struct termios *p)
	/*@modifies *p@*/;

	extern int
cfsetospeed (struct termios *p)
	/*@modifies *p@*/;

	extern int
tcdrain (int fd)
	/*@modifies errno@*/;

	extern int
tcflow (int fd, int action)
	/*@modifies errno@*/;

	extern int
tcflush (int fd, int qs)
	/*@modifies errno@*/;

	extern int
tcgetattr (int fd, /*@out@*/ struct termios *p)
	/*@modifies errno, *p@*/;

	extern int
tcsendbreak (int fd, int d)
	/*@modifies errno@*/;

	extern int
tcsetattr (int fd, int opt, const struct termios *p)
	/*@modifies errno@*/;

/*
** time.h
*/

/* Environ must be known before it can be used in `globals' clauses. */

/*@unchecked@*/ extern char **environ;

/*@constant int CLK_TCK@*/

	extern void
tzset (void)
	/*@globals environ@*/ /*@modifies internalState@*/;

/*
** unistd.h
*/

/*@constant int F_OK@*/
/*@constant int R_OK@*/
/*@constant int SEEK_CUR@*/
/*@constant int SEEK_END@*/
/*@constant int SEEK_SET@*/
/*@constant int STDERR_FILENO@*/
/*@constant int STDIN_FILENO@*/
/*@constant int STDOUT_FILENO@*/
/*@constant int W_OK@*/
/*@constant int X_OK@*/
/*@constant int _PC_CHOWN_RESTRUCTED@*/
/*@constant int _PC_MAX_CANON@*/
/*@constant int _PC_MAX_INPUT@*/
/*@constant int _PC_NAME_MAX@*/
/*@constant int _PC_NO_TRUNC@*/
/*@constant int _PC_PATH_MAX@*/
/*@constant int _PC_PIPE_BUF@*/
/*@constant int _PC_VDISABLE@*/
/*@constant int _POSIX_CHOWN_RESTRICTED@*/
/*@constant int _POSIX_JOB_CONTROL@*/
/*@constant int _POSIX_NO_TRUNC@*/
/*@constant int _POSIX_SAVED_IDS@*/
/*@constant int _POSIX_VDISABLE@*/
/*@constant int _POSIX_VERSION@*/
/*@constant int _SC_ARG_MAX@*/
/*@constant int _SC_CHILD_MAX@*/
/*@constant int _SC_CLK_TCK@*/
/*@constant int _SC_JOB_CONTROL@*/
/*@constant int _SC_NGROUPS_MAX@*/
/*@constant int _SC_OPEN_MAX@*/
/*@constant int _SC_SAVED_IDS@*/
/*@constant int _SC_STREAM_MAX@*/
/*@constant int _SC_TZNAME_MAX@*/
/*@constant int _SC_VERSION@*/

	extern /*@exits@*/ void
_exit (int status)
	/*@*/;

	extern int
access (const char *path, int mode)
	/*@modifies errno@*/;

	extern unsigned int
alarm (unsigned int)
	/*@modifies internalState@*/;

	extern int
chdir (const char *path)
	/*@modifies errno@*/;

	extern int
chown (const char *path, uid_t owner, gid_t group)
	/*@modifies fileSystem, errno@*/;

	extern int
close (int fd)
	/*@modifies fileSystem, errno, internalState@*/;
	/* state: record locks are unlocked */

	extern char *
ctermid (/*@returned@*/ /*@out@*/ /*@null@*/ char *s)
	/*@modifies *s, internalState@*/;

	/* cuserid is in the 1988 version of POSIX but removed in 1990 */
	extern char *
cuserid (/*@null@*/ /*@out@*/ char *s)
	/*@modifies *s@*/;

	extern int
dup2 (int fd, int fd2)
	/*@modifies errno, fileSystem@*/;

	extern int
dup (int fd)
	/*@modifies errno, fileSystem@*/;

	extern /*@mayexit@*/ int
execl (const char *path, const char *arg, ...)
	/*@modifies errno@*/;

	extern /*@mayexit@*/ int
execle (const char *file, const char *arg, ...)
	/*@modifies errno@*/;

	extern /*@mayexit@*/ int
execlp (const char *file, const char *arg, ...)
	/*@modifies errno@*/;

	extern /*@mayexit@*/ int
execv (const char *path, char *const argv[])
	/*@modifies errno@*/;

	extern /*@mayexit@*/ int
execve (const char *path, char *const argv[], char *const *envp)
	/*@modifies errno@*/;

	extern /*@mayexit@*/ int
execvp (const char *file, char *const argv[])
	/*@modifies errno@*/;

	extern pid_t
fork (void)
	/*@modifies fileSystem, errno@*/;

	extern long
fpathconf (int fd, int name)
	/*@modifies errno@*/;

	extern char *
getcwd (/*@returned@*/ /*@out@*/ char *buf, size_t size)
	/*@modifies errno, *buf@*/;

	extern gid_t
getegid (void)
	/*@*/;

	extern uid_t
geteuid (void)
	/*@*/;

	extern gid_t
getgid (void)
	/*@*/;

	extern int
getgroups (int gs, /*@out@*/ gid_t gl[])
	/*@modifies errno, gl[]@*/;

	extern /*@observer@*/ char *
getlogin (void)
	/*@*/;

	extern pid_t
getpgrp (void)
	/*@*/;

	extern pid_t
getpid (void)
	/*@*/;

	extern pid_t
getppid (void)
	/*@*/;

	extern uid_t
getuid (void)
	/*@*/;

	extern int
isatty (int fd)
	/*@*/;

	extern int
link (const char *o, const char *n)
	/*@modifies errno, fileSystem@*/;

	extern off_t
lseek (int fd, off_t offset, int whence)
	/*@modifies errno@*/;

	extern long
pathconf (const char *path, int name)
	/*@modifies errno@*/;

	extern int
pause (void)
	/*@modifies errno@*/;

	extern int
pipe (/*@out@*/ int fd[]) /* Out parameter noticed by Marc Espie. */
	/*@modifies errno@*/;

	extern ssize_t
read (int fd, /*@out@*/ void *buf, size_t nbyte)
	/*@modifies errno, *buf@*/;

	extern int
rmdir (const char *path)
	/*@modifies fileSystem, errno@*/;

	extern int
setgid (gid_t gid)
	/*@modifies errno, internalState@*/;

	extern int
setpgid (pid_t pid, pid_t pgid)
	/*@modifies errno, internalState@*/;

	extern pid_t
setsid (void)
	/*@*/;

	extern int
setuid (uid_t uid)
	/*@modifies errno, internalState@*/;

	extern unsigned int
sleep (unsigned int sec)
	/*@*/;

	extern long
sysconf (int name)
	/*@modifies errno@*/;

	extern pid_t
tcgetpgrp (int fd)
	/*@modifies errno@*/;

	extern int
tcsetpgrp (int fd, pid_t pgrpid)
	/*@modifies errno, internalState@*/;

	/* Q: observer ok? */
	extern /*@null@*/ /*@observer@*/ char *
ttyname (int fd)
	/*@modifies errno@*/;

	extern int
unlink (const char *path)
	/*@modifies fileSystem, errno@*/;

	extern ssize_t
write (int fd, const void *buf, size_t nbyte)
	/*@modifies errno@*/;

/*
** utime.h
*/

struct utimbuf {
  time_t	actime;
  time_t	modtime;
} ;

	extern int
utime (const char *path, /*@null@*/ const struct utimbuf *times)
	/*@modifies fileSystem, errno@*/;
