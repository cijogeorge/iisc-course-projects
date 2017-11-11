#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <list.h>
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "devices/shutdown.h"
/*My Implementation*/
#include "vm/vm.h"
#include "userprog/pagedir.h"
/*==My Implementation*/

#define MAX_FD 100

typedef int pid_t;

/* For mapping fd to file. */
static struct open_fd
{
  int fd;
  struct file *file;
  struct list_elem file_list_elem;
  struct list_elem open_files_elem;
};

static void syscall_handler (struct intr_frame *);

static void halt (void);
static int exec (const char *file);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static int tell (int fd);
static void close (int fd);
/*My Implementation*/
static mapid_t sys_mmap (int fd, void *vaddr);
static void sys_munmap (mapid_t mapid);
/*==MyImplementation*/

static mapid_t sys_mmap (int fd, void *vaddr);
static void sys_munmap (mapid_t mapid);

static struct open_fd *find_from_file_list (int fd);
static struct open_fd *find_from_open_files (int fd);

static int generate_fd (void);
static void return_fd (int fd);
static bool fd_status [MAX_FD + 2];

static struct lock filesys_lock;
  
static struct list file_list;

void
syscall_init (void) 
{
  int i;

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  list_init (&file_list);
  lock_init (&filesys_lock);
  
  for (i = 2; i < MAX_FD + 2; i++)
    fd_status [i] = 0;
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *sp;
  
  sp = f->esp;
  
  if (!is_user_vaddr (sp) || !is_user_vaddr (sp + 1) || !is_user_vaddr (sp + 2) || !is_user_vaddr (sp + 3))
    exit (-1);

  switch (*sp)
  {
    case SYS_HALT	: halt ();
		          break; 
    case SYS_EXIT	: exit (*(sp + 1));
                          break;
    case SYS_EXEC	: f->eax = exec (*(sp + 1));
                          break;
    case SYS_WAIT	: f->eax = wait (*(sp + 1));
                          break;
    case SYS_CREATE	: f->eax = create (*(sp + 1), *(sp + 2));
			  break;
    case SYS_REMOVE	: f->eax = remove (*(sp + 1));
			  break;
    case SYS_OPEN	: f->eax = open (*(sp + 1));
			  break;
    case SYS_FILESIZE	: f->eax = filesize (*(sp + 1));
			  break;
    case SYS_READ	: f->eax = read (*(sp + 1), *(sp + 2), *(sp + 3));
			  break;
    case SYS_WRITE	: f->eax = write (*(sp + 1), *(sp + 2), *(sp + 3));
			  break;
    case SYS_SEEK	: seek (*(sp + 1), *(sp + 2));
			  break;
    case SYS_TELL	: f->eax = tell (*(sp + 1));
			  break;
    case SYS_CLOSE	: close (*(sp + 1));
			  break;
    case SYS_MMAP	: f->eax = sys_mmap (*(sp + 1), *(sp + 2)); 
 			  break;
    case SYS_MUNMAP	: sys_munmap (*(sp + 1));
			  break;
    default		: exit (-1);
  }
  
  return;
}

static void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  struct list_elem *e;
  struct thread *t = thread_current ();
 
  t->exit_status = status;
  process_exit ();
 
  while (!list_empty (&t->open_files))
  {
    e = list_begin (&t->open_files);
    close (list_entry (e, struct open_fd, open_files_elem)->fd);
  }
  
  /* Exit thread */
  thread_exit ();
  
}

static pid_t
exec (const char *file)
{
  /*if (!file)
    return -1;*/
  
  if (!file || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))
    exit (-1);

  return process_execute (file);
}

static int
wait (pid_t pid)
{
  return process_wait (pid);
}

static bool
create (const char *file, unsigned initial_size)
{
  /*if (!file)
    return false;*/

  if (!file || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))
    exit (-1);
  
  return filesys_create (file, initial_size);
}

static bool
remove (const char *file)
{
  /*if (!file)
    return false;*/

  /*if (!is_user_vaddr (file))
    exit (-1);*/
 
  if (!file || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))
    exit (-1);
    
  return filesys_remove (file);
}

static int
open (const char *file)
{
  struct file *f;
  struct open_fd *ofd;

  /*if (!file)
    return -1;*/

  if (!file || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))
    exit (-1);

  f = filesys_open (file);
  
  if (!f)
    return -1;
    
  ofd = (struct open_fd *) malloc (sizeof (struct open_fd));
  
  if (!ofd)
  {
    file_close (f);
    return -1;
  }
    
  ofd->file = f;
  ofd->fd = generate_fd ();
  if (ofd->fd == -1)
  {
    file_close (f);
    free (ofd);
    return -1;
  }

  list_push_back (&file_list, &ofd->file_list_elem);
  list_push_back (&thread_current ()->open_files, &ofd->open_files_elem);
   
  return ofd->fd;
}

static int
filesize (int fd)
{
  struct open_fd *ofd;
  
  ofd = find_from_file_list (fd);

  if (!ofd)
    return -1;

  return file_length (ofd->file);
}

static int
read (int fd, void *buffer, unsigned length)
{
  struct open_fd * ofd;
  int i, ret = -1;

  if (!buffer || !is_user_vaddr (buffer) || !pagedir_get_page (thread_current ()->pagedir, buffer))
    exit (-1); 
  
  if (fd == STDOUT_FILENO)
    return ret;

  lock_acquire (&filesys_lock);

  if (fd == STDIN_FILENO)
  {
    for (i = 0; i < length; i++)
      *(char *) (buffer + i) = input_getc ();

    ret = length;
  }

  else if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + length))
  {
    lock_release (&filesys_lock);
    exit (-1);
  }

  else
  {
    ofd = find_from_file_list (fd);
   
    if (!ofd)
    {
      lock_release (&filesys_lock);
      return ret;
    }

    ret = file_read (ofd->file, buffer, length);
  }
    
  lock_release (&filesys_lock);
  return ret;
}

static int
write (int fd, const void *buffer, unsigned length)
{
  struct open_fd *ofd;
  int ret = -1;

  if (!buffer || !is_user_vaddr (buffer) || !pagedir_get_page (thread_current ()->pagedir, buffer))
    exit (-1);

  if (fd == STDIN_FILENO)
    return ret;
  
  lock_acquire (&filesys_lock);

  if (fd == STDOUT_FILENO)
    putbuf (buffer, length);

  else if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + length))
  {
    lock_release (&filesys_lock);
    exit (-1);
  }

  else
  {
    ofd = find_from_file_list (fd);

    if (!ofd)
    {
      lock_release (&filesys_lock);
      return ret;
    }
   
    ret = file_write (ofd->file, buffer, length);
  }
    
  lock_release (&filesys_lock);
  return ret;
}

static void
seek (int fd, unsigned position)
{
  struct open_fd *ofd;
  
  ofd = find_from_file_list (fd);

  if (!ofd)
    return;

  file_seek (ofd->file, position);
}

static int
tell (int fd)
{
  struct open_fd *ofd;
  
  ofd = find_from_file_list (fd);
  if (!ofd)
    return -1;

  return file_tell (ofd->file);
}

static void
close (int fd)
{
  struct open_fd *ofd;
  
  ofd = find_from_open_files (fd);
  
  if (!ofd)
    return;
 
  file_close (ofd->file);

  list_remove (&ofd->file_list_elem);
  list_remove (&ofd->open_files_elem);
  free (ofd);
  return_fd (fd);
}

/*My Implementation*/
static mapid_t
sys_mmap (int fd, void *vaddr)
{
  struct open_fd *ofd;

  
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO || !vaddr)
  {
    return -1;
  }
  if (!(ofd = find_from_open_files (fd)))
  {
    return -1;
  }
  if ((uint32_t)vaddr % PGSIZE != 0) /* miss-align */
  {
    return -1;
  }
    
 return vm_mmap (thread_current ()->pagedir, ofd->file, vaddr);
}

static void
sys_munmap (mapid_t mapid)
{
  return vm_munmap (mapid);
}

/*==My Implementation*/

static struct open_fd *
find_from_file_list (int fd)
{
  struct open_fd *ofd;
  struct list_elem *e;
  
  for (e = list_begin (&file_list); e != list_end (&file_list); e = list_next (e))
  {
    ofd = list_entry (e, struct open_fd, file_list_elem);
    
    if (ofd->fd == fd)
      return ofd;
  }
    
  return NULL;
}

static struct open_fd *
find_from_open_files (int fd)
{
  struct open_fd *ofd;
  struct list_elem *e;
  struct thread *t = thread_current ();
  
  for (e = list_begin (&t->open_files); e != list_end (&t->open_files); e = list_next (e))
    {
      ofd = list_entry (e, struct open_fd, open_files_elem);
      
      if (ofd->fd == fd)
        return ofd;
    }
    
  return NULL;
}

static int
generate_fd (void)
{
  int i, fd = -1;

  for (i = 2; i < MAX_FD + 2; i++)
  {
    if (fd_status [i] == 0)
    {
      fd_status [i] = 1;
      fd = i;
      break;
    }
  }
 
  return fd;
}

static void
return_fd (int fd)
{
  fd_status [fd] = 0;
}
