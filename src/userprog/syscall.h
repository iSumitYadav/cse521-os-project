#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

struct child_process{
	bool c_wait;
	bool c_exit;

	int c_pid;
	int c_load;
	int c_status;

	struct lock lock_wait;
	struct list_elem elem;
};


void p_file_close(int fd);
struct child_process *take_child_process(int pid);


void syscall_init (void);

#endif /* userprog/syscall.h */
