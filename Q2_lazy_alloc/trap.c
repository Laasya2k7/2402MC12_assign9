#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// handle an interrupt, exception, or system call from user space.
uint64
usertrap(void)
{
  int which_dev = 0;


  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap()
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  if (r_scause() == 8) {
    // system call
    if (killed(p))
      kexit(-1);

    // advance past ecall.
    p->trapframe->epc += 4;

    intr_on();
    syscall();
  } else if ((which_dev = devintr()) != 0) {
    // device interrupt handled.
} else if (r_scause() == 15 || r_scause() == 13) {
  if (vmfault(p->pagetable, p->sz, r_stval(),
              (r_scause() == 13) ? 1 : 0) != 0) {
    p->page_faults++;
  } else {
    printk("usertrap(): invalid page fault at 0x%lx\n", r_stval());
    setkilled(p);
  }
  } else {
    printk("usertrap(): unexpected scause 0x%lx pid=%d\n",
           r_scause(), p->pid);
    printk("            sepc=0x%lx stval=0x%lx\n",
           r_sepc(), r_stval());
    setkilled(p);
  }

  if (killed(p))
    kexit(-1);

  // give up the CPU if this is a timer interrupt.
    // Give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  prepare_return();

  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);

  ((void (*)(uint64, uint64))trampoline_userret)(
      satp, p->trapframe_va);

  panic("usertrap: userret returned");
}

// set up trapframe and control registers for a return to user space.
void
prepare_return(void)
{
  struct proc *p = myproc();

  // switch traps to uservec and disable interrupts.
  intr_off();

  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();

  // configure sret to return to user mode.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  w_sstatus(x);

  w_sepc(p->trapframe->epc);
}

// interrupts and exceptions from kernel code go here via kernelvec.
void
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0) {
    printk("scause=0x%lx sepc=0x%lx stval=0x%lx\n",
           scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  if (which_dev == 2 && myproc() != 0)
    yield();

  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if (cpuid() == 0) {
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  w_stimecmp(r_time() + 1000000);
}

// check whether an interrupt is external or software interrupt.
int
devintr()
{
  uint64 scause = r_scause();

  if (scause == 0x8000000000000009L) {
    int irq = plic_claim();

    if (irq == UART0_IRQ) {
      uartintr();
    } else if (irq == VIRTIO0_IRQ) {
      virtio_disk_intr();
    } else if (irq) {
      printk("unexpected interrupt irq=%d\n", irq);
    }

    if (irq)
      plic_complete(irq);

    return 1;
  } else if (scause == 0x8000000000000005L) {
    clockintr();
    return 2;
  } else {
    return 0;
  }
}
