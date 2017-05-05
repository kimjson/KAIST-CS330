#include "frame.h"
#include "page.h"
#include "devices/disk.h"
#include <list.h>

struct swap_entry {
  bool is_used;
  disk_sector_t first_sec_no;
  struct list_elem list_elem;
};

void swap_init (void);
bool swap_out (struct frame_entry *f);