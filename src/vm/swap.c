#include <threads/malloc.h>
#include <threads/thread.h>
#include <userprog/pagedir.h>
#include <threads/pte.h>
#include "swap.h"
#include "threads/synch.h"
#include "page.h"
#include <stdio.h>

struct list swap_table;
struct lock swap_lock;

void swap_init (void) {

  disk_sector_t i;
  struct disk *swap_disk = disk_get(1,1);
  lock_init(&swap_lock);
  list_init(&swap_table);
  for (i = 0; i < disk_size(swap_disk); i += 8) {
    struct swap_entry *se = (struct swap_entry *) malloc(sizeof(struct swap_entry));
    se->is_used = false;
    se->first_sec_no = i;
  }
  return;
}

void swap_out (struct frame_entry *f) {
  struct hash sup_page_table = f->using_thread->sup_page_table;
  struct sup_page_entry *sup_pte = sup_page_table_lookup(&sup_page_table, pte_get_page (*f->pte));
  if (pagedir_is_dirty(f->using_thread->pagedir, f->upage)) {
    struct disk *swap_disk = disk_get(1,1);
    if (swap_disk != NULL) {
      // list iterate and find empty swap slot.
      lock_acquire(&swap_lock);
      struct list_elem *e;
      for (e = list_begin (&swap_table); e != list_end (&swap_table);
           e = list_next (e))
      {
        struct swap_entry *se = list_entry (e, struct swap_entry, list_elem);
        if (!se->is_used) {
          disk_write(swap_disk, se->first_sec_no, f->kpage);
          se->is_used = true;
          sup_pte->swap_address = se;
          break;
        }
      }
      lock_release(&swap_lock);
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
    disk_read(swap_disk, sup_pte->swap_address->first_sec_no, kpage);
    sup_pte->swap_address->is_used = false;
    lock_release(&swap_lock);
    pagedir_set_page(thread_current()->pagedir, sup_pte->upage, kpage, writable);
    return kpage;
  }
  return NULL;
}
