#ifndef ARCH_AVR_CPU_H
#define ARCH_AVR_CPU_H

#include <stdint.h>
#include <stddef.h>
#include "types.h"

/*
 * AVR Hardware Abstraction Layer (HAL)
 * Replaces the legacy x86_64 bios.h
 */

typedef void interrupt_handler(void);

typedef uint32_t TimerDuration;

typedef enum Interrupt {
    ICI,
    ALARM,
    SERIAL_RX_READY,
    SERIAL_TX_READY,
    MAX_INTERRUPT
} Interrupt;

/* AVR ATmega328P is strictly single-core */
#define MAX_CORES 1
#define MAX_TERMINALS 1

extern uint cpu_core_id;

/* CPU Control */
uint cpu_cores(void);
void cpu_core_barrier_sync(void);
void cpu_ici(uint core);
void cpu_interrupt_handler(Interrupt interrupt, interrupt_handler handler);
int  cpu_disable_interrupts(void);
int  cpu_interrupts_enabled(void);
void cpu_enable_interrupts(void);
void cpu_core_halt(void);
void cpu_core_restart(uint c);
void cpu_core_restart_one(void);
void cpu_core_restart_all(void);

/* 
 * Context Switching
 * On AVR, the context only needs to store the Stack Pointer (SP).
 * All 32 general purpose registers and SREG are pushed onto the stack 
 * itself during the context swap.
 */
typedef struct cpu_context {
    void* sp;
} cpu_context_t;

void cpu_initialize_context(cpu_context_t* ctx, void* ss_sp, size_t ss_size, void (*func)(void));
void cpu_swap_context(cpu_context_t* oldctx, cpu_context_t* newctx);

/* Hardware Devices / Legacy BIOS definitions (to satisfy existing kernel code) */
void bios_init(uint serialno);
TimerDuration bios_set_timer(TimerDuration usec);
TimerDuration bios_cancel_timer(void);
TimerDuration bios_clock(void);

uint bios_serial_ports(void);
void bios_serial_interrupt_core(uint serial, Interrupt intno, uint core);
int  bios_read_serial(uint serial, char* ptr);
int  bios_write_serial(uint serial, char value);

#endif /* ARCH_AVR_CPU_H */