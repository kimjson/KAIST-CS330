#include <hash.h>
#include <lib/debug.h>

struct sup_page_entry {
  struct hash_elem hash_elem;
  void *upage;                // user virtual address
  void *kpage;                // kernel virtual address. physical frame is identified by this.
  bool is_valid;
};

void page_init (void);
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void sup_page_entry_create (void *upage, void *kpage);
struct sup_page_entry * sup_page_table_lookup (struct hash *sup_page_table, const void *upage);
