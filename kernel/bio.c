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

#define BUCKET_SIZE 13

struct {
  struct spinlock lock;
  struct spinlock bucket_lock[BUCKET_SIZE];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  struct buf head[BUCKET_SIZE];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < BUCKET_SIZE; ++i){
    initlock(&bcache.bucket_lock[i], "bcache.bucket");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int index = blockno % BUCKET_SIZE;
  acquire(&bcache.bucket_lock[index]);
  // Is the block already cached?
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[index]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);

  // check again in case that the eviction has already been done for the same block 
  acquire(&bcache.bucket_lock[index]);
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[index]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[index]);

  uint minn = 0xffffffff;
  struct buf *final = 0;
  struct buf *last = 0;
  for(int i = 0; i < BUCKET_SIZE; ++i){
    acquire(&bcache.bucket_lock[i]);
    for(b = bcache.head[i].next; b != &bcache.head[i]; b = b->next){
      if(b->refcnt == 0 && b->timestamp < minn){
        minn = b->timestamp;
        final = b;
      }
    }
    if(last == final){
      release(&bcache.bucket_lock[i]);
    }else{
      if(last){
        release(&bcache.bucket_lock[last->blockno % BUCKET_SIZE]);
      }
      last = final;
    }
  }
  
  if(!final){
    panic("bget: no buffers");
  }

  int t_index = final->blockno % BUCKET_SIZE;
  final->dev = dev;
  final->blockno = blockno;
  final->valid = 0;
  final->refcnt = 1;
  if (t_index != index){
    final->prev->next = final->next;
    final->next->prev = final->prev;
  }
  release(&bcache.bucket_lock[t_index]);

  if (t_index != index){
    acquire(&bcache.bucket_lock[index]);
    final->next = bcache.head[index].next;
    final->prev = &bcache.head[index];
    bcache.head[index].next->prev = final;
    bcache.head[index].next = final;
    release(&bcache.bucket_lock[index]);
  }
  release(&bcache.lock);
  acquiresleep(&final->lock);
  return final;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
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
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int index = b->blockno % BUCKET_SIZE;
  acquire(&bcache.bucket_lock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  release(&bcache.bucket_lock[index]);
}

void
bpin(struct buf *b) {
  int index = b->blockno % BUCKET_SIZE;
  acquire(&bcache.bucket_lock[index]);
  b->refcnt++;
  release(&bcache.bucket_lock[index]);
}

void
bunpin(struct buf *b) {
  int index = b->blockno % BUCKET_SIZE;
  acquire(&bcache.bucket_lock[index]);
  b->refcnt--;
  release(&bcache.bucket_lock[index]);
}


