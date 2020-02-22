// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];

static char locknames[NCPU][5];

void
kinit()
{
  for (uint8 i = 0; i < NCPU; ++i) {
    locknames[i][0] = 'k';
    locknames[i][1] = 'e';
    locknames[i][2] = 'm';
    locknames[i][3] = i;
    locknames[i][4] = 0;
    initlock(&kmems[i].lock, locknames[i]);
  }

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu = cpuid();
  pop_off();
  struct kmem* k = &kmems[cpu];
  acquire(&k->lock);
  r->next = k->freelist;
  k->freelist = r;
  release(&k->lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();
  pop_off();

  struct kmem* k = &kmems[cpu];
  acquire(&k->lock);
  r = k->freelist;
  if(r) {
    k->freelist = r->next;
    release(&k->lock);
  } else {
    release(&k->lock);
    for (int i = 0; i < NCPU; ++i) {
      if (i == cpu)
        continue;
      struct kmem* tmp_k = &kmems[i];
      acquire(&tmp_k->lock);
      r = tmp_k->freelist;
      if (r) {
        tmp_k->freelist = r->next;
        release(&tmp_k->lock);
        break;
      }
      release(&tmp_k->lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
