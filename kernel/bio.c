// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct bucket {
  struct buf head;
  struct spinlock lock;
};

struct {
  struct spinlock buflock;
  struct buf buf[NBUF];

  struct bucket buckets[NBUCKET];
} bcache;

uint bhash(uint blockno) {
  return blockno % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.buflock, "bcache.buflock");
  for (int i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.buckets[i].lock, "bcache.lock");
  }

  // Create linked list of each bucket buffers
  for (int j = 0; j < NBUCKET; ++j) {
    struct buf *head = &bcache.buckets[j].head;
    head->next = head;
    head->prev = head;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *tmpb;

  uint bkt = bhash(blockno);

  acquire(&bcache.buckets[bkt].lock);

  // Is the block already cached?
  for(b = bcache.buckets[bkt].head.next; b != &bcache.buckets[bkt].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[bkt].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[bkt].lock);

  uint bkt_num;
  acquire(&bcache.buflock);
  uint oldest_ts = -1;
  for (tmpb = bcache.buf; tmpb < bcache.buf+NBUF; tmpb++) {
    if (tmpb->refcnt == 0) {
      if (oldest_ts > tmpb->ts) {
        oldest_ts = tmpb->ts;
        b = tmpb;
      }
    }
  }
  if (b->refcnt == 0) {
    bkt_num = bhash(b->blockno);
    acquire(&bcache.buckets[bkt_num].lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next->prev = b->prev;
    b->prev->next = b->next;

    release(&bcache.buckets[bkt_num].lock);

    acquire(&bcache.buckets[bkt].lock);
    b->next = bcache.buckets[bkt].head.next;
    b->prev = &bcache.buckets[bkt].head;
    b->next->prev = b;
    b->prev->next = b;
    release(&bcache.buckets[bkt].lock);
    release(&bcache.buflock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.buflock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint bkt = bhash(b->blockno);
  acquire(&bcache.buckets[bkt].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->ts = ticks;
  }
  
  release(&bcache.buckets[bkt].lock);
}

void
bpin(struct buf *b) {
  uint bkt = bhash(b->blockno);
  acquire(&bcache.buckets[bkt].lock);
  b->refcnt++;
  release(&bcache.buckets[bkt].lock);
}

void
bunpin(struct buf *b) {
  uint bkt = bhash(b->blockno);
  acquire(&bcache.buckets[bkt].lock);
  b->refcnt--;
  release(&bcache.buckets[bkt].lock);
}


