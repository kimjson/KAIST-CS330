#include "cache.h"
#include <stdio.h>
#include "filesys.h"

struct semaphore cache_sema;
struct list cache;


void
cache_init(void){
	//printf("cache-init\n");
	list_init(&cache);
	sema_init(&cache_sema, 1);
}

/*
	in this function, cache get a block from the disk. If cache is full, evict a block by FIFO, and get a block from the disk.
*/
struct cache_entry *
cache_in(disk_sector_t first_sec_no){
	sema_down(&cache_sema);
	/* check cache_length*/
	struct cache_entry *ce;
	//printf("first_sec_no:%d\n",first_sec_no);
	struct cache_entry* target_ce = cache_lookup(first_sec_no,true);
	//printf("flag6");
	if(target_ce!=NULL){
		ce = target_ce;
	}
	else{
		//printf("flag7");
		if(list_size(&cache)>=64){
			struct cache_entry* victim_Cache = list_entry(list_pop_front(&cache),struct cache_entry, list_elem);
			cache_out(victim_Cache);
		}//
		//printf("flag8");

		ce = (struct cache_entry *) malloc(sizeof(struct cache_entry));
		ce->first_sec_no = first_sec_no;
		ce->is_dirty = false;
		disk_read(filesys_disk, first_sec_no, (char *)ce->block);
		list_push_back(&cache, &ce->list_elem);
		
	}
	sema_up(&cache_sema);
	return ce;
}

void
cache_out(struct cache_entry *c){
	//printf("cahce out\n");
	if (c->is_dirty) {
		// write to disk
		disk_write(filesys_disk, c->first_sec_no, (char *)c->block);
	}
	list_remove(&c->list_elem);
	free(c);
}

struct cache_entry*
cache_lookup(disk_sector_t sec_no, bool cache_in){
	struct cache_entry *return_cache = NULL;
	//printf("lookup\n");
	if(!cache_in)
	{ 
		sema_down(&cache_sema);
	}
	struct list_elem *e;
	for(e=list_begin(&cache); e != list_end(&cache); e=list_next(e)){
		struct cache_entry *target_entry = list_entry(e, struct cache_entry, list_elem);
		if(sec_no == target_entry->first_sec_no){
			//printf("hit\n");
			return_cache = target_entry;
			break;
		}

	}
	if(!cache_in)
	{ 
		sema_up(&cache_sema);
	}
	//printf("lookup end\n");
	return return_cache;
}


