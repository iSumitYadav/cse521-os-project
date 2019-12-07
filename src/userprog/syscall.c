#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"

#include "threads/thread.h"



#include <user/syscall.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "threads/malloc.h"
#include "threads/synch.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"



struct lock filesys_lock;

struct process_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};

int process_add_file (struct file *file_ptr);
struct file* process_get_file (int fd);

static void syscall_handler (struct intr_frame *);
int user_to_kernel_ptr(const void *vaddr);
void get_arg (struct intr_frame *frame, int *arg, int n);
void check_valid_ptr (const void *vaddr);
void check_valid_buffer (void* buffer, unsigned size);

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


void exit (int status)
{
  struct thread *cur = thread_current();
  if(is_thread_alive(cur->parent))
    {
      cur->cp->status_child = status;
    }
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}


void check_valid_ptr (const void *vaddr)
{
  if (!is_user_vaddr(vaddr) || vaddr < ((void *) 0x08048000))
    {
      // exit(-1); // General Error Given;
      struct thread *cur = thread_current();
  if(is_thread_alive(cur->parent))
    {
      cur->cp->status_child = -1;
    }
  printf ("%s: exit(%d)\n", cur->name, -1);
  thread_exit();
    }
}

int user_to_kernel_ptr(const void *vaddr)
{
  // TO DO: Need to check if all bytes within range are correct
  // for strings + buffers
  check_valid_ptr(vaddr);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
    {
      // exit(-1); // General Error Given;
      struct thread *cur = thread_current();
  if(is_thread_alive(cur->parent))
    {
      cur->cp->status_child = -1;
    }
  printf ("%s: exit(%d)\n", cur->name, -1);
  thread_exit();
    }
  return (int) ptr;
}

int process_add_file (struct file *file_ptr)
{
  struct process_file *pf = malloc(sizeof(struct process_file));
  pf->file = file_ptr;
  pf->fd = thread_current()->fd;
  thread_current()->fd++;
  list_push_back(&thread_current()->file_list, &pf->elem);
  return pf->fd;
}

struct file* process_get_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->file_list); e != list_end (&t->file_list);
       e = list_next (e))
        {
          struct process_file *pf = list_entry (e, struct process_file, elem);
          if (fd == pf->fd)
	    {
	      return pf->file;
	    }
        }
  return NULL;
}

void process_close_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->file_list);

  while (e != list_end (&t->file_list))
    {
      next = list_next(e);
      struct process_file *pf = list_entry (e, struct process_file, elem);
      if (fd == pf->fd || fd == -1)
	{
	  file_close(pf->file);
	  list_remove(&pf->elem);
	  free(pf);
	  if (fd != -1)
	    {
	      return;
	    }
	}
      e = next;
    }
}

struct child_process* add_child_process (int pid)
{
  struct child_process* cp = malloc(sizeof(struct child_process));
  cp->pid_child = pid;
  cp->load_child = 0;
  cp->wait_child = false;
  cp->exit_child = false;
  lock_init(&cp->wait_lock);
  list_push_back(&thread_current()->child_list,
		 &cp->elem);
  return cp;
}

struct child_process* get_child_process (int pid)
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->child_list); e != list_end (&t->child_list);
       e = list_next (e))
        {
          struct child_process *cp = list_entry (e, struct child_process, elem);
          if (pid == cp->pid_child)
		    {
		      return cp;
		    }
        }
  return NULL;
}

void remove_child_process (struct child_process *cp)
{
  list_remove(&cp->elem);
  free(cp);
}

void remove_child_processes (void)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->child_list);

  while (e != list_end (&t->child_list))
    {
      next = list_next(e);
      struct child_process *cp = list_entry (e, struct child_process,
					     elem);
      list_remove(&cp->elem);
      free(cp);
      e = next;
    }
}

void get_arg (struct intr_frame *frame, int *arg, int n)
{
  int i;
  int *ptr;
  for (i = 0; i < n; i++)
    {
      ptr = (int *) frame->esp + i + 1;
      check_valid_ptr((const void *) ptr);
      arg[i] = *ptr;
    }
}

void check_valid_buffer (void* buffer, unsigned size)
{
  unsigned i;
  char* local_buffer = (char *) buffer;
  for (i = 0; i < size; i++)
    {
      check_valid_ptr((const void*) local_buffer);
      local_buffer++;
    }
}

static void
syscall_handler (struct intr_frame *frame UNUSED) 
{
  int arg[3];
  check_valid_ptr((const void*) frame->esp);
  switch (* (int *) frame->esp)
    {
    case SYS_HALT:
      {
	shutdown_power_off();
	break;
      }
    case SYS_EXIT:
      {
	get_arg(frame, &arg[0], 1);
	// exit(arg[0]);
	struct thread *cur = thread_current();
  if(is_thread_alive(cur->parent))
    {
      cur->cp->status_child = arg[0];
    }
  printf ("%s: exit(%d)\n", cur->name, arg[0]);
  thread_exit();

	break;
      }
    case SYS_REMOVE:
      {
	get_arg(frame, &arg[0], 1);
	arg[0] = user_to_kernel_ptr((const void *) arg[0]);
	lock_acquire(&filesys_lock);
  	bool success = filesys_remove((const char *) arg[0]);
  	lock_release(&filesys_lock);
  	frame->eax = success;
	break;
      }

    case SYS_EXEC:
      {
	get_arg(frame, &arg[0], 1);
	arg[0] = user_to_kernel_ptr((const void *) arg[0]);
	const char *cmd_line = (const char *) arg[0];
	pid_t pid = process_execute(cmd_line);
  	struct child_process* cp = get_child_process(pid);
  	ASSERT(cp);
 
  	while (cp->load_child == 0)
    {
      barrier();
    }

  	if (cp->load_child == 2)
    {
      frame->eax = -1; // General Error Given
    }
    else
    {
    	frame->eax = pid;
    }

	break;
      }

    case SYS_WAIT:
      {
	get_arg(frame, &arg[0], 1);
	frame->eax = process_wait(arg[0]);
	break;
      }

    case SYS_CREATE:
      {
	get_arg(frame, &arg[0], 2);
	arg[0] = user_to_kernel_ptr((const void *) arg[0]);
	lock_acquire(&filesys_lock);
  	bool success = filesys_create((const char *)arg[0], arg[1]);
  	lock_release(&filesys_lock);
  	frame->eax = success;
	break;
      }

    case SYS_OPEN:
      {
	get_arg(frame, &arg[0], 1);
	arg[0] = user_to_kernel_ptr((const void *) arg[0]);
	lock_acquire(&filesys_lock);
  	struct file *file_temp = filesys_open((const char *) arg[0]);
  	if (!file_temp)
    {
      lock_release(&filesys_lock);
      frame->eax = -1; // General Error Given
    }
    else
    {
  	int fd = process_add_file(file_temp);
  	lock_release(&filesys_lock);
  	frame->eax = fd;
  	}
	break;
      }
    case SYS_CLOSE:
      {
	get_arg(frame, &arg[0], 1);
	lock_acquire(&filesys_lock);
  process_close_file(arg[0]);
  lock_release(&filesys_lock);
	break;
      }
    case SYS_FILESIZE:
      {
	get_arg(frame, &arg[0], 1);
	lock_acquire(&filesys_lock);
  struct file *file_temp = process_get_file(arg[0]);
  if (!file_temp)
    {
      lock_release(&filesys_lock);
      frame->eax = -1; // General Error Given
    }
    else
    {
  int size = file_length(file_temp);
  lock_release(&filesys_lock);
  frame->eax = size;
  	}
	break;
      }

    case SYS_READ:
      {
	get_arg(frame, &arg[0], 3);
	unsigned i;
  char* local_buffer = (char *) arg[1];
  for (i = 0; i < arg[2]; i++)
    {
      check_valid_ptr((const void*) local_buffer);
      local_buffer++;
    }
	arg[1] = user_to_kernel_ptr((const void *) arg[1]);
	if (arg[0] == STDIN_FILENO)
    {
      unsigned i;
      uint8_t* local_buffer = (uint8_t *) arg[1];
      for (i = 0; i < arg[2]; i++)
	{
	  local_buffer[i] = input_getc();
	}
      frame->eax = arg[2];
    }
    else
    {
  lock_acquire(&filesys_lock);
  struct file *file_temp = process_get_file(arg[0]);
  if (!file_temp)
    {
      lock_release(&filesys_lock);
      frame->eax = -1; // General Error Given
    }
    else
    {
  int bytes = file_read(file_temp, arg[1], arg[2]);
  lock_release(&filesys_lock);
  frame->eax = bytes;
	}
	}
	break;
      }

    case SYS_WRITE:
      {
	get_arg(frame, &arg[0], 3);
	unsigned i;
  char* local_buffer = (char *) arg[1];
  for (i = 0; i < arg[2]; i++)
    {
      check_valid_ptr((const void*) local_buffer);
      local_buffer++;
    }
	arg[1] = user_to_kernel_ptr((const void *) arg[1]);
	if (arg[0] == STDOUT_FILENO)
    {
      putbuf(arg[1], arg[2]);
      frame->eax = arg[2];
    }
    else
    {
  lock_acquire(&filesys_lock);
  struct file *file_temp = process_get_file(arg[0]);
  if (!file_temp)
    {
      lock_release(&filesys_lock);
      frame->eax = -1; // General Error Given
    }
    else
    {
  int bytes = file_write(file_temp, arg[1], arg[2]);
  lock_release(&filesys_lock);
  frame->eax = bytes;
	}
	}
	break;
      }

    case SYS_SEEK:
      {
	get_arg(frame, &arg[0], 2);
	lock_acquire(&filesys_lock);
  struct file *file_temp = process_get_file(arg[0]);
  if (!file_temp)
    {
      lock_release(&filesys_lock);
    }
  file_seek(file_temp, arg[1]);
  lock_release(&filesys_lock);
	break;
      }

    case SYS_TELL:
      {
	get_arg(frame, &arg[0], 1);
	lock_acquire(&filesys_lock);
  struct file *file_temp = process_get_file(arg[0]);
  if (!file_temp)
    {
      lock_release(&filesys_lock);
      frame->eax = -1; // General Error Given
    }
  off_t offset = file_tell(file_temp);
  lock_release(&filesys_lock);
  return offset;
	break;
      }

    }
}
