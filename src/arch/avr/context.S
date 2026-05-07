/*
 * context.S - Context Switching for AVR ATmega328P
 */

#include <avr/io.h>

.arch atmega328p

/*
 * void cpu_swap_context(cpu_context_t* oldctx, cpu_context_t* newctx);
 *
 * Arguments:
 *   oldctx -> r25:r24 (Pointer to the current thread's SP variable)
 *   newctx -> r23:r22 (Pointer to the next thread's SP variable)
 */
.section .text
.global cpu_swap_context
cpu_swap_context:

    /*
     * 1. Push current thread state onto current stack
     *
     * The call to `cpu_swap_context` already pushed the Return Address (PC)
     * onto the stack. Now we push all registers.
     */

    /* Push r0 */
    push r0

    /* Push SREG */
    in   r0, _SFR_IO_ADDR(SREG)
    push r0

    /* Push the rest of the general purpose registers */
    push r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push r16
    push r17
    push r18
    push r19
    push r20
    push r21
    push r22
    push r23
    push r24
    push r25
    push r26
    push r27
    push r28
    push r29
    push r30
    push r31

    /*
     * 2. Save current Hardware Stack Pointer (SP) into oldctx
     *
     * r25:r24 holds the pointer to `oldctx` (which is just a void* to SP).
     * We load this address into the Z pointer (r31:r30), then read the
     * AVR's actual SP and store it there.
     */
    movw r30, r24              /* Move oldctx pointer into Z register pair */
    in   r0, _SFR_IO_ADDR(SPL) /* Read low byte of actual SP */
    st   Z+, r0                /* Store it into oldctx->sp, increment Z */
    in   r0, _SFR_IO_ADDR(SPH) /* Read high byte of actual SP */
    st   Z,  r0                /* Store it into oldctx->sp (high byte) */

    /*
     * 3. Load the new thread's Stack Pointer (SP) into the Hardware SP
     *
     * r23:r22 holds the pointer to `newctx`.
     * We load this address into Z, read the saved SP, and write it
     * into the AVR's actual SP registers.
     */
    movw r30, r22              /* Move newctx pointer into Z register pair */
    ld   r0, Z+                /* Load low byte of saved SP, increment Z */
    out  _SFR_IO_ADDR(SPL), r0 /* Write to hardware SPL */
    ld   r0, Z                 /* Load high byte of saved SP */
    out  _SFR_IO_ADDR(SPH), r0 /* Write to hardware SPH */

    /*
     * 4. Pop the new thread's state off its stack
     */

    /* Pop general purpose registers (in reverse order) */
    pop  r31
    pop  r30
    pop  r29
    pop  r28
    pop  r27
    pop  r26
    pop  r25
    pop  r24
    pop  r23
    pop  r22
    pop  r21
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  r7
    pop  r6
    pop  r5
    pop  r4
    pop  r3
    pop  r2
    pop  r1

    /* Pop SREG */
    pop  r0
    out  _SFR_IO_ADDR(SREG), r0

    /* Pop r0 */
    pop  r0

    /*
     * 5. Return and execute new thread
     *
     * `ret` pops the saved Program Counter (PC) off the *new* stack
     * and jumps to it, successfully completing the context switch!
     */
    ret
