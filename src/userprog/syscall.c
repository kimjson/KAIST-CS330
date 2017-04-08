#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <threads/synch.h>
#include <string.h>
#include <stdlib.h>
#include <threads/malloc.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/kernel/console.h"
#include "lib/user/syscall.h"
#include "process.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
static bool is_invalid(void *);
static void handle_invalid(struct intr_frame *);
static bool is_duplicate_name(char *);

struct list file_name_list;


//struct file_name {
//  char name[16];
//  struct list_elem elem;
//};


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&file_name_list);
}

static bool is_duplicate_name(char *name) {

  if (list_empty(&file_name_list)) {
    return false;
  } else {
    struct list_elem *e;

    for (e = list_begin (&file_name_list); e != list_end (&file_name_list); e = list_next (e))
    {
      struct file_name *f = list_entry (e, struct file_name, elem);
      if (strcmp(f->name, name) == 0) {
        return true;
      }
    }
    return false;
  }
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
        f->eax = (uint32_t)result;
      } else if (is_duplicate_name(file)) {
        f->eax = (uint32_t)result;
      } else {
        lock_acquire(&lock);
        result = filesys_create(file, initial_size);
        struct file_name *file_name = malloc(sizeof(struct file_name));
        strlcpy(file_name->name, file, strlen(file)+1);
        list_push_back(&file_name_list, &file_name->elem);
        memcpy(&f->eax, &result, 1);
        lock_release(&lock);
      }
    } else if (syscall_number == SYS_OPEN) {
      if (is_invalid(f->esp + 4)) {
        handle_invalid(f);
      } else {
        char *file = *(char **)(f->esp + 4);
        if (is_invalid(file)) {
          handle_invalid(f);
        } else {
          lock_acquire(&lock);
          struct file *open_file = filesys_open(file);

          lock_release(&lock);
        }
      }
    }

  }
}
