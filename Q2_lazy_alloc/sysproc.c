#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  int n;
  uint64 addr;
  struct proc *p = myproc();

  argint(0, &n);

  addr = p->sz;

  if (n > 0) {
    if (addr + n < addr || addr + n >= MAXVA)
      return -1;

    p->sz += n;
  } else if (n < 0) {
    if (growproc(n) < 0)
      return -1;
  }

  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_shm_get(void)
{
  return shm_get();
}

uint64
sys_sem_init(void)
{
  int id, value;

  argint(0, &id);
  argint(1, &value);

  sem_init(id, value);

  return 0;
}

uint64
sys_sem_wait(void)
{
  int id;

  argint(0, &id);

  sem_wait(id);

  return 0;
}

uint64
sys_sem_post(void)
{
  int id;

  argint(0, &id);

  sem_post(id);

  return 0;
}



uint64
sys_clone(void)
{
  uint64 fcn, arg, stack;

  argaddr(0, &fcn);
  argaddr(1, &arg);
  argaddr(2, &stack);

  printk("sys_clone: fcn=%ld arg=%ld stack=%ld\n", fcn, arg, stack);

  return kclone((void (*)(void *))fcn, (void *)arg, (void *)stack);
}


uint64
sys_join(void)
{
  return kjoin();
}

uint64
sys_va2pa(void)
{
  uint64 va;
  struct proc *p = myproc();
  pte_t *pte;
  uint64 pa;

  // Get the virtual address supplied by the user.
  argaddr(0, &va);

  // Reject addresses outside the supported virtual-address range.
  if (va >= MAXVA)
    return -1;

  // Walk the current process's page table without creating entries.
  pte = walk(p->pagetable, va, 0);

  // Reject unmapped pages and pages inaccessible to user mode.
  if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
    printk("va2pa: address %p is unmapped or not user-accessible\n",
           (void *)va);
    return -1;
  }

  // Combine the physical page base with the page offset.
  pa = PTE2PA(*pte) + (va & (PGSIZE - 1));

  // Print the translation details.
  printk("va2pa: VA=%p PA=%p\n", (void *)va, (void *)pa);
  printk("  Level-2 index=%d, Level-1 index=%d, Level-0 index=%d, Offset=%d\n",
       (int)PX(2, va), (int)PX(1, va), (int)PX(0, va),
       (int)(va & (PGSIZE - 1)));
  printk("  Flags: P=%d W=%d U=%d\n",
         (*pte & PTE_V) != 0,
         (*pte & PTE_W) != 0,
         (*pte & PTE_U) != 0);

  return pa;
}

static uint64
count_pagetable_pages(pagetable_t pagetable, int level)
{
  uint64 count = 1;

  if (level == 0)
    return count;

  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];

    if ((pte & PTE_V) && !(pte & (PTE_R | PTE_W | PTE_X))) {
      pagetable_t child = (pagetable_t)PTE2PA(pte);
      count += count_pagetable_pages(child, level - 1);
    }
  }

  return count;
}

uint64
sys_getmeminfo(void)
{
  uint64 addr;
  struct meminfo mi;
  struct proc *p = myproc();

  argaddr(0, &addr);

  mi.total_pages = get_total_pages();
  mi.free_pages = get_free_pages();
  mi.process_pages = p->sz / PGSIZE;
  mi.page_table_pages = count_pagetable_pages(p->pagetable, 2);

  if (copyout(p->pagetable, sizeof(mi), addr,
              (char *)&mi, sizeof(mi)) < 0)
    return -1;

  return 0;
}


uint64
sys_vmprint(void)
{
  struct proc *p = myproc();
  vmprint(p->pagetable, p->sz);
  return 0;
}

uint64
sys_getfaults(void)
{
  return myproc()->page_faults;
}