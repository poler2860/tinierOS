/* startup.S — ATmega328P bare-metal startup
 * This file runs before main(). It sets up everything C needs.
 */

#include <avr/io.h>   /* gives us RAMEND, SPH, SPL */

/* Tell the assembler we're targeting AVR architecture */
.arch atmega328p

/* -----------------------------------------------
 * Interrupt Vector Table
 * Must be in .vectors section so the linker places
 * it at Flash address 0x0000
 * ----------------------------------------------- */
.section .vectors, "ax", @progbits
.global __vectors
__vectors:

    rjmp _start              /* Vector  0: Reset                          */
    rjmp __bad_interrupt     /* Vector  1: INT0                           */
    rjmp __bad_interrupt     /* Vector  2: INT1                           */
    rjmp __bad_interrupt     /* Vector  3: PCINT0                         */
    rjmp __bad_interrupt     /* Vector  4: PCINT1                         */
    rjmp __bad_interrupt     /* Vector  5: PCINT2                         */
    rjmp __bad_interrupt     /* Vector  6: WDT                            */
    rjmp __bad_interrupt     /* Vector  7: TIMER2_COMPA                   */
    rjmp __bad_interrupt     /* Vector  8: TIMER2_COMPB                   */
    rjmp __bad_interrupt     /* Vector  9: TIMER2_OVF                     */
    rjmp __bad_interrupt     /* Vector 10: TIMER1_CAPT                    */
    rjmp __vector_11         /* Vector 11: TIMER1_COMPA (System Alarm)    */
    rjmp __bad_interrupt     /* Vector 12: TIMER1_COMPB                   */
    rjmp __vector_13         /* Vector 13: TIMER1_OVF   (System Clock)    */
    rjmp __bad_interrupt     /* Vector 14: TIMER0_COMPA                   */
    rjmp __bad_interrupt     /* Vector 15: TIMER0_COMPB                   */
    rjmp __bad_interrupt     /* Vector 16: TIMER0_OVF                     */
    rjmp __bad_interrupt     /* Vector 17: SPI                            */
    rjmp __vector_18         /* Vector 18: USART_RX                       */
    rjmp __vector_19         /* Vector 19: USART_UDRE                     */
    rjmp __bad_interrupt     /* Vector 20: USART_TX                       */
    rjmp __bad_interrupt     /* Vector 21: ADC                            */
    rjmp __bad_interrupt     /* Vector 22: EE_READY                       */
    rjmp __bad_interrupt     /* Vector 23: ANALOG_COMP                    */
    rjmp __bad_interrupt     /* Vector 24: TWI                            */
    rjmp __bad_interrupt     /* Vector 25: SPM_READY                      */

/* Define weak aliases for ISRs so we don't get linker errors if they aren't used */
.weak __vector_11
.set  __vector_11, __bad_interrupt
.weak __vector_13
.set  __vector_13, __bad_interrupt
.weak __vector_18
.set  __vector_18, __bad_interrupt
.weak __vector_19
.set  __vector_19, __bad_interrupt

/* -----------------------------------------------
 * Reset handler — runs immediately after reset
 * ----------------------------------------------- */
.section .init0, "ax", @progbits
.global _start
_start:

/* ---- Job 0: Disable Watchdog Timer ----------------
 * If the watchdog is active, it will reset the board
 * indefinitely. We must clear it immediately.
 */
    clr     r16
    out     0x34, r16     /* MCUSR = 0 (Clear reset flags) */

    /* WDCE and WDE must be set to 1 to allow changes */
    ldi     r17, 0x18     /* WDTCSR = (1<<WDCE) | (1<<WDE) */
    sts     0x60, r17

    /* Now clear WDE to disable watchdog */
    sts     0x60, r16

/* ---- Job 1: Zero r1 --------------------------------
 * The AVR ABI contract: r1 must always be 0.
 */
    clr     r1

/* ---- Job 2: Set the Stack Pointer ------------------
 */
    ldi     r16, hi8(RAMEND)
    out     _SFR_IO_ADDR(SPH), r16
    ldi     r16, lo8(RAMEND)
    out     _SFR_IO_ADDR(SPL), r16

/* ---- Job 3: Copy .data from Flash to SRAM ----------
 */
    ldi     r30, lo8(__data_start_flash)
    ldi     r31, hi8(__data_start_flash)
    ldi     r26, lo8(__data_start_sram)
    ldi     r27, hi8(__data_start_sram)
    ldi     r24, lo8(__data_bytes_to_read)
    ldi     r25, hi8(__data_bytes_to_read)

    sbiw    r24, 0
    breq    .L_data_done

.L_data_copy:
    lpm     r0, Z+
    st      X+, r0
    sbiw    r24, 1
    brne    .L_data_copy

.L_data_done:

/* ---- Job 4: Zero .bss in SRAM ----------------------
 */
    ldi     r26, lo8(__bss_start_sram)
    ldi     r27, hi8(__bss_start_sram)
    ldi     r24, lo8(__bss_bytes_to_clear)
    ldi     r25, hi8(__bss_bytes_to_clear)

    sbiw    r24, 0
    breq    .L_bss_done

.L_bss_zero:
    st      X+, r1
    sbiw    r24, 1
    brne    .L_bss_zero

.L_bss_done:

/* ---- Job 5: Jump to main ---------------------------
 */
    call    main

/* ---- Safety net: infinite loop ---------------------
 */
.L_halt:
    rjmp    .L_halt

/* -----------------------------------------------
 * Default/unhandled interrupt handler
 * ----------------------------------------------- */
.section .text
.global __bad_interrupt
__bad_interrupt:
    rjmp    __bad_interrupt
