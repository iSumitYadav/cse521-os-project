#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

// #define CLOSE_ALL -1
// #define ERROR -1

// #define NOT_LOADED 0
// #define LOAD_SUCCESS 1
// #define LOAD_FAIL 2

struct child_process{
	bool wait_child;
	bool exit_child;

	int pid_child;
	int load_child;
	int status_child;

	struct lock wait_lock;
	struct list_elem elem;
};


void process_close_file(int fd);
void remove_child_processes(void);
struct child_process *add_child_process(int pid);
struct child_process *get_child_process(int pid);
void remove_child_process(struct child_process *cp);


void syscall_init (void);

#endif /* userprog/syscall.h */
