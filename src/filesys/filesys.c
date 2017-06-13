#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"
#include "filesys/cache.c"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
 // printf("filesys init\n");
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");


  cache_init();
  inode_init();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();


}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{

  cache_close();
  free_map_close ();
  //close all the directories and their entries.

}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  // printf("filesys create\n");
  disk_sector_t inode_sector = 0;
  struct dir *dir;
  char *copied_name = (char *)malloc(128);
  char *file_name;
  strlcpy (copied_name, name, strlen(name)+1);
  file_name = dir_split_name(copied_name);
  if (strcmp(file_name, copied_name) != 0) {
    dir = dir_open_path(copied_name);
  } else if (!thread_current()->curr_dir){
    thread_current()->curr_dir = dir_open_root();
    dir = dir_reopen(thread_current()->curr_dir);
  } else {
    dir = dir_reopen(thread_current()->curr_dir);
  }
  // printf("copied_name: %s\n", copied_name);
  // printf("file_name: %s\n", file_name);
  // struct dir *dir = dir_open_root ();
  bool success = (
    dir != NULL &&
    free_map_allocate (1, &inode_sector) &&
    inode_create (inode_sector, initial_size) &&
    dir_add (dir, file_name, inode_sector)
  );
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free(copied_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *copied_name = malloc(strlen(name)+1);
  const char *file_name;
  struct dir *dir;
  struct file *open_file;
  struct inode *inode = NULL;
  strlcpy (copied_name, name, strlen(name)+1);
  file_name = dir_split_name(copied_name);
  printf("copied_name: %s\n", copied_name);
  printf("file_name: %s\n", file_name);
  if (strcmp(file_name, copied_name) != 0) {
    dir = dir_open_path(copied_name);
  } else if (!thread_current()->curr_dir){
    thread_current()->curr_dir = dir_open_root();
    dir = dir_reopen(thread_current()->curr_dir);
  } else {
    dir = dir_reopen(thread_current()->curr_dir);
  }
  printf("name of current working directory of thread: \n");
  dir_print_name(thread_current()->curr_dir);
  printf("name of new working directory: \n");
  dir_print_name(dir);
  // printf("copied_name: %s\n", copied_name);
  // printf("file_name: %s\n", file_name);
  printf("dir: 0x%08x\n", dir);
  if (dir != NULL) {
    // dir_lookup (dir_open_root(), file_name, &inode);
    dir_lookup (dir, file_name, &inode);
    printf("inode: 0x%08x\n", inode);
    dir_close(dir);
    open_file = file_open(inode);
    // printf("open_file: 0x%08x\n", open_file);
    free(copied_name);
    return open_file;
  } else {
    free(copied_name);
    return NULL;
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  char *copied_name = (char *)malloc(128);
  const char *file_name;
  struct dir *dir;
  struct dir *dir_file;
  bool success;
  struct inode *inode = NULL;

  strlcpy (copied_name, name, strlen(name)+1);
  file_name = dir_split_name(copied_name);
  if (strcmp(file_name, copied_name) != 0) {
    dir = dir_open_path(copied_name);
  } else if (!thread_current()->curr_dir){
    thread_current()->curr_dir = dir_open_root();
    dir = dir_reopen(thread_current()->curr_dir);
  } else {
    dir = dir_reopen(thread_current()->curr_dir);
  }
  // if file is directory and not empty, fails.
  success = dir_lookup (dir, file_name, &inode);
  if (!inode_is_directory(inode)) {
    success = success && dir_remove(dir, file_name);
    dir_close(dir);
    return success;
  }
  dir_file = dir_open(inode);
  if (dir_empty(dir_file)) {
    success = success && dir_remove(dir, file_name);
    dir_close(dir_file);
    dir_close(dir);
    return success;
  }
  dir_close(dir);
  return false;
}

/* Formats the file system. */
static void
do_format (void)
{
   // printf("do format\n");

  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16, NULL, "/"))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
