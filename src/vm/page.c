#include "page.h"

//struct hash page_table;
//
void page_init (void) {

  return;

}

unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED) {

  const struct page *p = hash_entry(p_, struct page, hash_elem);
  return hash_bytes(&p->addr, sizeof p->addr);

}

bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {

  const struct page *a = hash_entry(a_, struct page, hash_elem);
  const struct page *b = hash_entry(b_, struct page, hash_elem);

  return a->addr < b->addr;
}