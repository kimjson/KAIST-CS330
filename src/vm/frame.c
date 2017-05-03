#include "frame.h"
#include "threads/palloc.h"

struct list frame_table;
struct lock frame_lock;

void
frame_init (void) {

  list_init(&frame_table);
  lock_init(&frame_lock);

}

void *
frame_table_allocator (enum palloc_flags)
{
  void *result = palloc_get_multiple (flags, 1);

  struct frame *f = malloc(sizeof(struct frame));
  f->using_thread = thread_current();
  f->page = result;
  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &f->list_elem);
  lock_release(&frame_lock);

  return result;
}