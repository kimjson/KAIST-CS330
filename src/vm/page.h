#include <hash.h>
#include <debug.h>

struct page {
  struct hash_elem hash_elem;
  void *addr;
};

void page_init (void);
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);