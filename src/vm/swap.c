#include <threads/malloc.h>
#include <threads/thread.h>
#include <userprog/pagedir.h>
#include <threads/pte.h>
#include "swap.h"
#include "threads/synch.h"
#include "page.h"
#include <stdio.h>

struct list swap_table;
struct lock swap_sema;

void swap_init (void) {

  disk_sector_t i;
  struct disk *swap_disk = disk_get(1,1);
  sema_init(&swap_sema, 1);
  sema_down(&swap_sema);
  list_init(&swap_table);
  for (i = 0; i < disk_size(swap_disk); i += 8) {
    struct swap_entry *se = (struct swap_entry *) malloc(sizeof(struct swap_entry));
    se->is_used = false;
    se->first_sec_no = i;
    list_push_back(&swap_table, &se->list_elem);
  }
  sema_up(&swap_sema);
}

void swap_out (struct frame_entry *f) {

  // sema_down(&swap_sema);
  struct sup_page_entry *sup_pte = sup_page_table_lookup(&f->using_thread->sup_page_table, f->upage);

  struct disk *swap_disk = disk_get(1,1);
  if (swap_disk != NULL) {
    // list iterate and find empty swap slot.
    struct list_elem *e;
    sema_down(&swap_sema);

    for (e = list_begin (&swap_table); e != list_end (&swap_table);
         e = list_next (e))
    {
      struct swap_entry *se = list_entry (e, struct swap_entry, list_elem);
      if (!se->is_used) {
        disk_sector_t i;
        for(i=0;i<8;i++)
          {
            disk_write(swap_disk, se->first_sec_no+i, (uint8_t *)(f->kpage) + i*DISK_SECTOR_SIZE);
          }
        se->is_used = true;
        sup_pte->swap_address = se;
        sup_pte->fault_case = CASE_SWAP;
        break;
      }
    }
    sema_up(&swap_sema);
    // update supplementary page table using pte.h static inline void *pte_get_page (uint32_t pte)
    pagedir_clear_page(f->using_thread->pagedir, f->upage);
    palloc_free_page(f->kpage);
    free(f);
  }


}

void swap_in (struct sup_page_entry *sup_pte, bool writable) {

  void *kpage = frame_table_allocator(PAL_USER);


  struct disk *swap_disk = disk_get(1,1);
  if (swap_disk != NULL) {
    disk_sector_t i;
    sema_down(&swap_sema);
    for(i=0;i<8;i++)
      {
        disk_read(swap_disk, sup_pte->swap_address->first_sec_no + i, (uint8_t *)kpage+i*DISK_SECTOR_SIZE);
      }
    sup_pte->swap_address->is_used = false;
    sup_pte->fault_case = CASE_ZERO;
    sema_up(&swap_sema);
  }
  pagedir_set_page(thread_current()->pagedir, sup_pte->upage, kpage, writable);

}
