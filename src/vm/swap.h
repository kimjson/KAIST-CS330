#ifndef _SWAP_H_
#define _SWAP_H_

#include "devices/disk.h"
#include "vm/frame.h"
#include <list.h>

struct swap_entry {
  bool is_used;
  disk_sector_t first_sec_no;
  struct list_elem list_elem;
};

void swap_init (void);
void swap_out (struct frame_entry *f);
void swap_in (struct sup_page_entry *sup_pte, bool writable);
// struct swap_entry *find_swap_by_upage(void *upage);

#endif
