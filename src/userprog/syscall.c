#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <filesys/filesys.h>
#include <threads/synch.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/kernel/console.h"
#include "lib/user/syscall.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static bool is_invalid(void *);
static void handle_invalid(struct intr_frame *);

struct list file_name_list;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&file_name_list);
}

static bool is_invalid(void *addr) {
  return addr == NULL || is_kernel_vaddr(addr) || pagedir_get_page (thread_current()->pagedir, addr) == NULL;
}

static void handle_invalid(struct intr_frame *f) {
  int status = -1;
  memcpy(&f->eax, &status, 4);
  printf("%s: exit(%d)\n", thread_current()->exec_name, status);
  thread_exit();
}

static void
syscall_handler (struct intr_frame *f)// UNUSED)
{
  // memory access validation
  if (is_invalid(f->esp)) {

    handle_invalid(f);

  } else {

    // lock init
    struct lock lock;
    lock_init(&lock);

    // get syscall number
    int syscall_number = *(int *)(f->esp);
    if (syscall_number == SYS_EXIT) {
      // exit syscall
      if (is_invalid(f->esp + 4)) {
        handle_invalid(f);
      } else {

        int status = *(int*)(f->esp+4);

        memcpy(&f->eax, &status, 4);
        printf("%s: exit(%d)\n", thread_current()->exec_name, status);
        thread_exit();
      }
    } else if (syscall_number == SYS_WRITE) {

      // write syscall
      unsigned size = *(unsigned *)(f->esp + 12);
      char *buffer = *(char **)(f->esp + 8 );
      int fd = *(int *)(f->esp + 4);

      if (fd == 1) {
        putbuf(buffer, size);
      }

    } else if (syscall_number == SYS_HALT) {

      // halt syscall
      int status = *(int *)(f->esp + 4);

      thread_exit();

    } else if (syscall_number == SYS_CREATE) {

      if (is_invalid(f->esp +4) || is_invalid(f->esp + 8)) {
        handle_invalid(f);
      }

      // create syscall
      char *file = *(char **)(f->esp + 4);
      unsigned initial_size = *(unsigned *)(f->esp + 8);
      bool result = false;

      if (is_invalid(file) || strlen(file) == 0) {
        handle_invalid(f);
      } else if (strlen(file) > 14) {
        memcpy(&f->eax, &result, 1);
      } else {
        lock_acquire(&lock);
        result = filesys_create(file, initial_size);
        memcpy(&f->eax, &result, 1);
        lock_release(&lock);
      }
    }

  }
}
