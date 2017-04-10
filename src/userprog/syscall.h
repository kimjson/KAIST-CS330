#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
int give_file_descriptor(struct file *);

#endif /* userprog/syscall.h */
