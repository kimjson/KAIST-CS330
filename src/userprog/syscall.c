#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/kernel/console.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)// UNUSED)
{
  // memory access validation
  if (f->esp == NULL) {
    thread_exit();
  } else if (is_kernel_vaddr(f->esp)) {
    thread_exit();
  } else if (pagedir_get_page (thread_current()->pagedir, f->esp) == NULL) {
    thread_exit();
  }

  // get syscall number
  int syscall_number = *(int *)(f->esp);
  if (syscall_number == SYS_EXIT) {
    // exit syscall
    thread_exit();
  } else if (syscall_number == SYS_WRITE) {
    // write syscall
    unsigned size = *(unsigned *)(f->esp - 4);
    char *buffer = *(char **)(f->esp - 8);
    int fd = *(int *)(f->esp - 12);
    if (fd == 1) {
      putbuf(buffer, size);
    }
  }
}
