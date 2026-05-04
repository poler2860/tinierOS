#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include "util.h"
#include "bios.h"

/*
	Native implementation of bios.h API
	(Single-core, non-VM)
*/

uint cpu_core_id = 0;
uint ncores = 1;
uint nterm = 0;
static interrupt_handler* intvec[maximum_interrupt_no];
static sigset_t bios_sigset;
static volatile int interrupts_enabled = 0;
static volatile uint32_t pending_interrupts = 0;

typedef struct {
	int fd_in;
	int fd_out;
} Terminal;

static Terminal TERM[MAX_TERMINALS];

static void dispatch_interrupts() {
	if (!interrupts_enabled) return;

	for (int i = 0; i < maximum_interrupt_no; i++) {
		if ((pending_interrupts & (1 << i)) && intvec[i]) {
			pending_interrupts &= ~(1 << i);
			int old_enabled = interrupts_enabled;
			interrupts_enabled = 0;
			intvec[i]();
			interrupts_enabled = old_enabled;
		}
	}
}

static void bios_signal_handler(int sig, siginfo_t* si, void* uc) {
	if (sig == SIGALRM) {
		pending_interrupts |= (1 << ALARM);
	}
	
	if (interrupts_enabled) {
		dispatch_interrupts();
	}
}

void bios_init(uint serialno) {
	nterm = serialno;
	sigemptyset(&bios_sigset);
	sigaddset(&bios_sigset, SIGALRM);

	struct sigaction sa;
	sa.sa_sigaction = bios_signal_handler;
	sa.sa_flags = SA_SIGINFO;
	sigfillset(&sa.sa_mask);
	if (sigaction(SIGALRM, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	for (uint i = 0; i < nterm; i++) {
		char kbd[16], con[16];
		snprintf(kbd, 16, "kbd%u", i);
		snprintf(con, 16, "con%u", i);
		TERM[i].fd_in = open(kbd, O_RDWR | O_NONBLOCK);
		TERM[i].fd_out = open(con, O_RDWR | O_NONBLOCK);
	}
}

uint cpu_cores() { return ncores; }

void cpu_core_barrier_sync() { /* No-op on single core */ }

void cpu_ici(uint core) {
	if (core == 0) {
		pending_interrupts |= (1 << ICI);
		if (interrupts_enabled) dispatch_interrupts();
	}
}

void cpu_interrupt_handler(Interrupt interrupt, interrupt_handler handler) {
	if (interrupt < maximum_interrupt_no) {
		intvec[interrupt] = handler;
	}
}

int cpu_disable_interrupts() {
	int old = interrupts_enabled;
	interrupts_enabled = 0;
	pthread_sigmask(SIG_BLOCK, &bios_sigset, NULL);
	return old;
}

void cpu_enable_interrupts() {
	interrupts_enabled = 1;
	pthread_sigmask(SIG_UNBLOCK, &bios_sigset, NULL);
	dispatch_interrupts();
}

int cpu_interrupts_enabled() {
	return interrupts_enabled;
}

void cpu_core_halt() {
	while (pending_interrupts == 0) {
		pause();
	}
	dispatch_interrupts();
}

void cpu_core_restart(uint c) { /* No-op on single core */ }
void cpu_core_restart_one() { /* No-op on single core */ }
void cpu_core_restart_all() { /* No-op on single core */ }

void cpu_initialize_context(cpu_context_t* ctx, void* ss_sp, size_t ss_size, void (*func)()) {
	getcontext(ctx);
	ctx->uc_link = NULL;
	ctx->uc_stack.ss_sp = ss_sp;
	ctx->uc_stack.ss_size = ss_size;
	ctx->uc_stack.ss_flags = 0;
	sigfillset(&ctx->uc_sigmask);
	makecontext(ctx, func, 0);
}

void cpu_swap_context(cpu_context_t* oldctx, cpu_context_t* newctx) {
	swapcontext(oldctx, newctx);
}

TimerDuration bios_set_timer(TimerDuration usec) {
	struct itimerval itv;
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = usec / 1000000;
	itv.it_value.tv_usec = usec % 1000000;
	
	struct itimerval old_itv;
	setitimer(ITIMER_REAL, &itv, &old_itv);
	return old_itv.it_value.tv_sec * 1000000 + old_itv.it_value.tv_usec;
}

TimerDuration bios_cancel_timer() {
	return bios_set_timer(0);
}

TimerDuration bios_clock() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

uint bios_serial_ports() {
	return nterm;
}

void bios_serial_interrupt_core(uint serial, Interrupt intno, uint core) {
	/* Simplified: everything goes to core 0 */
}

int bios_read_serial(uint serial, char* ptr) {
	if (serial >= nterm) return 0;
	int rc = read(TERM[serial].fd_in, ptr, 1);
	return rc == 1;
}

int bios_write_serial(uint serial, char value) {
	if (serial >= nterm) return 0;
	int rc = write(TERM[serial].fd_out, &value, 1);
	return rc == 1;
}
