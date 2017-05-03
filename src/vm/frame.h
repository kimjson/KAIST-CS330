#include <list.h>

struct frame {
  struct list_elem list_elem;
  struct thread *using_thread;
  uint32_t pte;
  void *page;
};

void frame_init (void);
void *frame_table_allocator (enum palloc_flags);
