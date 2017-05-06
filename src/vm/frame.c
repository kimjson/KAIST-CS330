#include "frame.h"

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

    // physical memory full, swap out
    // Find a victim page(frame).
    struct frame_entry *victim_f = list_entry(list_pop_front(&frame_table), struct frame_entry, list_elem);

    // Find an empty slot in swap disk.
    struct swap_entry *empty_slot = get_swap_slot();
    swap_out(victim_f, empty_slot);
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

struct swap_entry *swap_out (struct frame_entry *f, struct swap_entry *empty_slot) {
  struct hash sup_page_table = f->using_thread->sup_page_table;
  struct sup_page_entry *sup_pte = sup_page_table_lookup(&sup_page_table, pte_get_page (*f->pte));
  if (pagedir_is_dirty(f->using_thread->pagedir, f->upage)) {
    struct disk *swap_disk = disk_get(1,1);
    if (swap_disk != NULL) {
      // list iterate and find empty swap slot.
      disk_write(swap_disk, empty_slot->first_sec_no, f->kpage);
      empty_slot->is_used = true;
      sup_pte->first_sec_no = empty_slot->first_sec_no;
    }
  }
  // update supplementary page table using pte.h static inline void *pte_get_page (uint32_t pte)
  sup_pte->kpage = NULL;
  *f->pte &= 0xfffffffe;
  palloc_free_page(f->kpage);
  free(f);
}

void *swap_in (struct sup_page_entry *sup_pte, bool writable) {
  void *kpage = frame_table_allocator(PAL_USER);
  struct disk *swap_disk = disk_get(1,1);
  if (swap_disk != NULL) {
    lock_acquire(&swap_lock);
    struct list_elem *e;
    for (e = list_begin (&swap_table); e != list_end (&swap_table);
         e = list_next (e))
    {
      struct swap_entry *se = list_entry (e, struct swap_entry, list_elem);
      if (se->first_sec_no == sup_pte->first_sec_no) {
        disk_read(swap_disk, sup_pte->swap_address->first_sec_no, kpage);
        sup_pte->swap_address->is_used = false;
      }
    }
    lock_release(&swap_lock);
    pagedir_set_page(thread_current()->pagedir, sup_pte->upage, kpage, writable);
    return kpage;
  }
  return NULL;
}
