#ifndef __KERNEL_CC_H
#define __KERNEL_CC_H

#include "cpu.h"
#include "kernel_sched.h"

#define preempt_off  cpu_disable_interrupts()
#define preempt_on   cpu_enable_interrupts()

/* Atomic kernel lock using inline assembly */
static inline void kernel_lock() { 
    __asm__ __volatile__ ("cli" ::: "memory"); 
}

static inline void kernel_unlock() { 
    __asm__ __volatile__ ("sei" ::: "memory"); 
}

#endif
