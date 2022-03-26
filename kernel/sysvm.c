#include "types.h"
#include "riscv.h"
#include "fcntl.h"
#include "param.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "memlayout.h"
#include "fs.h"
#include "file.h"



// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int argfd(int n, int *pfd, struct file **pf) {
  int fd;
  struct file *f;
  if (argint(n, &fd) < 0) return -1;
  if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0) return -1;
  if (pfd) *pfd = fd;
  if (pf) *pf = f;
  return 0;
}

uint64 sys_mmap(void) {
  uint64 addr;
  struct file *pf;
  int length, prot, flags, fd, offset;
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 || argfd(4, &fd, &pf) < 0 ||
      argint(5, &offset) < 0) {
    return -1;
  }
  // assume addr and offset always be zero
  if (addr != 0 || offset != 0) {
    return -1;
  }
  if((pf->writable == 0) && !(flags & MAP_PRIVATE) && (prot & PROT_WRITE)) {
    return -1;
  }
  if((pf->readable == 0) && (prot & PROT_READ)) {
    return -1;
  }
  struct proc *p = myproc();
  int i = 0;
  for (i = 0; i < 16; ++i) {
    if (p->mmap[i].length == 0) {
      break;
    }
  }
  if (i == 16) {
    return -1;
  }
  int perm = PTE_U | PTE_V;
  if(prot & PROT_READ){
    perm |= PTE_R;
  }
  if(prot & PROT_WRITE) {
    perm |= PTE_W;
  }

  p->mmap[i].file = pf;
  p->mmap[i].length = length;
  p->mmap[i].perm = perm;
  p->mmap[i].offset = offset;
  p->mmap[i].flags = flags;
  if (addr == 0) {
    uint64 oldsz = PGROUNDUP(p->sz);
    uint64 newsz = oldsz + length;
    p->mmap[i].addr = oldsz;
    if (newsz >= TRAPFRAME) {
      return -1;
    }
    p->sz = newsz;
  } else {
    p->mmap[i].addr = addr;
  }
  filedup(pf);
  return p->mmap[i].addr;
}


uint64 sys_munmap(void) {
  uint64 addr;
  int length;
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0) {
    return -1;
  }

  struct proc *p = myproc();
  int i = 0;
  for (i = 0; i < 16; ++i) {
    if (p->mmap[i].addr == addr || p->mmap[i].addr + p->mmap[i].length == addr + length) {
      break;
    }
  }
  if(i == 16){
    return -1;
  }

  if (p->mmap[i].flags & MAP_SHARED) {
    // write back
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int cur = 0;
    struct file *f = p->mmap[i].file;
    while(cur < length) {
      int n1 = length - cur;
      if(n1 > max)
        n1 = max;
      begin_op();
      ilock(f->ip);
      int r = writei(f->ip, 1, addr + cur, p->mmap[i].offset + addr - p->mmap[i].addr + cur, n1);
      iunlock(f->ip);
      end_op();
      cur += r;
      if(r != n1){
        break;
      }
    }
  }
  // unmap the memory
  uvmunmap(p->pagetable, addr, length / PGSIZE, 1);
  if(addr == p->mmap[i].addr){
    p->mmap[i].addr += length;
    p->mmap[i].offset += length;
  }
  p->mmap[i].length -= length;
  if(p->mmap[i].length == 0){
    fileclose(p->mmap[i].file);
  }
  return 0;
}