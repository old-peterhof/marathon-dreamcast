#include <_ansi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <kos/fs.h>
#include <arch/dbgio.h>
#include <arch/arch.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#define	errno_macro	errno
#undef errno
extern int errno;

/* This is used by _sbrk.  */
register char *stack_ptr asm ("r15");

/*
   kos return 0 when err,
   newlib fd return -1 when err
   newlib fd must >=0 and short
*/

#define	MAX_OPEN	64
static file_t fh[MAX_OPEN];

#define	CHECKFILE(file)	if (file<3) { errno = EBADF; return -1; }
#define	FD2KOS(fd)	fh[fd]

#define	DPRINTF	dbgio_printf
//#define	DPRINTF(...)

int
_read (int file,
       char *ptr,
       int len)
{
//	DPRINTF("_read(%d,%p,%d)\n",file,ptr,len);
  CHECKFILE(file);
  return fs_read(FD2KOS(file),ptr,len);
}

int
_lseek (int file,
	int ptr,
	int dir)
{
//	DPRINTF("_lseek(%d,%d,%d)\n",file,ptr,dir);
  CHECKFILE(file);
  return fs_seek(FD2KOS(file),ptr,dir);
}

int
_write ( int file,
	 const char *ptr,
	 int len)
{
//	DPRINTF("_write(%d,%p,%d)\n",file,ptr,len);
  switch(file) {
  case 0: /* stdin */
    return -1;
  case 1: /* stdout */
  case 2: /* stderr */
    dbgio_writek(ptr,len);
    return len;
  default:
    return fs_write(FD2KOS(file),ptr,len);
  }
}

int
_close (int file)
{
	dbgio_printf("_close(%d)\n",file);
  CHECKFILE(file);
  fs_close(FD2KOS(file));
  fh[file]=0;
  return 0;
}

int
filelength (int file)
{
	DPRINTF("filelength(%d)\n",file);
  CHECKFILE(file);
//  dbgio_printf("seek %x,%d,%d\n",FD2KOS(file),ptr,dir);
  return fs_total(FD2KOS(file));
}

int access(const char *name,int mode)
{
	int fd;
	printf("access(%s,%x)\n",name,mode);
	fd = fs_open(name,O_RDONLY);
	if (fd==0) {
		if (memcmp(name,"/mem/",5)!=0) fd = fs_open(name,O_DIR);
		if (fd==0) {
			errno_macro = ENOENT;
			dbgio_printf("err\n");
			return -1;
		}
	}
	fs_close(fd);
	return 0;
}

int
_open (const char *path,
	int flags)
{
  file_t f;
  int fd;

	dbgio_printf("_open(%s,%x) ",path?path:"NULL",flags);
//  dbgio_printf(path);
  if ((flags&3)==O_WRONLY) {
	printf("write open %s\n",path);
  }

  for(fd=3;fd<MAX_OPEN && fh[fd];fd++) ;
  if (fd==MAX_OPEN) {
	dbgio_printf(" err max_open\n");
	errno = EMFILE;
    return -1;
  }
  f = fs_open(path,flags);
  if (f==0) {
	dbgio_printf(" err\n");
//	dbgio_printf("can't_open(%s,%x)\n",path?path:"NULL",flags);
	errno = ENOENT;
	return -1;
  }
  dbgio_printf("%d:%x\n",fd,f);
  fh[fd] = f;
  return fd;
}

int
_fstat (int file,
	struct stat *st)
{
  int size;
	DPRINTF("_fstat(%d,%p) ",file,st);
  CHECKFILE(file);

  size = fs_total(FD2KOS(file));
 	dbgio_printf("%d\n",size);
  if (size==-1) {
  	dbglog(0,"fstat:file %d\n",file);
  	return -1;
  }

  memset(st,0,sizeof(*st));

  st->st_mode = S_IFREG;
  st->st_size = size;
  return 0;
}

int mkdir(const char *path,mode_t mode)
{
	DPRINTF("mkdir(%s,%x)\n",path,mode);
	errno_macro = ENOSYS;
	printf("mkdir:%s\n",path);
	return -1;
}
/*
int
_stat (const char *name,
	struct stat *st)
{
  int fd;
  fd = fs_open(name,O_RDONLY);

  size = fs_total(FD2KOS(file));
  if (size==-1) {
  	dbglog(0,"fstat:file %d\n",file);
  	return -1;
  }

  memset(st,0,sizeof(*st));

  st->st_mode = S_IFREG;
  st->st_size = size;
  return 0;
}
*/

int
_creat (const char *path,
	int mode)
{
	DPRINTF("_creat(%s,%x)\n",path,mode);
  return _open(path,O_WRONLY|O_TRUNC /*|O_CREAT*/);
}

int
_rename (const char *oldpath,const char *newpath)
{
	DPRINTF("_rename(%s,%s)\n",oldpath,newpath);
  return fs_rename(oldpath,newpath);
}

int
rename (const char *oldpath,const char *newpath)
{
	return _rename(oldpath,newpath);
}

int
_unlink (const char *path)
{
	int ret;
	dbgio_printf("_unlink(%s)\n",path);
	ret = fs_unlink(path);
	if (ret==0) return 0;
	else {
		errno = ENOENT;
/*
		int fd = fs_open(path,O_RDONLY);
		if (fd==0) {
			fd = fs_open(path,O_DIR);
			if (fd==0) {
				errno = ENOENT;
				return -1;
			} else {
				errno = EPERM;
			}
		} else {
			errno = EACCES;
		}
		fs_close(fd);
*/
	}
	dbgio_printf("err\n");
	return -1;
}

int chdir(const char *path)
{
	DPRINTF("_chdir(%s)\n",path);
  return fs_chdir(path);
}

_exit (int n)
{
  arch_exit();
}

#include <sys/dirent.h>

DIR* opendir(const char *path)
{
	DIR *ret;
	dbgio_printf("_opendir(%s) ",path);
	ret = (DIR*)fs_open(path,O_DIR);
	if (ret==NULL) {
		errno_macro = ENOENT;
		dbgio_printf("err\n");
	} else {
		dbgio_printf("%p\n",ret);
	}
	return ret;
}

struct dirent *readdir (DIR *dir)
{
	DPRINTF("_readdir(%p)\n",dir);
	return fs_readdir((file_t)dir);
}

int closedir(DIR *dir)
{
	dbgio_printf("_closedir(%p)\n",dir);
	fs_close((file_t)dir);
	return 0;
}

caddr_t
_sbrk (int incr)
{
  extern char end;		/* Defined by the linker */
  static char *heap_end;
  static int total;
  char *prev_heap_end;

  if (heap_end == 0)
    {
      heap_end = &end;
    }
  prev_heap_end = heap_end;
  dbgio_printf("sbrk:%d\n",incr);
  if (heap_end + incr > stack_ptr)
    {
      _write (1, "Heap and stack collision\n", 25);
      abort ();
    }
  heap_end += incr;
  total += incr;
  dbgio_printf("total:%d\n",total);
  return (caddr_t) prev_heap_end;
}

caddr_t sbrk (int incr) { return sbrk(incr); }
int mm_init() {return 0;}

int
_link (char *old, char *new)
{
	errno = ENOSYS;
  return -1;
}

int
isatty (int fd)
{
//  if (fd<3) return 1;
  return 0;
}

_kill (n, m)
{
	errno = ENOSYS;
  return -1;
}

_getpid (n)
{
  return 1;
}

/*
_raise ()
{
}
*/

/*
int
_stat (const char *path, struct stat *st)

{
  return __trap34 (SYS_stat, path, st, 0);
}
*/

int
_chmod (const char *path, short mode)
{
	errno = ENOSYS;
  return -1;
}

int
_chown (const char *path, short owner, short group)
{
	errno = ENOSYS;
  return -1;
}

int
_utime (path, times)
     const char *path;
     char *times;
{
	errno = ENOSYS;
  return -1;
}

int
_fork ()
{
	errno = ENOSYS;
  return -1;
}

int
_wait (statusp)
     int *statusp;
{
	errno = ENOSYS;
  return -1;
}

int
_execve (const char *path, char *const argv[], char *const envp[])
{
	errno = ENOSYS;
  return -1;
}

int
_execv (const char *path, char *const argv[])
{
	errno = ENOSYS;
  return -1;
}

int
_pipe (int *fd)
{
	errno = ENOSYS;
  return -1;
}

#if 0
/* This is only provided because _gettimeofday_r and _times_r are
   defined in the same module, so we avoid a link error.  */
clock_t
_times (struct tms *tp)
{
  return -1;
}

int
_gettimeofday (struct timeval *tv, struct timezone *tz)
{
  tv->tv_usec = 0;
  tv->tv_sec = __trap34 (SYS_time);
  return 0;
}

static inline int
__setup_argv_for_main (int argc)
{
  char **argv;
  int i = argc;

  argv = __builtin_alloca ((1 + argc) * sizeof (*argv));

  argv[i] = NULL;
  while (i--) {
    argv[i] = __builtin_alloca (1 + __trap34 (SYS_argnlen, i));
    __trap34 (SYS_argn, i, argv[i]);
  }

  return main (argc, argv);
}

int
__setup_argv_and_call_main ()
{
  int argc = __trap34 (SYS_argc);

  if (argc <= 0)
    return main (argc, NULL);
  else
    return __setup_argv_for_main (argc);
}

#endif
