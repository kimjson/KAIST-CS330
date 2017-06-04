#include "cache.h"
#include <stdio.h>

struct semaphore cache_sema;
struct list cache;

void
cache_init(void){
	list_init(&cache);
	sema_init(&cache_sema, 1);
}

/*
	in this function, cache get a block from the disk. If cache is full, evict a block by FIFO, and get a block from the disk. 
*/
void
cache_in(disk_sector_t first_sec_no){
	sema_down(&cache_sema);
	/* check cache_length*/
	struct disk *swap_disk = disk_get(1,1);

	if(list_size(&cache)>64){
		struct cache_entry *victim_C = list_entry(list_pop_front(&cache),struct cache_entry, list_elem);
		cache_out(victim_c);
	}

	sema_down(&swap_sema);
	struct cache_entry *ce = (struct cache_entry *) malloc(sizeof(struct cache_entry));
	ce->first_sec_no = first_sec_no;
	ce->is_dirty = false;
	disk_read(swap_disk, first_sec_no, (char *)ce->block);
	list_push_back(&cache, &ce->list_elem);
	sema_up(&swap_sema);
	sema_up(&cache_sema);

}

void
cahce_out(struct cache_entry *c){

	struct disk *swap_disk = disk_get(1,1);

}

struct cache_entry*
cache_lookup(struct cache_entry *target_entry){
	struct cache_entry *return_cache;
	sema_down(&cache_sema);
	struct list_elem *e;
	for(e = list_begin(&cache); e != list_end(&frame_table); e= list_next(e)){
		struct cache_entry *c = list_entry(e, struct cache_entry, list_elem);
		if(c->first_sec_no == target_entry->first_sec_no){
			return_cache = cache_entry;
			break;
		}

	}
	sema_up(&cache_sema);
	return return_cache;
}

