#include <threads/malloc.h>
#include "swap.h"

struct list swap_table;
disk_sector_t last_sec_no;

void swap_init (void) {

  struct disk *swap_disk = disk_get(1,1);
  list_init(&swap_table);
  for (disk_sector_t i = 0; i < swap_disk->capacity; i += 8) {
    struct swap_entry *se = (struct swap_entry *) malloc(sizeof(struct swap_entry));
    se->is_used = false;
    se->first_sec_no = i;
  }
  last_sec_no = 0;
  return;
}

bool swap_out (struct frame_entry *f) {
  struct disk *swap_disk = disk_get(1,1)
  if (swap_disk == NULL) {
    return false;
  } else {
    // list iterate and find empty swap slot.
    struct list_elem *e;
    for (e = list_begin (&swap_table); e != list_end (&swap_table);
         e = list_next (e))
    {
      struct swap_entry *se = list_entry (e, struct swap_entry, list_elem);
      if (se->is_used) {
        continue;
      } else {
        
      }
    }
  }
}

