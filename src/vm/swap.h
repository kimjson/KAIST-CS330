#include "page.h"
#include "devices/disk.h"
#include <list.h>
#include <threads/malloc.h>
#include <threads/thread.h>
#include <userprog/pagedir.h>
#include <threads/pte.h>
#include "threads/synch.h"

#ifndef _SWAP_H_
#define _SWAP_H_

struct swap_entry {
  bool is_used;
  disk_sector_t first_sec_no;
  struct list_elem list_elem;
  void *upage;
};

void swap_init (void);
void *swap_in (struct sup_page_entry *sup_pte, bool writable);
struct swap_entry *get_swap_slot(void);
struct swap_entry *find_swap_slot(disk_sector_t sec_no);

#endif
