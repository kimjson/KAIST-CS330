#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <threads/vaddr.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/process.h"

#define MAX_STACK_SIZE 8*1024*1024

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void)
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void)
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f)
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */

  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process. */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit ();

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);

      PANIC ("Kernel bug - unexpected interrupt in kernel");

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f)
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  bool is_stack_growth=false; //determine whether page fault caused by stack growth

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  // printf("fault address: 0x%08x\n", fault_addr);
  // printf("not present: %d\n", not_present);
  // printf("write1: %d\n", write);

  //printf("PHYS_BASE:0x%08x",PHYS_BASE-8388608);
  //printf("fault_addr:0x%08x",fault_addr);
  //printf("f->exp:0x%08x",f->esp);

  struct sup_page_entry *sup_pte = sup_page_table_lookup(&thread_current()->sup_page_table, pg_round_down (fault_addr));
  if((fault_addr>=(f->esp)-32)&&(fault_addr >= PHYS_BASE - 8388608)&&(fault_addr < PHYS_BASE) && sup_pte == NULL)
  {
    is_stack_growth=true;
  }
  else
  {
    is_stack_growth=false;
  }


//  printf("is_stack_growth:%d\n",is_stack_growth);
  // f->esp = pg_round_down (f->esp);

  fault_addr = pg_round_down(fault_addr);
  if(is_stack_growth)
  {
    void *kpage = frame_table_allocator(PAL_USER);
    sup_page_entry_create(fault_addr,kpage, NULL);
    pagedir_set_page(thread_current()->pagedir, fault_addr, kpage, write);
  }
  else
  {
    if (is_kernel_vaddr(fault_addr) || !not_present || sup_pte == NULL) {
      //printf("fault addr: 0x%08x\n", fault_addr);
      //printf("not present: %d\n", not_present);
      printf("%s: exit(%d)\n", thread_current()->exec_name, -1);
      // uint32_t paddr = (uint32_t)pagedir_get_page(thread_current()->pagedir, fault_addr);
      // printf("present bit: %d\n", paddr & PTE_P);
      // printf("mode bit: %d\n", paddr & PTE_W);
      // printf("owner bit: %d\n", paddr & PTE_U);
      // printf("write: %d\n", write);
      thread_current()->info->is_killed = true;
      thread_current()->info->exit_status = -1;
      thread_exit();
    }
    else {
      // if fault_case is swap, swap in
      if (sup_pte->file_address == NULL) {
        swap_in(sup_pte, not_present);
      }
      // if fault_case is filesys, read from file
      else {

        // printf("read from file start\n");
        void *kpage;
        if (sup_pte->lazy_type == 1)//all zerod page
        {
          kpage = frame_table_allocator(PAL_USER | PAL_ZERO);
          pagedir_set_page(thread_current()->pagedir, sup_pte->upage, kpage, sup_pte->writable);
          sup_pte->kpage = kpage;
        }
        else if (sup_pte->lazy_type == 2)//all non-zerod page
        {
          kpage = frame_table_allocator(PAL_USER);
          sup_pte->kpage = kpage;
          file_read_at(sup_pte->file_address, kpage, PGSIZE, sup_pte->file_pos);
          pagedir_set_page(thread_current()->pagedir, sup_pte->upage, kpage, sup_pte->writable);
        }
        else // loading mmaped file
        {
          kpage = frame_table_allocator(PAL_USER | PAL_ZERO);
          pagedir_set_page(thread_current()->pagedir, sup_pte->upage, kpage, not_present);
          sup_pte->kpage = kpage;
          // sup_pte->file_pos = sup_pte->file_address->pos;
          file_read_at (sup_pte->file_address, kpage, PGSIZE, sup_pte->file_pos);
          // printf("read from file end\n");
        }
      }
    }
  }

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
//  printf ("Page fault at %p: %s error %s page in %s context.\n",
//          fault_addr,
//          not_present ? "not present" : "rights violation",
//          write ? "writing" : "reading",
//          user ? "user" : "kernel");
//  kill (f);
}
