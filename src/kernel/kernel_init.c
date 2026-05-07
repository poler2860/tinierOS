#include "io.h"
#include <stdint.h>

#define PB5 5
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_sys.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "cpu.h"
#include "tinyos.h"

// !NOTE: for test use
static void delay(volatile uint32_t count) {
    while (count--) {
        __asm__ __volatile__ ("nop");
    }
}

static int test_process(int argl, void* args) {
    (void)argl; (void)args;
    Fid_t fd = OpenTerminal(0);
    const char* msg = "Hello from tinierOS!\r\n";
    
    while (1) {
        PORTB |= (1 << PB5);
        Write(fd, msg, 22);
        delay(4000000); /* Approx 1 second */
        PORTB &= ~(1 << PB5);
        delay(4000000); /* Approx 1 second */
    }
    return 0;
}


int main(void) {
    /* Initialize Hardware */
    DDRB |= (1 << PB5);   /* set bit 5 = output direction */
    PORTB &= ~(1 << PB5); /* LED OFF */

    /* Double flash heartbeat to signal boot completion */
    for (int i=0; i<4; i++) {
        PORTB ^= (1 << PB5);
        delay(100000);
    }
    PORTB &= ~(1 << PB5);

    bios_init(0);

    /* Initialize Kernel */
    initialize_scheduler();
    initialize_processes();
    initialize_files();
    initialize_devices();

    /* Start the first process */
    sys_Exec(test_process, 0, NULL);

    /* Start Scheduler (runs idle thread) */
    /* Note: run_scheduler will call preempt_on when ready */
    run_scheduler();

    return 0;
}
