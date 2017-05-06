#include "swap.h"

struct list swap_table;
struct lock swap_lock;

void swap_init (void) {

  disk_sector_t i;
  struct disk *swap_disk = disk_get(1,1);
  lock_init(&swap_lock);
  list_init(&swap_table);
  for (i = 8; i < swap_disk->capacity; i += 8) {
    struct swap_entry *se = (struct swap_entry *) malloc(sizeof(struct swap_entry));
    se->is_used = false;
    se->first_sec_no = i;
  }
  return;
}

struct swap_entry *get_swap_slot(void) {
  // list iterate and find empty swap slot.
  lock_acquire(&swap_lock);
  struct list_elem *e;
  for (e = list_begin (&swap_table); e != list_end (&swap_table);
       e = list_next (e))
  {
    struct swap_entry *se = list_entry (e, struct swap_entry, list_elem);
    if (!se->is_used) {
      return se;
    }
  }
  return 0;
}

struct swap_entry *find_swap_slot(disk_sector_t sec_no) {
  // list iterate and find empty swap slot.
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
}
