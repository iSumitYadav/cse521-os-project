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

int process_include_file (struct file *file_ptr);
struct file* process_take_file (int fd);

static void syscall_handler (struct intr_frame *);
int pointer_from_user_to_kernel(const void *vaddr);
void get_arg (struct intr_frame *frame, int *arg, int n);
void check_ptr (const void *vaddr);
void check_buffer (void* buffer, unsigned size);

void syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


void exit (int status)
{
  struct thread *cur = thread_current();
  if(is_thread_alive(cur->parent)){
      cur->cp->c_status = status;
    }
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

int pointer_from_user_to_kernel(const void *vaddr)
{
  // TO DO: Need to check if all bytes within range are correct
  // for strings + buffers
  check_ptr(vaddr);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr){
      // exit(-1); // General Error Given;
      struct thread *cur = thread_current();
	  if(is_thread_alive(cur->parent)){
	      cur->cp->c_status = -1;
	    }
	  printf ("%s: exit(%d)\n", cur->name, -1);
	  thread_exit();
    }
  return (int) ptr;
}

void check_ptr (const void *vaddr)
{
  if (!is_user_vaddr(vaddr) || vaddr < ((void *) 0x08048000)){
      // exit(-1); // General Error Given;
      struct thread *cur = thread_current();
	  if(is_thread_alive(cur->parent))
	    {
	      cur->cp->c_status = -1;
	    }
	  printf ("%s: exit(%d)\n", cur->name, -1);
	  thread_exit();
    }
}


int process_include_file (struct file *file_ptr)
{
  struct process_file *pf = malloc(sizeof(struct process_file));
  pf->file = file_ptr;
  pf->fd = thread_current()->fd;
  thread_current()->fd++;
  list_push_back(&thread_current()->file_list, &pf->elem);
  return pf->fd;
}

struct file* process_take_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *elem;

  for (elem = list_begin (&t->file_list); elem != list_end (&t->file_list);elem = list_next (elem))
    {
      struct process_file *pf = list_entry (elem, struct process_file, elem);
      if (fd == pf->fd)
      	return pf->file;
    }
  return NULL;
}

void p_file_close (int fd)
{
    struct thread *t = thread_current();
    struct list_elem *next, *elem = list_begin(&t->file_list);

  	while (elem != list_end (&t->file_list)){
	  next = list_next(elem);
	  struct process_file *pf = list_entry (elem, struct process_file, elem);
	  if (fd == pf->fd || fd == -1){
		  file_close(pf->file);
		  list_remove(&pf->elem);
		  free(pf);
		  if (fd != -1){
		      return;
		    }
		}
	  elem = next;
	}
}

struct child_process* take_child_process (int pid)
{
  struct thread *t = thread_current();
  struct list_elem *elem;

  for (elem = list_begin (&t->child_list); elem != list_end (&t->child_list);elem = list_next (elem)){
      struct child_process *cp = list_entry (elem, struct child_process, elem);
      if (pid == cp->c_pid){
	      return cp;
	    }
    }
  return NULL;
}

void get_arg (struct intr_frame *frame, int *arg, int n)
{
  int i;
  int *ptr;
  for (i = 0; i < n; i++){
      ptr = (int *) frame->esp + i + 1;
      check_ptr((const void *) ptr);
      arg[i] = *ptr;
    }
}

void check_buffer (void* buffer, unsigned size)
{
  unsigned i;
  char* local_buffer = (char *) buffer;
  for (i = 0; i < size; i++){
      check_ptr((const void*) local_buffer);
      local_buffer++;
    }
}

static void syscall_handler (struct intr_frame *frame UNUSED) 
{
  int arg[3];
  check_ptr((const void*) frame->esp);
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
			struct thread *cur = thread_current();
		    if(is_thread_alive(cur->parent))
		    {
		      cur->cp->c_status = arg[0];
		    }
		    printf ("%s: exit(%d)\n", cur->name, arg[0]);
		    thread_exit();

			break;
	    }
	    case SYS_REMOVE:
	    {
			get_arg(frame, &arg[0], 1);
			arg[0] = pointer_from_user_to_kernel((const void *) arg[0]);
			lock_acquire(&filesys_lock);
		  	bool success = filesys_remove((const char *) arg[0]);
		  	lock_release(&filesys_lock);
		  	frame->eax = success;

			break;
		}

	    case SYS_EXEC:
	    {
			get_arg(frame, &arg[0], 1);
			arg[0] = pointer_from_user_to_kernel((const void *) arg[0]);
			const char *cmd_line = (const char *) arg[0];
			pid_t pid = process_execute(cmd_line);
		  	struct child_process* cp = take_child_process(pid);
		  	ASSERT(cp);
		 
		  	while (cp->c_load == 0){
		      barrier();
		    }

		  	if (cp->c_load == 2){
		      frame->eax = -1; // General Error Given
		    }
		    else{
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
			arg[0] = pointer_from_user_to_kernel((const void *) arg[0]);
			lock_acquire(&filesys_lock);
		  	bool success = filesys_create((const char *)arg[0], arg[1]);
		  	lock_release(&filesys_lock);
		  	frame->eax = success;
		
			break;
	    }

	    case SYS_OPEN:
	    {
			get_arg(frame, &arg[0], 1);
			arg[0] = pointer_from_user_to_kernel((const void *) arg[0]);
			lock_acquire(&filesys_lock);
		  	struct file *file_temp = filesys_open((const char *) arg[0]);
		  	if (!file_temp){
		      lock_release(&filesys_lock);
		      frame->eax = -1; // General Error Given
		    }
		    else{
			  	int fd = process_include_file(file_temp);
			  	lock_release(&filesys_lock);
			  	frame->eax = fd;
		  	}
			
			break;
	    }
	    case SYS_CLOSE:
	    {
			get_arg(frame, &arg[0], 1);
			lock_acquire(&filesys_lock);
		    p_file_close(arg[0]);
		    lock_release(&filesys_lock);
			
			break;
	      }
	    case SYS_FILESIZE:
	    {
			get_arg(frame, &arg[0], 1);
			lock_acquire(&filesys_lock);
		    struct file *file_temp = process_take_file(arg[0]);
		    if (!file_temp){
		      lock_release(&filesys_lock);
		      frame->eax = -1; // General Error Given
		    }
		    else{
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
		    for (i = 0; i < arg[2]; i++){
		      check_ptr((const void*) local_buffer);
		      local_buffer++;
		    }
			arg[1] = pointer_from_user_to_kernel((const void *) arg[1]);
			if (arg[0] == STDIN_FILENO){
		      unsigned i;
		      uint8_t* local_buffer = (uint8_t *) arg[1];
		      for (i = 0; i < arg[2]; i++){
			  	local_buffer[i] = input_getc();
			  }
		      frame->eax = arg[2];
		    }
		    else{
			    lock_acquire(&filesys_lock);
			    struct file *file_temp = process_take_file(arg[0]);
			    if (!file_temp){
			      lock_release(&filesys_lock);
			      frame->eax = -1; // General Error Given
			    }
			    else{
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
		    for (i = 0; i < arg[2]; i++){
		      check_ptr((const void*) local_buffer);
		      local_buffer++;
		    }
			arg[1] = pointer_from_user_to_kernel((const void *) arg[1]);
			if (arg[0] == STDOUT_FILENO){
		      putbuf(arg[1], arg[2]);
		      frame->eax = arg[2];
		    }
		    else{
			    lock_acquire(&filesys_lock);
			    struct file *file_temp = process_take_file(arg[0]);
			    if (!file_temp){
			      lock_release(&filesys_lock);
			      frame->eax = -1; // General Error Given
			    }
			    else{
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
		    struct file *file_temp = process_take_file(arg[0]);
		    if (!file_temp){
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
	    struct file *file_temp = process_take_file(arg[0]);
	    if (!file_temp){
	      lock_release(&filesys_lock);
	      frame->eax = -1; // General Error Given
	    }
	    off_t offset = file_tell(file_temp);
	    lock_release(&filesys_lock);
	    // return offset;
		
		break;
	    }

    }
}
