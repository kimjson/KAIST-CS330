#include "frame.h"
#include "swap.h"

struct lock frame_lock;
struct list frame_table;

void
frame_init (void) {

  list_init(&frame_table);
  lock_init(&frame_lock);

}

void *
frame_table_allocator (enum palloc_flags flags)
{
  void *result = palloc_get_multiple (flags, 1);

  if (result == NULL) {
    // physical memory full, swap in(?) and out
    struct frame_entry *victim_f = list_entry(list_pop_front(&frame_table), struct frame_entry, list_elem);
    swap_out(victim_f);
    result = palloc_get_multiple(flags, 1);
  }
  struct frame_entry *f = (struct frame_entry *)malloc(sizeof(struct frame_entry));
  f->using_thread = thread_current();
  f->kpage = result;
  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &f->list_elem);
  lock_release(&frame_lock);

  return result;
}

void
frame_table_free (void *page) {
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
  {
    struct frame_entry *f = list_entry (e, struct frame_entry, list_elem);
    if (f->kpage == page) {
      list_remove(&f->list_elem);
      palloc_free_multiple(page, 1);
      free(f);
      return;
    }
  }
}

void
frame_entry_set_pte (void *kpage, uint32_t *pte) {
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
  {
    struct frame_entry *f = list_entry (e, struct frame_entry, list_elem);
    if (f->kpage == kpage) {
//      *pte = *pte | PTE_P;
      f->pte = pte;
      return;
    }
  }
}
