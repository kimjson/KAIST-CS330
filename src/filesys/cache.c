#include "cache.h"

struct list cache_list;
struct semaphore cache_sema;

void cache_init() {
  sema_init(&cache_sema, 1);
  list_init(&cache_list);
}

struct cache_block *
cache_block_get(disk_sector_t sector, bool exclusive) {
  if (list_size(&cache_list) == 64) {
    sema_down(&cache_sema);
    struct cache_block *victim_cache
    = list_entry(list_pop_front(&cache_list), struct cache_block, elem);
    sema_up(&cache_sema);
    // write behind
  }
  struct cache_block *new_cache
  = (struct cache_block *)malloc(sizeof(struct cache_block));
  new_cache->is_dirty = false;
  new_cache->is_exclusive = exclusive;
  new_cache->num_readers = 0;
  new_cache->num_writers = 0;
  new_cache->num_requests = 0;
  sema_init(&new_cache->sema, 0);
  sema_down(&cache_sema);
  list_push_back(&cache_list, &new_cache->elem);
  sema_up(&cache_sema);
  return new_cache;
}

void cache_block_put(struct cache_block *b) {
  sema_up(&b->sema);
}

void *cache_block_read(struct cache_block *b) {
  disk_read (filesys_disk, b->disk_sector_id, b->data);
  return b->data;
}
