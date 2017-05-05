#include <threads/malloc.h>
#include <threads/thread.h>
#include "swap.h"
#include "threads/synch.h"

struct list swap_table;
struct lock swap_lock;

void swap_init (void) {

  struct disk *swap_disk = disk_get(1,1);
  lock_init(&swap_lock);
  list_init(&swap_table);
  for (disk_sector_t i = 0; i < swap_disk->capacity; i += 8) {
    struct swap_entry *se = (struct swap_entry *) malloc(sizeof(struct swap_entry));
    se->is_used = false;
    se->first_sec_no = i;
  }
  return;
}

struct swap_entry *swap_out (struct frame_entry *f) {
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
        return se;
      }
    }
    lock_release(&swap_lock);
  }
  return NULL;
}

void *swap_in (void) {

}

