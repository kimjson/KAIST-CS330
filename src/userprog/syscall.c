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
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "lib/user/syscall.h"
#include "process.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
static bool is_invalid(void *);
static void handle_invalid(struct intr_frame *);
static bool is_duplicate_name(char *);
static int give_file_descriptor(struct file *);
static struct file *find_file_by_fd(int);
static bool thread_has_file(int);

//struct list file_name_list;
struct list file_list;
struct lock lock;

static bool fd_less_func(const struct list_elem *a, const struct list_elem *b, void *aux) {
  return list_entry(a,struct file, elem)->fd < list_entry(b,struct file, elem)->fd;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&file_list);

  // lock init
  lock_init(&lock);
}

static int give_file_descriptor(struct file *file_pointer) {
  struct list_elem *e;
  int fd_temp = 1;
  for (e = list_begin (&file_list); e != list_end (&file_list); e = list_next (e))
  {
    struct file *f = list_entry (e, struct file, elem);

    if (f->fd > fd_temp + 1) {
      file_pointer->fd = fd_temp + 1;
      list_insert_ordered(&file_list, &(file_pointer->elem), &fd_less_func, NULL);
      return fd_temp + 1;
    }
    fd_temp = f->fd;
  }
  file_pointer->fd = fd_temp + 1;
  list_push_back(&file_list, &file_pointer->elem);
  return fd_temp + 1;
}

static struct file *find_file_by_fd(int fd) {
  struct list_elem *e;
  for (e = list_begin (&file_list); e != list_end (&file_list); e = list_next (e))
  {
    struct file *f = list_entry (e, struct file, elem);
    if (f->fd == fd) {
      return list_entry(e, struct file, elem);
    }
  }
  return NULL;
}

static bool thread_has_file(int fd) {
  struct list_elem *e;
  for (e = list_begin (&thread_current()->file_list); e != list_end (&thread_current()->file_list); e = list_next (e))
  {
    struct file *f = list_entry (e, struct file, elem_for_thread);
    if (f->fd == fd) {
      return true;
    }
  }
  return false;
}

//static bool is_duplicate_name(char *name) {
//
//  if (list_empty(&file_name_list)) {
//    return false;
//  } else {
//    struct list_elem *e;
//
//    for (e = list_begin (&file_name_list); e != list_end (&file_name_list); e = list_next (e))
//    {
//      struct file_name *f = list_entry (e, struct file_name, elem);
//      if (strcmp(f->name, name) == 0) {
//        return true;
//      }
//    }
//    return false;
//  }
//}

static bool is_invalid(void *addr) {
  return addr == NULL || is_kernel_vaddr(addr) || pagedir_get_page (thread_current()->pagedir, addr) == NULL;
}

static void handle_invalid(struct intr_frame *f) {
  int status = -1;
  memcpy(&f->eax, &status, 4);
  printf("%s: exit(%d)\n", thread_current()->exec_name, status);
  thread_current()->info->is_killed = true;
  thread_exit();
}

static void
syscall_handler (struct intr_frame *f)// UNUSED)
{
  // memory access validation
  if (is_invalid(f->esp)) {

    handle_invalid(f);

  } else {
    // get syscall number
    int syscall_number = *(int *)(f->esp);
    if (syscall_number == SYS_EXIT) {
      // exit syscall
      if (is_invalid(f->esp + 4)) {
        handle_invalid(f);
      } else {

        int status = *(int*)(f->esp+4);

        thread_current()->info->exit_status = status;
        printf("%s: exit(%d)\n", thread_current()->exec_name, status);

        thread_exit();
      }
    } else if (syscall_number == SYS_WRITE) {
      // write syscall
      unsigned size = *(unsigned *)(f->esp + 12);
      char *buffer = *(char **)(f->esp + 8 );
      int fd = *(int *)(f->esp + 4);

      if (is_invalid(buffer)) {
        handle_invalid(f);
      }

      if (fd == 1) {
        putbuf(buffer, size);
      } else if (fd > 1) {
        struct file *found_file = find_file_by_fd(fd);
        if (found_file == NULL || !thread_has_file(fd)) {
          f->eax = (uint32_t) -1;
        } else {
          int result = file_write(found_file, buffer, size);
          f->eax = (uint32_t)result;
        }
      } else {
        f->eax = (uint32_t) -1;
      }

    } else if (syscall_number == SYS_HALT) {

      // halt syscall
      int status = *(int *)(f->esp + 4);

      thread_exit();

    } else if (syscall_number == SYS_CREATE) {
      // create syscall

      if (is_invalid(f->esp +4) || is_invalid(f->esp + 8)) {
        handle_invalid(f);
      }

      char *file = *(char **)(f->esp + 4);
      unsigned initial_size = *(unsigned *)(f->esp + 8);
      bool result = false;

      if (is_invalid(file) || strlen(file) == 0) {
        handle_invalid(f);
      } else if (strlen(file) > 14) {
        f->eax = (uint32_t)result;
      } else if (filesys_open(file) != NULL) {
        f->eax = (uint32_t)result;
      } else {
        lock_acquire(&lock);
        result = filesys_create(file, initial_size);
        struct file_name *file_name = malloc(sizeof(struct file_name));
        strlcpy(file_name->name, file, strlen(file)+1);
        f->eax = (uint32_t)result;
        lock_release(&lock);
      }
    } else if (syscall_number == SYS_OPEN) {
      // Open syscall
//      printf("OPEN\n");
      if (is_invalid(f->esp + 4)) {
        handle_invalid(f);
      } else {
        char *file = *(char **) (f->esp + 4);
        if (is_invalid(file)) {
          handle_invalid(f);
        } else {
          int result;
          lock_acquire(&lock);
          struct file *open_file = filesys_open(file);
          if (open_file != NULL) {
//            printf("OPEN FILE NOT NULL\n");
            result = give_file_descriptor(open_file);
            list_push_back(&thread_current()->file_list, &open_file->elem_for_thread);
            f->eax = (uint32_t) result;
          } else {
//            printf("OPEN FILE NULL\n");
            f->eax = (uint32_t) -1;
          }
          lock_release(&lock);
        }
      }
    } else if (syscall_number == SYS_REMOVE) {

    } else if (syscall_number == SYS_CLOSE) {
//      printf("CLOSE\n");
      int fd = *(int *)(f->esp + 4);
      struct list_elem *e1, *e2;

      for (e1 = list_begin (&file_list); e1 != list_end (&file_list); e1 = list_next (e1))
      {
        struct file *f = list_entry (e1, struct file, elem);
        if (f->fd == fd) {
          list_remove(&f->elem);
          break;
        }
      }
      if (e1 == list_end(&file_list)) {
        // file that does not exist
//        thread_exit();
      }
      for (e2 = list_begin (&thread_current()->file_list); e2 != list_end (&thread_current()->file_list); e2 = list_next (e2))
      {
        struct file *f = list_entry (e2, struct file, elem_for_thread);
        if (f->fd == fd) {
          list_remove(&f->elem_for_thread);
          break;
        }
      }
      if (e2 == list_end(&thread_current()->file_list)) {
        // file that is not open or owned by current thread.
//        thread_exit();
      }
    } else if (syscall_number == SYS_READ) {
      // Syscall read
//      printf("READ\n");
      int fd = *(int *)(f->esp + 4);
      char *buffer = *(char **)(f->esp + 8);
      unsigned size = *(unsigned *)(f->esp + 12);
      if (is_invalid(buffer)) {
        handle_invalid(f);
      }
      if (fd == 0) {
        unsigned i;
        for (i=0; i < size; i++) {
          buffer[i] = input_getc();
        }
        f->eax = (uint32_t)i;
      } else if (fd > 1) {
        struct file *read_file = find_file_by_fd(fd);
        if (read_file == NULL || !thread_has_file(fd)) {
          f->eax = (uint32_t)-1;
        } else {
          int result = file_read(read_file, buffer, size);
          f->eax = (uint32_t)result;
        }
      } else {
        f->eax = (uint32_t)-1;
      }
    } else if (syscall_number == SYS_FILESIZE) {
      // Syscall read
      int fd = *(int *)(f->esp + 4);

      if (fd > 1) {
        struct file *found_file = find_file_by_fd(fd);
        if (found_file == NULL || !thread_has_file(fd)) {
          f->eax = (uint32_t)-1;
        } else {
//          printf("NORMAL FILESIZE\n");
          int result = file_length(found_file);
          f->eax = (uint32_t)result;
        }
      } else {
        f->eax = (uint32_t)-1;
      }
    } else if (syscall_number == SYS_EXEC) {
      if (is_invalid(f->esp + 4)) {
        handle_invalid(f);
      }
      char *cmd_line = *(char **) (f->esp + 4);
      if (is_invalid(cmd_line)) {
        handle_invalid(f);
      }
      pid_t result = process_execute(cmd_line);
      struct thread *child = find_child_by_tid((tid_t) result);
      struct thread_info *child_info = find_child_info_by_tid((tid_t) result);
      if (child != NULL && child_info != NULL) {
        sema_down(&child->info->exec_sema);
        //printf("flag1\n");
        if (!child_info->load_success) {
          result = -1;
        }
      }
      f->eax = (uint32_t)result;
    } else if (syscall_number == SYS_WAIT) {
      pid_t pid = *(pid_t *)(f->esp + 4);
      int result = process_wait((tid_t)pid);
      f->eax = (uint32_t)result;
    }
  }
}
