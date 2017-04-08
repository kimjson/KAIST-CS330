#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include <stdbool.h>
#include <list.h>
#include "filesys/off_t.h"

struct inode;

/* An open file. */
struct file
{
  int fd;                     /* File descriptor */
  struct list_elem elem;      /* List elem for file_list(global list) */
  struct list_elem elem_for_thread;      /* List elem for thread->file_list(global list) */
  struct inode *inode;        /* File's inode. */
  off_t pos;                  /* Current position. */
  bool deny_write;            /* Has file_deny_write() been called? */
};

struct file_name {
  char name[16];
  struct list_elem elem;
};

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

#endif /* filesys/file.h */
