#include <list.h>
#include "threads/palloc.h"

struct frame_entry {
  struct list_elem list_elem;
  struct thread *using_thread;
  uint32_t *pte;
  void *kpage;
};

void frame_init (void);
void *frame_table_allocator (enum palloc_flags flags);
void frame_table_free (void *page);
void frame_entry_set_pte (void *kpage, uint32_t *pte);
