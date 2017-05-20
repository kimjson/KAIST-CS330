#include "frame.h"
#include "swap.h"

struct semaphore frame_sema;
struct list frame_table;

void
frame_init (void) {

  list_init(&frame_table);
  sema_init(&frame_sema, 1);

}

void *
frame_table_allocator (enum palloc_flags flags)
{
  sema_down(&frame_sema);
  void *result = palloc_get_multiple (flags, 1);
  if (result == NULL) {
    // physical memory full, swap in(?) and out
    struct frame_entry *victim_f = list_entry(list_pop_front(&frame_table), struct frame_entry, list_elem);
    struct sup_page_entry *sup_pte = sup_page_table_lookup(&victim_f->using_thread->sup_page_table, victim_f->upage);

    if (sup_pte->file_address == NULL) {
      swap_out(victim_f);
    } else {
      if (pagedir_is_dirty (victim_f->using_thread->pagedir, victim_f->upage)) {
        file_write_at(sup_pte->file_address, victim_f->kpage, PGSIZE, sup_pte->file_pos);
      }
      pagedir_clear_page(victim_f->using_thread->pagedir, victim_f->upage);
      palloc_free_page(victim_f->kpage);
      free(victim_f);
    }
    result = palloc_get_multiple(flags, 1);
  }
  struct frame_entry *f = (struct frame_entry *)malloc(sizeof(struct frame_entry));
  f->using_thread = thread_current();
  f->kpage = result;
  list_push_back(&frame_table, &f->list_elem);
  sema_up(&frame_sema);
  return result;
}

void
frame_table_free (void *page) {
  sema_down(&frame_sema);
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
  {
    struct frame_entry *f = list_entry (e, struct frame_entry, list_elem);
    if (f->kpage == page) {
      list_remove(&f->list_elem);
      palloc_free_multiple(page, 1);
      free(f);
      break;
    }
  }
  sema_up(&frame_sema);
}

void
frame_entry_set_pte (void *upage, void *kpage, uint32_t *pte) {
  sema_down(&frame_sema);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
  {
    struct frame_entry *f = list_entry (e, struct frame_entry, list_elem);
    if (f->kpage == kpage) {
      f->pte = pte;
      f->upage = upage;
      break;
    }
  }
  sema_up(&frame_sema);
}

void
free_frames(struct thread *exiting_thread) {
  sema_down(&frame_sema);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
  {
    struct frame_entry *f = list_entry (e, struct frame_entry, list_elem);
    if (f->using_thread == exiting_thread) {
      list_remove(&f->list_elem);
      palloc_free_multiple(f->kpage, 1);
      free(f);
    }
  }
  sema_up(&frame_sema);
}
