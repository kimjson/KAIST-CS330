#include <list.h>
#include "threads/palloc.h"
#include <threads/pte.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "devices/disk.h"
#include "swap.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

struct frame_entry {
  struct list_elem list_elem;
  struct thread *using_thread;
  uint32_t *pte;
  void *kpage;
  void *upage;
};

void frame_init (void);
void *frame_table_allocator (enum palloc_flags flags);
void frame_table_free (void *page);
void frame_entry_set_pte (void *kpage, uint32_t *pte);
