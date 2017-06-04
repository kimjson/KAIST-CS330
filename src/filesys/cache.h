#ifndef _cache_H_
#define _cache_H_


#include <list.h>
#include "threads/palloc.h"
#include <threads/pte.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "devices/disk.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
//#include "vm/swap.h"

struct cache_entry {
  bool is_dirty;
  disk_sector_t first_sec_no;
  char block[512];
  struct list_elem list_elem;
};

void cache_init (void);
void cache_out (struct cahce_entry *c);
void cache_in (struct _entry *sup_pte, bool writable);
bool cache_find();
struct cache_entry* cache_lookup(struct cache_entry *target_entry);
// struct swap_entry *find_swap_by_upage(void *upage);

#endif
