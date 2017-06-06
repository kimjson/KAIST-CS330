#ifndef _CACHE_H_
#define _CACHE_H_

#include "devices/disk.h"
#include <list.h>

struct cache_block {
  disk_sector_t disk_sector_id;
  bool is_dirty;
  // bool is_valid;
  bool is_exclusive;
  int num_readers;
  int num_writers;
  int num_requests;
  struct semaphore sema;
  struct list_elem elem;
  char data[DISK_SECTOR_SIZE];
};

void cache_init(void);
struct cache_block *cache_block_get(disk_sector_t sector, bool exclusive);
void cache_block_put(struct cache_block *b);
void *cache_block_read(struct cache_block *b);
void *cache_block_zero(struct cache_block *b);
void cache_block_set_dirty(struct cache_block *b);

#endif
