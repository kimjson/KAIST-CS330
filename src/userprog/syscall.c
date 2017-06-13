#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <threads/synch.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "lib/user/syscall.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
static bool is_invalid(void *);
static void handle_invalid(struct intr_frame *);

static void handle_halt(struct intr_frame *);
static void handle_exit(struct intr_frame *);
static void handle_exec(struct intr_frame *);
static void handle_wait(struct intr_frame *);
static void handle_create(struct intr_frame *);
static void handle_remove(struct intr_frame *);
static void handle_open(struct intr_frame *);
static void handle_filesize(struct intr_frame *);
static void handle_read(struct intr_frame *);
static void handle_write(struct intr_frame *);
static void handle_seek(struct intr_frame *);
static void handle_tell(struct intr_frame *);
static void handle_close(struct intr_frame *);

// syscall for project 3-2
static void handle_mmap(struct intr_frame *);
static void handle_munmap(struct intr_frame *);

// syscall for project 4
static void handle_chdir(struct intr_frame *);
static void handle_mkdir(struct intr_frame *);
static void handle_readdir(struct intr_frame *);
static void handle_isdir(struct intr_frame *);
static void handle_inumber(struct intr_frame *);


static struct file *find_file_by_fd(int);
struct lock lock;

//static bool fd_less_func(const struct list_elem *a, const struct list_elem *b, void *aux) {
//  return list_entry(a,struct file, elem)->fd < list_entry(b,struct file, elem)->fd;
//}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  // lock init
  lock_init(&lock);
}

int give_file_descriptor(struct file *file_pointer) {
  struct thread *curr = thread_current();
  struct list_elem *e;

  if (list_empty(&curr->file_list)) {
    file_pointer->fd = 2;
    list_push_back(&curr->file_list, &file_pointer->elem_for_thread);
  } else {
    file_pointer->fd = list_entry(list_back(&curr->file_list), struct file, elem_for_thread)->fd + 1;
    list_push_back(&curr->file_list, &file_pointer->elem_for_thread);
  }
  return file_pointer->fd;
}

static struct file *find_file_by_fd(int fd) {
  struct list_elem *e;
  struct thread *curr = thread_current();
  for (e = list_begin (&curr->file_list); e != list_end (&curr->file_list); e = list_next (e))
  {
    struct file *f = list_entry (e, struct file, elem_for_thread);
    if (f->fd == fd) {
      return list_entry(e, struct file, elem_for_thread);
    }
  }
  return NULL;
}

static bool is_invalid(void *addr) {
  return addr == NULL || is_kernel_vaddr(addr) || pagedir_get_page (thread_current()->pagedir, addr) == NULL;
}

static void handle_invalid(struct intr_frame *f) {
  f->eax = (uint32_t)-1;
  printf("%s: exit(%d)\n", thread_current()->exec_name, -1);
  thread_current()->info->is_killed = true;
  thread_current()->info->exit_status = -1;
  thread_exit();
}

static void handle_exit(struct intr_frame *f) {
  // exit syscall
  if (is_invalid(f->esp + 4)) {
    handle_invalid(f);
  } else {

    int status = *(int*)(f->esp+4);
    thread_current()->info->exit_status = status;
    printf("%s: exit(%d)\n", thread_current()->exec_name, status);

    //

    thread_exit();
  }
}

static void handle_write(struct intr_frame *f) {
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
    lock_acquire(&lock);
    if (found_file == NULL || found_file->deny_write || inode_is_directory(file_get_inode (found_file))) {
      f->eax = (uint32_t) -1;
    } else {
      int result = file_write(found_file, buffer, size);
      f->eax = (uint32_t)result;
    }
    lock_release(&lock);
  } else {
    f->eax = (uint32_t) -1;
  }
}

static void handle_halt(struct intr_frame *f) {
  thread_exit();
}

static void handle_create(struct intr_frame *f) {
  if (is_invalid(f->esp +4) || is_invalid(f->esp + 8)) {
    handle_invalid(f);
  }

  char *file = *(char **)(f->esp + 4);
  unsigned initial_size = *(unsigned *)(f->esp + 8);
  bool result = false;

  lock_acquire(&lock);
  if (is_invalid(file) || strlen(file) == 0) {
    handle_invalid(f);
  } else if (strlen(file) > 14) {
    f->eax = (uint32_t)result;
  } else if (filesys_open(file) != NULL) {
    f->eax = (uint32_t)result;
  } else {

    result = filesys_create(file, initial_size);
    f->eax = (uint32_t)result;
  }
  lock_release(&lock);
}

static void handle_open(struct intr_frame *f) {
  // Open syscall
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
        result = give_file_descriptor(open_file);
        f->eax = (uint32_t) result;
      } else {
        f->eax = (uint32_t) -1;
      }
      lock_release(&lock);
    }
  }
}

static void handle_remove(struct intr_frame *f) {
  char *file = *(char **)(f->esp + 4);
  lock_acquire(&lock);
  f->eax = (uint32_t) filesys_remove(file);
  lock_release(&lock);
}

static void handle_close(struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  struct list_elem *e2;


  for (e2 = list_begin (&thread_current()->file_list); e2 != list_end (&thread_current()->file_list); e2 = list_next (e2))
  {
    struct file *f = list_entry (e2, struct file, elem_for_thread);
    if (f->fd == fd) {
      list_remove(&f->elem_for_thread);
      if (inode_is_directory(file_get_inode (f))) {
        dir_close(f);
      } else {
        file_close (f);
      }
      break;
    }

  }
  if (e2 == list_end(&thread_current()->file_list)) {
    thread_current()->info->exit_status = -1;
    printf("%s: exit(%d)\n", thread_current()->exec_name, -1);

    thread_exit();
  }
}

static void handle_read(struct intr_frame *f) {
  // Syscall read
  int fd = *(int *)(f->esp + 4);
  char *buffer = *(char **)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
 // printf("handle read\n");
  if(is_invalid(f->esp+4)||is_invalid(f->esp+8)||is_invalid(f->esp+12)){
    handle_invalid(f);
  }
//  if (is_invalid(buffer)) {
//    handle_invalid(f);
//  }
  lock_acquire(&lock);
  if (fd == 0) {
    unsigned i;
    for (i=0; i < size; i++) {
      buffer[i] = input_getc();
    }
    f->eax = (uint32_t)i;
  } else if (fd > 1) {
    struct file *read_file = find_file_by_fd(fd);
    if (read_file == NULL || inode_is_directory(file_get_inode (read_file))) {
      f->eax = (uint32_t)-1;
    } else {
      //printf("flag22222\n");
      int result = file_read(read_file, buffer, size);
      //printf("result:%d\n",result);
      f->eax = (uint32_t)result;
    }
  } else {
    f->eax = (uint32_t)-1;
  }
  lock_release(&lock);
}

static void handle_filesize(struct intr_frame *f) {
  // Syscall read
  int fd = *(int *)(f->esp + 4);

  if (fd > 1) {
    struct file *found_file = find_file_by_fd(fd);
    if (found_file == NULL) {
      f->eax = (uint32_t)-1;
    } else {
      int result = file_length(found_file);
      f->eax = (uint32_t)result;
    }
  } else {
    f->eax = (uint32_t)-1;
  }
}

static void handle_exec(struct intr_frame *f) {
  if (is_invalid(f->esp + 4)) {
    handle_invalid(f);
  }
  char *cmd_line = *(char **) (f->esp + 4);
  if (is_invalid(cmd_line)) {
    handle_invalid(f);
  }
  pid_t result = process_execute(cmd_line);
  struct thread_info *child_info = find_child_info_by_tid((tid_t) result);
  if (child_info != NULL) {
    sema_down(&child_info->exec_sema);
    if (!child_info->load_success) {
      result = -1;
    }
  }
  f->eax = (uint32_t)result;
}

static void handle_wait(struct intr_frame *f) {
  pid_t pid = *(pid_t *)(f->esp + 4);
  int result = process_wait((tid_t)pid);
  f->eax = (uint32_t)result;
}

static void handle_seek(struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  unsigned position = *(unsigned *)(f->esp + 8);
  struct file *file = find_file_by_fd(fd);
  file_seek(file, position);
}

static void handle_tell(struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  unsigned result;
  struct file *file = find_file_by_fd(fd);
  result = (unsigned)file_tell(file);
  f->eax = result;
}

static void handle_mmap(struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  void *addr = *(void **)(f->esp + 8);
  // if (is_invalid(addr)) {
  //    handle_invalid(f);
  //  }
  // if fd is 0,1 or addr is 0 or addr is not page aligned, return -1

  if (fd < 2 || addr == NULL || pg_round_down(addr) != addr) {
    f->eax = -1;
    return;
  } else {
    mapid_t new_mapping = 0;
    struct file *old_file = find_file_by_fd(fd);
    // if (list_entry(&old_file->mapping_elem, struct file, mapping_elem) != NULL) {
    //   f->eax = -1;
    //   return;
    // }
    struct file *target_file = file_reopen (old_file);
    int filesize = file_length (target_file);
    int page_num = filesize/PGSIZE;
    int page_remainder = filesize - PGSIZE*page_num;
    int i;
    for(i=0;i<page_num;i++){
      if (sup_page_table_lookup (&thread_current ()->sup_page_table, addr+PGSIZE*i) != NULL) {
        f->eax = -1;
        return;
      }
    }
    if (page_remainder > 0) {
      if (sup_page_table_lookup (&thread_current ()->sup_page_table, addr+PGSIZE*page_num) != NULL) {
        f->eax = -1;
        return;
      }
    }
    for(i=0;i<page_num;i++){
      struct sup_page_entry *sup_pte = sup_page_entry_create (addr+PGSIZE*i, NULL, target_file);
      sup_pte->file_pos = PGSIZE*i;
    }
    if (page_remainder > 0) {
      struct sup_page_entry *sup_pte = sup_page_entry_create (addr+PGSIZE*page_num, NULL, target_file);
      sup_pte->file_pos = PGSIZE*page_num;
    }
    if (!list_empty (&thread_current()->mapping_list)) {
      new_mapping = list_entry(list_back (&thread_current()->mapping_list), struct file, mapping_elem)->mapping + 1;
    }
    list_push_back (&thread_current()->mapping_list, &target_file->mapping_elem);
    f->eax = new_mapping;
  }
}

static void handle_munmap(struct intr_frame *f) {
  mapid_t mapping = *(mapid_t *)(f->esp + 4);
  struct list_elem *e;
  for (e = list_begin (&thread_current()->mapping_list); e != list_end (&thread_current()->mapping_list);
       e = list_next (e))
    {
      struct file *f = list_entry (e, struct file, mapping_elem);
      if (f->mapping == mapping) {
        struct hash_iterator i;
        hash_first (&i, &thread_current()->sup_page_table);
        bool is_continue = true;

        struct list hash_delete_list;
        list_init (&hash_delete_list);
        while (is_continue) {

          struct sup_page_entry *sup_pte = hash_entry(hash_cur(&i), struct sup_page_entry, hash_elem);

          if(hash_next (&i)){
            is_continue=true;
          }
          else{
            is_continue=false;
          }

          if(sup_pte->file_address==f){
            if(pagedir_is_dirty(thread_current()->pagedir, sup_pte->upage)){
              int result = file_write_at(f, sup_pte->kpage, PGSIZE, sup_pte->file_pos);
            }
            list_push_back (&hash_delete_list, &sup_pte->delete_elem);
          }
        }//while

        struct list_elem *d_elem;

        d_elem = list_begin (&hash_delete_list);
        while (1)
          {
            if (d_elem == list_end (&hash_delete_list)) {
              break;
            }
            struct sup_page_entry *spte = list_entry (d_elem, struct sup_page_entry, delete_elem);
            d_elem = list_next (d_elem);
            list_remove(&spte->delete_elem);
            frame_table_free(spte->kpage);
            sup_page_entry_delete(spte);
          }


        list_remove(&f->mapping_elem);
        break;
      }
    }
}

static void handle_chdir (struct intr_frame *f) {
  struct dir *changed;
  const char *dir = *(char **)(f->esp + 4);
  changed = dir_open_path(dir);
  if (changed == NULL) {
    f->eax = (uint32_t)false;
  } else {
    dir_close(thread_current ()->curr_dir);
    thread_current ()->curr_dir = dir_open_path(dir);
    f->eax = (uint32_t)true;
  }
}

static void handle_mkdir (struct intr_frame *f) {
  char *new_dir;
  const char *dir = *(char **)(f->esp + 4);
  char *dir_path, *parent;
  struct inode *temp_inode;
  strlcpy(dir_path, dir, strlen(dir)+1); // replace to max path size.
  new_dir = dir_split_name(dir_path);
  if (strcmp(new_dir, dir_path) != 0) {
    dir = dir_open_path(dir_path);
  } else if (!thread_current()->curr_dir){
    thread_current()->curr_dir = dir_open_root();
    dir = dir_reopen(thread_current()->curr_dir);
  } else {
    dir = dir_reopen(thread_current()->curr_dir);
  }
  disk_sector_t inode_sector = 0;
  bool success = (
    parent != NULL &&
    free_map_allocate (1, &inode_sector) &&
    inode_create (inode_sector, 16 * dir_size_entry())
  );
  if (success) {
    inode_set_directory(inode_open(inode_sector), true);
    f->eax = (uint32_t) success && (
      dir_add (parent, new_dir, inode_sector) &&
      dir_add(parent, ".", inode_sector) &&
      dir_add(parent, "..", inode_get_inumber (dir_get_inode (parent)))
    );
  } else {
    f->eax = (uint32_t)false;
  }
  dir_close(parent);
  f->eax = (uint32_t)success;
}

static void handle_readdir (struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  char *name = *(char **)(f->esp + 8);
  struct file *file = find_file_by_fd(fd);
  struct dir *dir;
  if (!inode_is_directory(file_get_inode (file))) {
    f->eax = (uint32_t) false;
    return;
  }
  dir = dir_open(file->inode);
  f->eax = (uint32_t) dir_readdir(dir, name);
  dir_close(dir);
}

static void handle_isdir (struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  f->eax = (uint32_t) inode_is_directory(file_get_inode(find_file_by_fd(fd)));
}

static void handle_inumber (struct intr_frame *f) {
  int fd = *(int *)(f->esp + 4);
  f->eax = (uint32_t) inode_get_inumber(file_get_inode(find_file_by_fd(fd)));
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
      handle_exit(f);
    } else if (syscall_number == SYS_WRITE) {
      handle_write(f);
    } else if (syscall_number == SYS_HALT) {
      handle_halt(f);
    } else if (syscall_number == SYS_CREATE) {
      handle_create(f);
    } else if (syscall_number == SYS_OPEN) {
      handle_open(f);
    } else if (syscall_number == SYS_REMOVE) {
      handle_remove(f);
    } else if (syscall_number == SYS_CLOSE) {
      handle_close(f);
    } else if (syscall_number == SYS_READ) {
      handle_read(f);
    } else if (syscall_number == SYS_FILESIZE) {
      handle_filesize(f);
    } else if (syscall_number == SYS_EXEC) {
      handle_exec(f);
    } else if (syscall_number == SYS_WAIT) {
      handle_wait(f);
    } else if (syscall_number == SYS_SEEK) {
      handle_seek(f);
    } else if (syscall_number == SYS_TELL) {
      handle_tell(f);
    } else if (syscall_number == SYS_MMAP) {
      handle_mmap(f);
    } else if (syscall_number == SYS_MUNMAP) {
      handle_munmap(f);
    }
  }
}
