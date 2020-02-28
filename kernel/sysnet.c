//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

struct sock {
  struct sock *next; // the next socket in the list
  uint32 raddr;      // the remote IPv4 address
  uint16 lport;      // the local UDP port number
  uint16 rport;      // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;  // a queue of packets waiting to be received
};

static struct spinlock lock;
static struct sock *sockets;

void
sockinit(void)
{
  initlock(&lock, "socktbl");
}

int
sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock*)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos) {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
	pos->rport == rport) {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char*)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//

static void sockfree(struct sock* si) {
  struct sock** pos = &sockets;
  acquire(&lock);
  while (*pos != si) {
    pos = &(*pos)->next;
  }
  *pos = si->next;
  release(&lock);
}

static struct sock* sockfind(uint32 raddr, uint16 lport, uint16 rport) {
  struct sock* pos = sockets;
  acquire(&lock);
  while (pos) {
    if (pos->raddr == raddr && pos->lport == lport && pos->rport == rport) {
      release(&lock);
      return pos;
    }
    pos = pos->next;
  }
  release(&lock);
  return 0;
}

void sockclose(struct sock* si) {
  sockfree(si);
  kfree((char*)si);
}

int sockwrite(struct sock* si, uint64 addr, int n) {
  unsigned int headroom_len = sizeof(struct eth) + sizeof(struct ip) +
                              sizeof(struct udp);
  struct mbuf* mbuf = mbufalloc(headroom_len);
  struct proc* p = myproc();
  acquire(&si->lock);
  if (n > MBUF_SIZE-headroom_len)
    n = MBUF_SIZE-headroom_len;
  if (copyin(p->pagetable, mbuf->head, addr, n) == -1)
    return -1;
  mbufput(mbuf, n);
  net_tx_udp(mbuf, si->raddr, si->lport, si->rport);
  release(&si->lock);
  return n;
}

int sockread(struct sock* si, uint64 addr, int n) {
  struct proc* p = myproc();
  acquire(&si->lock);
  while (mbufq_empty(&si->rxq)) {
    if(p->killed) {
      release(&si->lock);
      return -1;
    }
    sleep(&si->rxq, &si->lock);
  }
  struct mbuf* mbuf = mbufq_pophead(&si->rxq);
  if (n > mbuf->len)
    n = mbuf->len;

  if (copyout(p->pagetable, addr, mbuf->head, n) == -1)
    return -1;

  mbuffree(mbuf);
  release(&si->lock);
  return n;
}

// called by protocol handler layer to deliver UDP packets
void
sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  //
  // Your code here.
  //
  // Find the socket that handles this mbuf and deliver it, waking
  // any sleeping reader. Free the mbuf if there are no sockets
  // registered to handle it.
  //
  struct sock* si = sockfind(raddr, lport, rport);
  if (si) {
    mbufq_pushtail(&si->rxq, m);
    wakeup(&si->rxq);
    return;
  }
  mbuffree(m);
}
