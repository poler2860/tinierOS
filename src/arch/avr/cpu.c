#include "cpu.h"
#include "tinyos.h"
#include "kernel_proc.h"

/* Manual definitions for AVR bare-metal (no stdlib) */
#define ISR(vector) \
    void vector (void) __attribute__ ((signal, used, externally_visible)); \
    void vector (void)

#define sei() __asm__ __volatile__ ("sei" ::: "memory")
#define cli() __asm__ __volatile__ ("cli" ::: "memory")

/* ATmega328P Register Definitions (subset needed for drivers) */
#define _SFR_MEM8(mem_addr) (*(volatile uint8_t *)(mem_addr))
#define _SFR_MEM16(mem_addr) (*(volatile uint16_t *)(mem_addr))

#define SREG_ADDR   0x5F
#define SPH_ADDR    0x5E
#define SPL_ADDR    0x5D

#define TCCR1A_ADDR 0x80
#define TCCR1B_ADDR 0x81
#define TCNT1_ADDR  0x84
#define OCR1A_ADDR  0x88
#define TIMSK1_ADDR 0x6F

#define UCSR0A_ADDR 0xC0
#define UCSR0B_ADDR 0xC1
#define UCSR0C_ADDR 0xC2
#define UBRR0L_ADDR 0xC4
#define UBRR0H_ADDR 0xC5
#define UDR0_ADDR   0xC6

#define SREG  _SFR_MEM8(SREG_ADDR)
#define SPH   _SFR_MEM8(SPH_ADDR)
#define SPL   _SFR_MEM8(SPL_ADDR)

#define TCCR1A _SFR_MEM8(TCCR1A_ADDR)
#define TCCR1B _SFR_MEM8(TCCR1B_ADDR)
#define TIMSK1 _SFR_MEM8(TIMSK1_ADDR)

#define TCNT1_L _SFR_MEM8(0x84)
#define TCNT1_H _SFR_MEM8(0x85)
#define OCR1A_L _SFR_MEM8(0x88)
#define OCR1A_H _SFR_MEM8(0x89)

#define UCSR0A _SFR_MEM8(UCSR0A_ADDR)
#define UCSR0B _SFR_MEM8(UCSR0B_ADDR)
#define UCSR0C _SFR_MEM8(UCSR0C_ADDR)
#define UBRR0L _SFR_MEM8(UBRR0L_ADDR)
#define UBRR0H _SFR_MEM8(UBRR0H_ADDR)
#define UDR0   _SFR_MEM8(UDR0_ADDR)

/* Register Bits */
#define WGM12 3
#define CS11  1
#define CS10  0
#define OCIE1A 1
#define TOIE1  0
#define RXEN0  4
#define TXEN0  3
#define RXCIE0 7
#define UCSZ01 2
#define UCSZ00 1
#define RXC0   7
#define UDRE0  5

/* Vector Symbols (to match startup.S) */
#define TIMER1_COMPA_vect __vector_11
#define TIMER1_OVF_vect   __vector_13
#define USART_RX_vect     __vector_18
#define USART_UDRE_vect   __vector_19

uint cpu_core_id = 0;

uint cpu_cores(void) { return 1; }
void cpu_core_barrier_sync(void) {}
void cpu_ici(uint core) { (void)core; }

static interrupt_handler* interrupt_handlers[MAX_INTERRUPT];

void cpu_interrupt_handler(Interrupt interrupt, interrupt_handler handler) {
    if (interrupt < MAX_INTERRUPT) {
        interrupt_handlers[interrupt] = handler;
    }
}

ISR(TIMER1_COMPA_vect) {
    if (interrupt_handlers[ALARM]) {
        interrupt_handlers[ALARM]();
    }
}

ISR(TIMER1_OVF_vect) {
    system_clock_overflows++;
}

ISR(USART_RX_vect) {
    uint8_t dummy = UDR0; (void)dummy; /* Always read to clear interrupt */
    if (interrupt_handlers[SERIAL_RX_READY]) {
        interrupt_handlers[SERIAL_RX_READY]();
    }
}

ISR(USART_UDRE_vect) {
    if (interrupt_handlers[SERIAL_TX_READY]) {
        interrupt_handlers[SERIAL_TX_READY]();
    }
}

int cpu_disable_interrupts(void) {
    int enabled = (SREG & 0x80) != 0;
    cli();
    return enabled;
}

int cpu_interrupts_enabled(void) {
    return (SREG & 0x80) != 0;
}

void cpu_enable_interrupts(void) {
    sei();
}

void cpu_core_halt(void) {
    /* Basic sleep loop (Idle mode = 0) */
    __asm__ __volatile__ (
        "out %0, %1" "\n\t"
        "sei"        "\n\t"
        "sleep"      "\n\t"
        :: "I" (0x33), "r" (0x01) /* SMCR = 0x01 (Sleep Enable, Idle Mode) */
    );
}

void cpu_core_restart(uint c) { (void)c; }
void cpu_core_restart_one(void) {}
void cpu_core_restart_all(void) {}

/*
 * cpu_initialize_context()
 * Creates an initial stack frame for a new thread.
 * When cpu_swap_context switches to this thread, it will pop these values
 * into the AVR's registers, and the final 'ret' instruction will pop the
 * Program Counter (PC) to start executing 'func'.
 */
void cpu_initialize_context(cpu_context_t* ctx, void* ss_sp, size_t ss_size, void (*func)(void)) {
    /* AVR stack grows downwards. Start at the highest address of the stack block. */
    uint8_t* stack = (uint8_t*)ss_sp + ss_size - 1;

    /* 
     * 1. Push Program Counter (PC)
     * On ATmega328P (16-bit PC), 'ret' pops low byte, then high byte.
     * So we must store high byte at higher address, low byte at lower address.
     */
    uint16_t pc = (uint16_t)(uintptr_t)func;
    *stack-- = (uint8_t)(pc & 0xFF);        /* Low byte  */
    *stack-- = (uint8_t)((pc >> 8) & 0xFF); /* High byte */

    /* 
     * cpu_swap_context expects:
     * [higher address]
     * PC_L
     * PC_H
     * R0
     * SREG
     * R1
     * ...
     * R31
     * [lower address / SP]
     */

    /* 2. Push R0 */
    *stack-- = 0x00;

    /* 3. Push SREG
     * Set Global Interrupt Enable flag (bit 7) so interrupts are 
     * automatically enabled when this thread starts.
     */
    *stack-- = 0x80;

    /* 4. Push R1 through R31 (31 general purpose registers) */
    for (int i = 1; i <= 31; i++) {
        *stack-- = 0x00;
    }

    /* Save the final stack pointer into the thread's context block */
    ctx->sp = stack;
}

void Mutex_Lock(Mutex* mx) { (void)mx; }
void Mutex_Unlock(Mutex* mx) { (void)mx; }

/* 
 * BIOS Implementation for AVR
 */
void bios_init(uint serialno) {
    (void)serialno;
    
    /* Setup Timer 1 for the system clock */
    TCCR1A = 0;
    /* CTC mode, clk/64 prescaler */
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); 
    /* 16MHz / 64 = 250kHz. One tick = 4us. */
    TIMSK1 |= (1 << OCIE1A); /* Enable Compare Match A interrupt */
    
    /* Enable overflow interrupt for long-term clock */
    TIMSK1 |= (1 << TOIE1);

    /* Setup UART0: 9600 baud, 8N1 */
    /* UBRR = F_CPU / (16 * baud) - 1 = 16000000 / (16 * 9600) - 1 = 103.16 -> 103 */
    UCSR0A = 0; /* Clear status */
    UBRR0H = 0;
    UBRR0L = 103;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); /* Enable RX, TX, and RX interrupt */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); /* 8-bit data */

    /* Set a safe initial alarm (approx 10ms) */
    OCR1A_H = (uint8_t)(2500 >> 8);
    OCR1A_L = (uint8_t)(2500 & 0xFF);

    /* Boot Diagnostic Output (BLOCKING for boot) */
    while (!(UCSR0A & (1 << UDRE0))); UDR0 = 'T';
    while (!(UCSR0A & (1 << UDRE0))); UDR0 = 'O';
    while (!(UCSR0A & (1 << UDRE0))); UDR0 = 'S';
    while (!(UCSR0A & (1 << UDRE0))); UDR0 = '>';
}

TimerDuration bios_set_timer(TimerDuration usec) {
    /* Convert usec to Timer 1 ticks (4us per tick) */
    uint16_t ticks = (uint16_t)(usec >> 2);
    if (ticks == 0) ticks = 1;
    
    /* Write 16-bit register: High byte FIRST, then Low byte */
    OCR1A_H = (uint8_t)(ticks >> 8);
    OCR1A_L = (uint8_t)(ticks & 0xFF);
    
    /* Reset counter: High byte FIRST, then Low byte */
    TCNT1_H = 0;
    TCNT1_L = 0;
    
    return usec;
}

TimerDuration bios_cancel_timer(void) {
    /* Read 16-bit register: Low byte FIRST, then High byte */
    uint16_t current_tcnt = TCNT1_L;
    current_tcnt |= ((uint16_t)TCNT1_H << 8);
    
    uint16_t current_ocr = OCR1A_L;
    current_ocr |= ((uint16_t)OCR1A_H << 8);
    
    if (current_tcnt >= current_ocr) return 0;
    
    uint16_t remaining_ticks = current_ocr - current_tcnt;
    
    /* Effectively disable */
    OCR1A_H = 0xFF;
    OCR1A_L = 0xFF;
    
    return (TimerDuration)remaining_ticks << 2;
}

static volatile uint32_t system_clock_overflows = 0;
ISR(TIMER1_OVF_vect) {
    system_clock_overflows++;
}

TimerDuration bios_clock(void) {
    /* Return approximate time in ms */
    /* Each overflow of 16-bit timer at 250kHz is 65536 * 4us = 262.144ms */
    /* Approx 262ms per overflow. 262 = 256 + 4 + 2 approx. 
     * To avoid 32-bit mult: (ovf << 8) is close enough for a basic clock.
     */
    return system_clock_overflows << 8;
}

uint bios_serial_ports(void) { return 1; }
void bios_serial_interrupt_core(uint serial, Interrupt intno, uint core) {
    (void)serial; (void)intno; (void)core;
}

int bios_read_serial(uint serial, char* ptr) {
    if (serial != 0) return 0;
    if (!(UCSR0A & (1 << RXC0))) return 0;
    *ptr = UDR0;
    return 1;
}

int bios_write_serial(uint serial, char value) {
    if (serial != 0) return 0;
    /* Return 0 if UDRE (Data Register Empty) is not set */
    if (!(UCSR0A & (1 << UDRE0))) return 0;
    UDR0 = value;
    return 1;
}