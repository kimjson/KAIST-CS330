#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //disk_sector_t start;                /* First data sector. */
    off_t length;                       /* File size in bytes. */

    disk_sector_t direct_block[12];
    disk_sector_t indirect_block;
    disk_sector_t double_indirect_block;
    unsigned magic;                     /* Magic number. */
    bool is_directory;
    uint32_t unused[111];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    //off_t length;
    //struct inode_disk data;             /* Inode content. */
    bool is_directory;
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */

disk_sector_t
inode_byte_to_sector(struct inode *inode, off_t pos)
{
    int offset = pos/DISK_SECTOR_SIZE;
    struct inode_disk *id;

    struct cache_entry *target_ce = cache_lookup(inode->sector,false);
    if(target_ce==NULL){
      target_ce = cache_in(inode->sector);
    }
    id = (struct inode_disk *)target_ce->block;

 //   printf("ibytc, sector:%d\n",inode->sector);
   // printf("ibytc, offset :%d\n",offset);



    if(offset<12){
      //  printf("direct\n");
      //  printf("return sector:%d\n",id->direct_block[offset]);
        return id->direct_block[offset];
    }
    else if(offset>=12 && offset <140){
        //indirect block
        //printf("offset:%d\n",offset);
        //printf("print indirect:%d\n",id->indirect_block);
      //  printf("offset:%d\n",offset);
        //printf("offset:%d\n",offset);
      //  printf("block->inidrectblock:%d\n",id->indirect_block);
        target_ce = cache_lookup(id->indirect_block,false);
        if(target_ce == NULL){
          //cache miss
          target_ce = cache_in(id->indirect_block);
        }
        //printf("return sector:%d\n",(disk_sector_t)(target_ce->block[4*(offset-12)]));

        // hex_dump (0,  &target_ce->block[4*(offset-12)], 12,true);
        disk_sector_t temp_sector;
        memcpy (&temp_sector, &target_ce->block[4*(offset-12)], 4);
        //printf("temp:%d\n",temp_sector);
        return temp_sector;

    }
    else{
      // printf("double\n");
      //double indirect block
      int index_no = (offset - 12)/128;
      int remainder = offset - 12 - 128*index_no;
      struct cache_entry *target_ce2 = cache_lookup(id->double_indirect_block,false);
      disk_sector_t target_sector_no;
      index_no = index_no - 1;
    //  printf("offset:%d\n",offset);

      if(target_ce2 == NULL){
          //cache miss
        target_ce2 = cache_in(id->double_indirect_block);
      }
      memcpy (&target_sector_no, &target_ce2->block[4*index_no], 4);

      // target_sector_no = target_ce2->block[4*index_no];
      struct cache_entry *target_ce3 = cache_lookup(target_sector_no,false);
      if(target_ce3==NULL){
        //cache miss
        target_ce3 = cache_in(target_sector_no);
      }

      disk_sector_t temp_sector;
      memcpy (&temp_sector, &target_ce3->block[4*remainder], 4);

      return temp_sector;


    }

}



/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  //printf("init\n");
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */

// bool
// inode_create (disk_sector_t sector)
// {
//   struct inode_disk *disk_inode = NULL;
//   //bool success = false;
//   // printf("inode create sector:%d\n",sector);
//   ASSERT (length >= 0);
//
//   /* If this assertion fails, the inode structure is not exactly
//      one sector in size, and you should fix that. */
//   ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);
//
//   disk_sector_t sec_no;
//   disk_inode = calloc (1, sizeof *disk_inode);
//   disk_inode->magic = INODE_MAGIC;
//   disk_inode->length = 0;
//   return true;
// }

bool
inode_extend(struct inode * inode, off_t offset, off_t size)
{
  off_t cur_length = inode_length(inode);



  if(cur_length<offset+size){

    size_t sectors = bytes_to_sectors(cur_length);
    size_t new_sectors = bytes_to_sectors (size+offset);
    int i;
    int count;
    disk_sector_t sec_no;
    struct inode_disk *disk_inode=NULL;
    static char zeros[DISK_SECTOR_SIZE];


    // printf()
    disk_inode = calloc (1, sizeof *disk_inode);

    //warning: free address can be disk inode

    struct cache_entry * target_ce = cache_lookup(inode->sector,false);
    if(target_ce == NULL){
      target_ce = cache_in(inode->sector);
    }

    memcpy(disk_inode, target_ce->block, DISK_SECTOR_SIZE);

    if(sectors==new_sectors){

      goto Done;
    }
    if(sectors>140){
      goto Double_indirect;
    }
    else if(sectors>12){
      goto Indirect;
    }


    count = 12;
    if(new_sectors<12){
      count = new_sectors;
    }
    for(i=sectors;i<count;i++){
      if(!free_map_allocate(1,&sec_no)){
        return false;
      }
      // hex_dump(0,disk_inode,100,false);
      disk_inode->direct_block[i] = sec_no;
      target_ce = cache_lookup(sec_no,false);
      if(target_ce==NULL){
        target_ce = cache_in(sec_no);
      }
      memcpy(target_ce->block,zeros,DISK_SECTOR_SIZE);
      target_ce->is_dirty=true;
    }

    if(new_sectors<=12){
      goto Done;
    }
    sectors=12;

    if(!free_map_allocate(1,&disk_inode->indirect_block)){
      return false;
    }


    Indirect: ;

    if(new_sectors<140){
      count = new_sectors-12;
    }else{
      count=128;
    }

    disk_sector_t indirect_no = disk_inode->indirect_block;
    struct cache_entry* indirect_index_block = cache_lookup(disk_inode->indirect_block,false);
    if(indirect_index_block==NULL){
        indirect_index_block=cache_in(disk_inode->indirect_block);
    }

    for(i=sectors-12;i<count;i++)
    {
        if(!free_map_allocate(1,&sec_no)){
          return false;
        }

        struct cache_entry* target_ce3 = cache_lookup(sec_no,false);
        if(target_ce3==NULL){
            target_ce3 = cache_in(sec_no);
         }
        memcpy(target_ce3->block,zeros,DISK_SECTOR_SIZE);
        target_ce3->is_dirty=true;

        indirect_index_block = cache_lookup(indirect_no,false);
        if(indirect_index_block==NULL){
            indirect_index_block = cache_in(indirect_no);
         }
        memcpy (&indirect_index_block->block[4*i],&sec_no, 4);
        indirect_index_block->is_dirty =true;
        //hex_dump (0, indirect_index_block->block, 4*i, true);
    }

    indirect_index_block->is_dirty=true;

    if(new_sectors<=140){
      goto Done;
    }

    sectors = 140;
    if(!free_map_allocate(1,&disk_inode->double_indirect_block)){
      return false;
    }


    Double_indirect: ;
    int start_index_no = (sectors-140)/128;
    int start_offset = (sectors-140)%128;

    count = new_sectors - 140;

    int temp_count;
    struct cache_entry* double_indirect_index_block = cache_lookup(disk_inode->double_indirect_block,false);
    if(double_indirect_index_block==NULL){
        double_indirect_index_block=cache_in(disk_inode->double_indirect_block);
    }

    int index_no = start_index_no;
    disk_sector_t double_index_no = disk_inode->double_indirect_block;


    while(1){

      temp_count = 128;
      if(count<128){temp_count = count;}

      free_map_allocate(1,&sec_no);


      struct cache_entry* target_ce5 = cache_lookup(sec_no,false);//index block
      if(target_ce5==NULL){
          target_ce5 = cache_in(sec_no);
      }


      double_indirect_index_block = cache_lookup(double_index_no, false);
      if(double_indirect_index_block==NULL){
        double_indirect_index_block = cache_in(double_index_no);
      }
      memcpy (&double_indirect_index_block->block[4*index_no],&sec_no, 4);


      double_indirect_index_block->is_dirty=true;

      disk_sector_t middle_sec_no=sec_no;

      for(i=start_offset;i<temp_count;i++)
      {
          if(!free_map_allocate(1,&sec_no)){
            //printf("allocate false33333\n");

            return false;}

          struct cache_entry* target_ce6 = cache_lookup(sec_no,false);
          if(target_ce6==NULL){
              target_ce6 = cache_in(sec_no);
           }
          memcpy(target_ce6->block,zeros,DISK_SECTOR_SIZE);
          target_ce6->is_dirty=true;

          target_ce5=cache_lookup(middle_sec_no, false);
          if(target_ce5==NULL){
            target_ce5 = cache_in(sec_no);
          }
          memcpy (&target_ce5->block[4*i],&sec_no, 4);
          target_ce5->is_dirty=true;
      }
      //target_ce5->is_dirty=true;
      index_no++;
      count=count-128;
      start_offset=0;


      if(count<=0){break;}
    }

    //printf("ssssssssssssssssssssssssssssssss\n");

    Done: ;
      disk_inode->length = size+offset;
      struct cache_entry * target_ce7 = cache_lookup(inode->sector,false);
      if(target_ce7==NULL){
        target_ce7=cache_in(inode->sector);
      }
      memcpy(target_ce7->block,disk_inode,DISK_SECTOR_SIZE);
      target_ce7->is_dirty=true;
      return true;

  }else{
    return true;
  }

}




bool
inode_create (disk_sector_t sector, off_t length,bool is_dir)
{
  // printf("print create\n");

  struct inode_disk *disk_inode = NULL;
  //bool success = false;


  // printf("inode create sector:%d\n",sector);
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);

  //printf("disk inode address:0x%08x\n",&disk_inode);

  size_t sectors = bytes_to_sectors (length);
  // printf("length:%d\n",length);
//  printf("sectors:%d\n",sectors);

  disk_inode->magic = INODE_MAGIC;
  disk_inode->length = length;
  disk_inode->is_directory = is_dir;

  if(sectors>0){
    int i;
    int count = 12;
    disk_sector_t sec_no;
    //struct cache_entry* target_ce;
    static char zeros[DISK_SECTOR_SIZE];

    //direct blocks
    if(sectors<12){
      count=sectors;
    }
    for(i=0;i<count;i++){
      if(!free_map_allocate(1,&sec_no)){
    //    printf("allocate false\n");
        return false;
      }
      disk_inode->direct_block[i] = sec_no;



      //printf("dixk:%d\n",disk_inode->direct_block[i]);
      struct cache_entry* target_ce = cache_lookup(sec_no,false);
      if(target_ce==NULL){
        target_ce = cache_in(sec_no);
      }
      memcpy(target_ce->block,zeros,DISK_SECTOR_SIZE);
      target_ce->is_dirty=true;
    }

    if(sectors<=12){
      struct cache_entry* target_ce2 = cache_lookup(sector,false);
      if(target_ce2==NULL){
        target_ce2=cache_in(sector);
      }
      memcpy(target_ce2->block,disk_inode,DISK_SECTOR_SIZE);
      target_ce2->is_dirty=true;
      return true;
    }

    //indirect blocks
    if(sectors<140){
      count = sectors-12;
    }else{
      count=128;
    }

    //printf("inter count:%d\n",count);
    if(!free_map_allocate(1,&sec_no)){
  //    printf("allocate111 false\n");

      return false;
    }

    //printf("indirect block number:%d\n",sec_no);

    disk_inode->indirect_block = sec_no;
    disk_sector_t indirect_no= sec_no;
    struct cache_entry* indirect_index_block = cache_lookup(disk_inode->indirect_block,false);
    if(indirect_index_block==NULL){
        indirect_index_block=cache_in(disk_inode->indirect_block);
    }
//    memcpy(indirect_index_block->block,zeros,DISK_SECTOR_SIZE);


  //  printf("indirect_index_block address:0x%08x\n",indirect_index_block->block);

    for(i=0;i<count;i++)
    {

        if(!free_map_allocate(1,&sec_no)){
          //printf("fllag11111111\n");
          return false;
        }




        struct cache_entry* target_ce3 = cache_lookup(sec_no,false);
        if(target_ce3==NULL){
            target_ce3 = cache_in(sec_no);
         }
        memcpy(target_ce3->block,zeros,DISK_SECTOR_SIZE);
        target_ce3->is_dirty=true;



        indirect_index_block = cache_lookup(indirect_no,false);
        if(indirect_index_block==NULL){
            indirect_index_block = cache_in(indirect_no);
         }
        memcpy (&indirect_index_block->block[4*i],&sec_no, 4);
        indirect_index_block->is_dirty =true;
    }
    // printf("\n\n");
    // printf("end\n\n");

    // hex_dump (0,indirect_index_block->block, 4*count, true);

    indirect_index_block->is_dirty=true;


    if(sectors<=140){
      struct cache_entry* target_ce4 = cache_lookup(sector,false);
      if(target_ce4==NULL){
        //printf("cache miss\n");
        target_ce4=cache_in(sector);
      }
      memcpy(target_ce4->block,disk_inode,DISK_SECTOR_SIZE);
      // printf("craeting secotr:%d\n",sector);
     // hex_dump(0,((struct inode_disk *)target_ce->block)->unused,448,true);
      target_ce4->is_dirty=true;
      return true;
    }



    //double indirect blocks
    count = sectors - 140;
    free_map_allocate(1,&sec_no);
    disk_inode->double_indirect_block = sec_no;
    int temp_count;

    struct cache_entry* double_indirect_index_block = cache_lookup(disk_inode->double_indirect_block,false);

    if(double_indirect_index_block==NULL){
        double_indirect_index_block=cache_in(disk_inode->double_indirect_block);
    }

    int index_no = 0;
    disk_sector_t double_index_no=sec_no;

    while(1){
      temp_count = 128;
      if(count<128){temp_count = count;}

      free_map_allocate(1,&sec_no);
      //double_indirect_index_block->block[4*index_no] = sec_no;

      //double_indirect_index_block->block[4*index_no] = sec_no;
      struct cache_entry* target_ce5 = cache_lookup(sec_no,false);//index block
      if(target_ce5==NULL){
          target_ce5 = cache_in(sec_no);
      }

      double_indirect_index_block = cache_lookup(double_index_no, false);
      if(double_indirect_index_block==NULL){
        double_indirect_index_block = cache_in(double_index_no);
      }
      memcpy (&double_indirect_index_block->block[4*index_no],&sec_no, 4);
      double_indirect_index_block->is_dirty=true;

      disk_sector_t middle_sec_no=sec_no;


      for(i=0;i<temp_count;i++)
      {
          if(!free_map_allocate(1,&sec_no)){
            //printf("allocate false33333\n");

            return false;}

          struct cache_entry* target_ce6 = cache_lookup(sec_no,false);
          if(target_ce6==NULL){
              target_ce6 = cache_in(sec_no);
           }
          memcpy(target_ce6->block,zeros,DISK_SECTOR_SIZE);
          target_ce6->is_dirty=true;

          target_ce5=cache_lookup(middle_sec_no, false);
          if(target_ce5==NULL){
            target_ce5 = cache_in(sec_no);
          }
          memcpy (&target_ce5->block[4*i],&sec_no, 4);
          target_ce5->is_dirty=true;
      }
      index_no++;
      count=count-128;
      if(count<=0){break;}
    }


    struct cache_entry * target_ce7 = cache_lookup(sector,false);
    if(target_ce7==NULL){
      target_ce7=cache_in(sector);
    }
    memcpy(target_ce7->block,disk_inode,DISK_SECTOR_SIZE);
    target_ce7->is_dirty=true;
    return true;
  }
  return true;
}

    //   if (free_map_allocate (sectors, &disk_inode->start))
    //     {

    //       //disk_write (filesys_disk, sector, disk_inode);

    //       // printf("sector:%d\n",sector);
    //       struct cache_entry* target_ce = cache_lookup(sector,false);


    //       if(target_ce!=NULL){
    //         //cache hit

    //         // printf("target->ce:%d\n", target_ce->first_sec_no);
    //         memcpy(target_ce->block,disk_inode,DISK_SECTOR_SIZE);
    //         target_ce->is_dirty=true;
    //       }else{
    //         // printf("flag4");
    //         target_ce = cache_in(sector);
    //         //cache_in(sector+1);
    //         memcpy(target_ce->block,disk_inode,DISK_SECTOR_SIZE);
    //         target_ce->is_dirty =true;
    //       }

    //       if (sectors > 0)
    //         {
    //           static char zeros[DISK_SECTOR_SIZE];
    //           size_t i;
    //           // printf("flag5\n");
    //           for (i = 0; i < sectors; i++)
    //             {
    //             //disk_write (filesys_disk, disk_inode->start + i, zeros);
    //             struct cache_entry* target_ce2 = cache_lookup(disk_inode->start+i,false);
    //             if(target_ce2!=NULL){
    //             //cache hit
    //               memcpy(target_ce2->block,zeros,DISK_SECTOR_SIZE);
    //               target_ce2->is_dirty =true;
    //             }else{
    //               target_ce2 = cache_in(disk_inode->start+i);
    //               memcpy(target_ce2->block,zeros,DISK_SECTOR_SIZE);
    //               target_ce2->is_dirty =true;
    //             }
    //           }
    //         }
    //       // printf("flag10\n");
    //       success = true;
    //     }
    //   free (disk_inode);
    // }


/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector)
{
  //printf("inode open sector:%d\n",sector);

  struct list_elem *e;
  struct inode *inode;
  // printf("opening inode:%d\n",sector);
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
    //  printf("ls inode:%d\n",inode->sector);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }

    }
  // printf("nothing find open inode\n");

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;


//  printf("inode initialize\n");
  list_push_front (&open_inodes, &inode->elem);

  /* Initialize. */
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;


  // printf("indode sector:%d\n",sector);
  // struct cache_entry* target_ce = cache_lookup(inode->sector,false);

  // if(target_ce != NULL){
  //     memcpy(&inode->data,target_ce->block,DISK_SECTOR_SIZE);
  // }else{
  //     target_ce = cache_in(inode->sector);
  //     //cache_in(inode->sector+1);
  //     memcpy(&inode->data,target_ce->block,DISK_SECTOR_SIZE);
  // }
  //disk_read (filesys_disk, inode->sector, &inode->data);


  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */

void
inode_close (struct inode *inode)
{
// printf("close inode:%d\n",inode->sector);
 
/* Ignore null pointer. */
   // printf("clsoe_flag0\n");
 
  if (inode == NULL)
  {
    // printf("flag11111111111111111111\n");
    return;
  }
 
    // printf("clsoe_flag0101\n");
/* Release resources if this was the last opener. */
  // printf("open_cnt:%d\n",inode->open_cnt);
  if (--inode->open_cnt == 0)
  { 
  /* Remove from inode list and release lock. */
    list_remove (&inode->elem);
    // printf("closing_length:%d\n",length);

 
    // printf("close flag1\n");
/* Deallocate blocks if removed. */
    if (inode->removed)
  { 
      // printf("close flag12\n");
      int length;
      int cnt;
      int i;
      struct cache_entry* target_ce = cache_lookup(inode->sector,false);
      struct cache_entry* temp_ce;
      if(target_ce==NULL){
        target_ce = cache_in(inode->sector);
      }
      struct inode_disk *data = (struct inode_disk*)target_ce->block;
      length = bytes_to_sectors(data->length);
      disk_sector_t indirect_block = data->indirect_block;
      disk_sector_t double_indirect_block = data->double_indirect_block;
      
      // printf("closing_length:%d\n",length);
      if(length<12){
        cnt=length;
      }else{cnt=12;}
 
// printf("close flag2\n");
 
      // free_map_release (inode->sector, 1);
      // temp_ce = cache_lookup(inode->sector,false);
      // if(temp_ce!=NULL){
      //   cache_out(temp_ce);
      // }

      free_map_release (inode->sector, 1);
      struct cahce_entry * last_ce = cache_lookup(inode->sector,false);
      if(last_ce!=NULL){
          cache_out(last_ce);
      }

      for(i=0;i<cnt;i++){
        free_map_release (data->direct_block[i], 1);
        temp_ce = cache_lookup(data->direct_block[i],false);
        if(temp_ce!=NULL){
          cache_out(temp_ce);
        }
      }


      if(length<=12){
        goto Done;
      }
 
// printf("close flag3\n");
 
 
        //indirect block
        if(length<=140){
            cnt = length-12;
        }else{cnt=128;}
         
        struct cache_entry * indirect_index_block = cache_lookup(indirect_block,false);
        if(indirect_index_block==NULL){
        target_ce = cache_in(indirect_block);
        }
        disk_sector_t target_sector_no;
         
        for(i=0;i<cnt;i++){
         

            memcpy (&target_sector_no, &indirect_index_block->block[4*i], 4);
            free_map_release(target_sector_no,1);
            temp_ce = cache_lookup(target_sector_no,false);
            if(temp_ce!=NULL){
                cache_out(temp_ce);
            }
        }
        //disk_sector_t target_sector_no = target_ce->block[0];
        free_map_release (indirect_block, 1);
        cache_out(indirect_index_block);
         
        if(length<140){
        goto Done;
        }
 
// printf("close flag4\n");
 
         
        //double indirect block
        cnt = length - 140;
        struct cache_entry * double_indirect_index_block= cache_lookup(double_indirect_block,false);
        if(double_indirect_index_block==NULL){
          double_indirect_index_block= cache_in(double_indirect_block);
        }
        i=0;
         
        int temp_cnt;
        struct cache_entry* intermediate_index_block;
        int index=0;
        disk_sector_t second_target_sector_no;
        while(cnt>0){
        temp_cnt=128;
        if(cnt<temp_cnt){
          temp_cnt=cnt;
        }
        // target_sector_no = target_ce->block[4*i];
        memcpy (&target_sector_no, &double_indirect_index_block->block[4*index], 4);
        intermediate_index_block = cache_lookup(target_sector_no, false);
        if(intermediate_index_block==NULL){
          intermediate_index_block = cache_in(target_sector_no);
        }
 
 
 
        for(i=0;i<temp_cnt;i++){
          memcpy (&second_target_sector_no, &intermediate_index_block->block[4*i], 4);
          free_map_release(second_target_sector_no,1);
          temp_ce = cache_lookup(second_target_sector_no,false);
        if(temp_ce!=NULL){
          cache_out(temp_ce);
          }
        }
         
        free_map_release(target_sector_no,temp_cnt);
        temp_ce = cache_lookup(target_sector_no,false);
        if(temp_ce!=NULL){
        cache_out(temp_ce);
        }
         
        index++;
        cnt = cnt-128;
        }
// printf("close flag6\n");
 
        free_map_release (double_indirect_block, 1);
        temp_ce = cache_lookup(double_indirect_block,false);
        if(temp_ce!=NULL){
        cache_out(temp_ce);
        }
         
        // free_map_release (inode->data.start,
        // bytes_to_sectors (inode->data.length));
        }
        Done:;
        // printf("clsoe_flag2\n");
         
        // printf("cl ose_flag7\n");
        // free_map_release (inode->sector, 1);
        // struct cache_entry* last_ce = cache_lookup(inode->sector,false);
        // if(last_ce!=NULL){
        //   cache_out(last_ce)
        // }

        
        list_remove (&inode->elem);
        free (inode);
        // return true;
        }
        // return false;
}




// void
// inode_close (struct inode *inode)
// {
// //  printf("close inode:%d\n",inode->sector);

//   /* Ignore null pointer. */
//   if (inode == NULL)
//     return;

//   /* Release resources if this was the last opener. */
//   if (--inode->open_cnt == 0)
//     {
//       /* Remove from inode list and release lock. */
//       list_remove (&inode->elem);

//   //    printf("close flag1\n");
//       /* Deallocate blocks if removed. */
//       if (inode->removed)
//         {
//     //      printf("close flag12\n");
//           int length;
//           int cnt;
//           int i;
//           struct cache_entry* target_ce = cache_lookup(inode->sector,false);
//           if(target_ce==NULL){
//             target_ce = cache_in(inode->sector);
//           }
//           struct inode_disk *data = (struct inode_disk*)target_ce->block;
//           disk_sector_t indirect_block = data->indirect_block;
//           disk_sector_t double_indirect_block = data->double_indirect_block;

//           length = bytes_to_sectors(data->length);

//           if(length<12){
//             cnt=length;
//           }else{cnt=12;}


//           free_map_release (inode->sector, 1);

//           for(i=0;i<cnt;i++){
//             free_map_release (data->direct_block[i], 1);

//           }
//           if(length<=12){
//               goto Done;
//           }



//           //indirect block
//           if(length<=140){
//             cnt = length-12;
//           }else{cnt=128;}

//           target_ce = cache_lookup(data->indirect_block,false);
//           if(target_ce==NULL){
//             target_ce = cache_in(data->indirect_block);
//           }
//           disk_sector_t target_sector_no;

//           memcpy (&target_sector_no, &target_ce->block[0], 4);

//           //disk_sector_t target_sector_no = target_ce->block[0];
//           free_map_release (data->indirect_block, 1);
//           free_map_release(target_sector_no,cnt);
//           if(length<140){
//               goto Done;
//           }

//         //  printf("close flag4\n");


//           //double indirect block
//           cnt = length - 140;
//           target_ce = cache_lookup(data->double_indirect_block,false);
//           if(target_ce==NULL){
//             target_ce = cache_in(data->double_indirect_block);
//           }
//           i=0;
//         //  printf("close flag5\n");

//           int temp_cnt;
//           while(cnt>0){
//             temp_cnt=128;
//             // target_sector_no = target_ce->block[4*i];
//             memcpy (&target_sector_no, &target_ce->block[4*i], 4);

//             if(cnt<128){
//               temp_cnt=cnt;
//             }
//             free_map_release(target_sector_no,temp_cnt);
//             i++;
//             cnt = cnt-128;
//           }
//         //  printf("close flag6\n");

//           free_map_release (data->double_indirect_block, 1);

//           // free_map_release (inode->data.start,
//                             // bytes_to_sectors (inode->data.length));
//         }
//     Done:
//     //  printf("close_flag7\n");
//       free (inode);
//     }
// }

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
    // printf("remove\n");

  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{

 // printf("read at \n");

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  //uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = inode_byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE)
        {
          // if cache hit, bring it.
          struct cache_entry *target_ce = cache_lookup(sector_idx,false);
          if (target_ce == NULL) {
            // cache miss
            target_ce = cache_in(sector_idx);
          }
          memcpy (buffer + bytes_read, target_ce->block, DISK_SECTOR_SIZE);

          //printf("read:%s\n",target_ce->block);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          // if (bounce == NULL)
          //   {
          //     bounce = malloc (DISK_SECTOR_SIZE);
          //     if (bounce == NULL)
          //       break;
          //   }

          struct cache_entry *target_ce = cache_lookup(sector_idx,false);
          if (target_ce == NULL) {
            // cache hit.
            target_ce = cache_in(sector_idx);
          }

          memcpy (buffer + bytes_read, target_ce->block+sector_ofs, chunk_size);

          // disk_read (filesys_disk, sector_idx, bounce);
          // memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  //free (bounce);
   //printf("read end\n");



  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  //uint8t *bounce = NULLi

  // printf("write_size:%d\n",size);
  // printf("write_offset:%d\n",offset);


  if (inode->deny_write_cnt)
    {return 0;}

  if (!inode_extend(inode,offset,size)){
    printf("extending fail\n");
    return 0;
  }


  while (size > 0){

      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = inode_byte_to_sector (inode, offset);
      //printf("sectoridx:%d\n",sector_idx);
      int sector_ofs = offset % DISK_SECTOR_SIZE;
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      //printf("write inode_length:%d\n",inode_length(inode));
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      //printf("inode-left:%d\n",inode_left);
      //printf("sector-left:%d\n",sector_left);
      //printf("min-left:%d\n",min_left);


      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      //printf("chunk_size:%d\n",chunk_size);
      if (chunk_size <= 0)
        break;

     // printf("write at inode_sector:%d\n",sector_idx);

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE)
        {

          // printf("flag11111\n");
          struct cache_entry *target_ce = cache_lookup(sector_idx,false);
          if (target_ce != NULL) {
            // cache hit.
            memcpy (target_ce->block, buffer+bytes_written, DISK_SECTOR_SIZE);
            target_ce->is_dirty = true;
          } else {
            /* Read full sector directly into caller's buffer. */
            target_ce = cache_in(sector_idx);
            //cache_in(sector_idx+1);
            memcpy (target_ce->block, buffer+bytes_written, DISK_SECTOR_SIZE);
            target_ce->is_dirty = true;
          }


          //disk_write (filesys_disk, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          //if (bounce == NULL)
          //  {
           //   bounce = malloc (DISK_SECTOR_SIZE);
            //  if (bounce == NULL)
             //   break;
           // }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */

          if (sector_ofs > 0 || chunk_size < sector_left)
            {
              struct cache_entry* target_ce = cache_lookup(sector_idx,false);
              if(target_ce != NULL){
                //cache hit
                memcpy(target_ce->block+sector_ofs,buffer+bytes_written, chunk_size);
                target_ce->is_dirty =true;
              }else{
                // printf("flag2222\n");

                target_ce=cache_in(sector_idx);
                //cache_in(sector_idx+1);
                memcpy(target_ce->block+sector_ofs,buffer+bytes_written,chunk_size);
                target_ce->is_dirty = true;
            }
                  //printf("write:%s\n",target_ce->block);

          }

          else
            {
            //  memset (bounce, 0, DISK_SECTOR_SIZE);

              struct cache_entry* target_ce = cache_lookup(sector_idx,false);
              //printf("secotr_left:%d\n",sector_left);
              //printf("chunk_size:%d\n",chunk_size);

              if(target_ce != NULL){
                //cache hit

                memset(target_ce->block,0,DISK_SECTOR_SIZE);
                memcpy(target_ce->block+sector_ofs,buffer+bytes_written, chunk_size);
                target_ce->is_dirty=true;
              }else{
                // printf("flag3333\n");

                target_ce=cache_in(sector_idx);
                //cache_in(sector_idx+1);
                memset(target_ce->block,0,DISK_SECTOR_SIZE);
                memcpy(target_ce->block+sector_ofs,buffer+bytes_written, sector_left);
                //memcpy(target_ce->block+sector_ofs,buffer+bytes_written, chunk_size);

                target_ce->is_dirty=true;
                }

              //memset (buffer+bytes_written, 0, chunk_size);
                      //printf("write:%s\n",target_ce->block);

            }
          //memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          //disk_write (filesys_disk, sector_idx, bounce);
        }
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }


 //free (bounce);
    //printf("inode_wrtie_end\n");
    return bytes_written;
  }



/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
    // printf("deny_write\n");

  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
    // printf("inode_allow_write\n");

  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct cache_entry * target_ce = cache_lookup(inode->sector,false);
  if(target_ce==NULL){
      target_ce = cache_in(inode->sector);
  }
  struct inode_disk * id= (struct inode_disk*)target_ce->block;
  return id->length;
}

bool
inode_is_directory(struct inode *inode) {
  // printf("0x%08x is directory: %d\n", inode, inode->data.is_directory);
  struct cache_entry *ce = cache_lookup(inode->sector, false);
  if (!ce) {
    ce = cache_in(inode->sector);
  }
  struct inode_disk *disk_inode = (struct inode_disk *) ce->block;
  return disk_inode->is_directory;
}

void
inode_set_directory(struct inode *inode, bool b) {
  struct cache_entry *ce = cache_lookup(inode->sector, false);
  if (!ce) {
    ce = cache_in(inode->sector);
  }
  struct inode_disk *disk_inode = (struct inode_disk *) ce->block;
  disk_inode->is_directory = b;
  // disk_inode->is_dirty = true;
  ce->is_dirty = true;
}

int
inode_open_cnt(struct inode *inode) {
  return inode->open_cnt;
}

// bool
// inode_is_directory(struct inode *inode) {
//   printf("0x%08x is directory: %d\n", inode, inode->data.is_directory);
//   return inode->data.is_directory;
// }
//
// void
// inode_set_directory(struct inode *inode, bool b) {
//   inode->data.is_directory = b;
// }
