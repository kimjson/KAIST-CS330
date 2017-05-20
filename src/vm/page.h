#ifndef _PAGE_H_
#define _PAGE_H_
#include "threads/malloc.h"
#include "threads/thread.h"
#include <hash.h>
#include <lib/debug.h>
#include <devices/disk.h>

enum fault_case
{
 CASE_SWAP,
 CASE_FILESYS,
 CASE_ZERO
};

struct sup_page_entry {
  struct hash_elem hash_elem;
  void *upage;                // user virtual address
  void *kpage;                // kernel virtual address. physical frame is identified by this.
  bool is_valid;
  enum fault_case fault_case;             // 0: case_swap, 1: case_filesys, 2: case_zero
  struct swap_entry *swap_address;
  struct file *file_address;
  off_t file_pos;
};

void page_init (void);
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct sup_page_entry * sup_page_entry_create (void *upage, void *kpage, struct file *file);
struct sup_page_entry * sup_page_table_lookup (struct hash *sup_page_table, const void *upage);
struct sup_page_entry * sup_page_table_file_lookup (struct hash *sup_page_table, struct file *file);
void sup_page_entry_delete(struct sup_page_entry* sup_pte);
#endif
