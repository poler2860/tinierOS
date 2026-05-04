
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "tinyos.h"
#include <string.h>
// #include <assert.h>

/*
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB *get_pcb(Pid_t pid) { return PT[pid].pstate == FREE ? NULL : &PT[pid]; }

Pid_t get_pid(PCB *pcb) { return pcb == NULL ? NOPROC : pcb - PT; }

/* Initialize a PCB */
static inline void initialize_PCB(PCB *pcb) {
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for (int i = 0; i < MAX_FILEID; i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(&pcb->children_list, NULL);
  rlnode_init(&pcb->exited_list, NULL);
  rlnode_init(&pcb->children_node, pcb);
  rlnode_init(&pcb->exited_node, pcb);
  rlnode_init(&pcb->ptcb_list, NULL);
  pcb->thread_count = 0;
  pcb->child_exit = COND_INIT;
}

static PCB *pcb_freelist;

void initialize_processes() {
  /* initialize the PCBs */
  for (Pid_t p = 0; p < MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB *pcbiter;
  pcb_freelist = NULL;
  for (pcbiter = PT + MAX_PROC; pcbiter != PT;) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if (Exec(NULL, 0, NULL) != 0)
    FATAL("The scheduler process does not have pid==0");
}

/*
  Must be called with kernel_mutex held
*/
PCB *acquire_PCB() {
  PCB *pcb = NULL;

  if (pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB *pcb) {
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}

/*
 *
 * Process creation
 *
 */

/*
        This function is provided as an argument to spawn,
        to execute the main thread of a process.
*/
void start_main_thread() {
  int exitval;

  Task call = CURPROC->main_task;
  int argl = CURPROC->argl;
  void *args = CURPROC->args;

  exitval = call(argl, args);
  Exit(exitval);
}

/*
        System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void *args) {
  PCB *curproc, *newproc;

  /* The new process PCB */
  newproc = acquire_PCB();

  if (newproc == NULL)
    goto finish; /* We have run out of PIDs! */

  if (get_pid(newproc) <= 1) {
    /* Processes with pid<=1 (the scheduler and the init process)
       are parentless and are treated specially. */
    newproc->parent = NULL;
  } else {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(&curproc->children_list, &newproc->children_node);

    /* Inherit file streams from parent */
    for (int i = 0; i < MAX_FILEID; i++) {
      newproc->FIDT[i] = curproc->FIDT[i];
      if (newproc->FIDT[i])
        FCB_incref(newproc->FIDT[i]);
    }
  }

  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  newproc->args = xmalloc(argl);
  memcpy(newproc->args, args, argl);

  // if (args != NULL) {
  //   newproc->args = malloc(argl);
  //   memcpy(newproc->args, args, argl);
  // } else
  //   newproc->args = NULL;

  /*
      Create and wake up the thread for the main function. This must be the last
      thing we do, because once we wakeup the new thread it may run! so we need
      to have finished the initialization of the PCB.

   */
  if (call != NULL) {
    // newproc->main_thread = init_thread(newproc->main_thread,
    // newproc,start_main_thread, call, argl, args);

    newproc->main_thread = spawn_thread(newproc, start_main_thread);
    create_ptcb(newproc->main_thread, call, argl, args);
    newproc->thread_count++;
    wakeup(newproc->main_thread);
  }

finish:
  return get_pid(newproc);
}

/* System call */
Pid_t sys_GetPid() { return get_pid(CURPROC); }

Pid_t sys_GetPPid() { return get_pid(CURPROC->parent); }

static void cleanup_zombie(PCB *pcb, int *status) {
  if (status != NULL)
    *status = pcb->exitval;

  rlist_remove(&pcb->children_node);
  rlist_remove(&pcb->exited_node);

  release_PCB(pcb);
}

static Pid_t wait_for_specific_child(Pid_t cpid, int *status) {

  /* Legality checks */
  if ((cpid < 0) || (cpid >= MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB *parent = CURPROC;
  PCB *child = get_pcb(cpid);
  if (child == NULL || child->parent != parent) {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while (child->pstate == ALIVE)
    kernel_wait(&parent->child_exit, SCHED_USER);

  cleanup_zombie(child, status);

finish:
  return cpid;
}

static Pid_t wait_for_any_child(int *status) {
  Pid_t cpid;

  PCB *parent = CURPROC;

  /* Make sure I have children! */
  int no_children, has_exited;
  while (1) {
    no_children = is_rlist_empty(&parent->children_list);
    if (no_children)
      break;

    has_exited = !is_rlist_empty(&parent->exited_list);
    if (has_exited)
      break;

    kernel_wait(&parent->child_exit, SCHED_USER);
  }

  if (no_children)
    return NOPROC;

  PCB *child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

  return cpid;
}

Pid_t sys_WaitChild(Pid_t cpid, int *status) {
  /* Wait for specific child. */
  if (cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }
}

void sys_Exit(int exitval) {

  PCB *curproc = CURPROC; /* cache for efficiency */

  /* First, store the exit status */
  curproc->exitval = exitval;

  /*
    Here, we must check that we are not the init task.
    If we are, we must wait until all child processes exit.
   */
  if (get_pid(curproc) == 1) {
    while (sys_WaitChild(NOPROC, NULL) != NOPROC)
      ;
  }

  sys_ThreadExit(exitval);
}

static file_ops procinfo_fo = {
    .Read = procinfo_read, .Write = procinfo_write, .Close = procinfo_close};

// Not making use of the write function so return -1
int procinfo_write() { return -1; }

int procinfo_read(void *pi, char *buf, unsigned int n) {
  // If PCB is null return error code (-1)
  if (pi == NULL) {
    return -1;
  }

  procinfo_cb *pi_cb = (procinfo_cb *)pi;
  // If cursor is invalid return error code (-1)
  if (pi_cb->cursor < 1 || pi_cb->cursor > MAX_PROC) {
    return -1;
  }

  if (pi_cb->cursor == MAX_PROC) {
    return 0;
  }

  // Search for the next available proccess
  while (PT[pi_cb->cursor].pstate == FREE) {
    pi_cb->cursor++;
    if (pi_cb->cursor == MAX_PROC) {
      return 0;
    }
  }

  // Get all pcbs information
  pi_cb->info.pid = pi_cb->cursor;
  pi_cb->info.ppid = get_pid((PT[pi_cb->cursor]).parent);
  pi_cb->info.alive = PT[pi_cb->cursor].pstate == ALIVE;
  pi_cb->info.thread_count = PT[pi_cb->cursor].thread_count;
  pi_cb->info.main_task = PT[pi_cb->cursor].main_task;
  pi_cb->info.argl = PT[pi_cb->cursor].argl;

  // Argl if greater than the max size is equal to the max size
  if (pi_cb->info.argl > PROCINFO_MAX_ARGS_SIZE) {
    memcpy(pi_cb->info.args, (char *)PT[pi_cb->cursor].args,
           sizeof(char) * PROCINFO_MAX_ARGS_SIZE);       // name
    memcpy(buf, (char *)&pi_cb->info, sizeof(procinfo)); // info
  } else {
    memcpy(pi_cb->info.args, (char *)PT[pi_cb->cursor].args,
           sizeof(char) * pi_cb->info.argl);
    memcpy(buf, (char *)&pi_cb->info, sizeof(procinfo));
  }

  // Move cursor to next proccess
  pi_cb->cursor++;
  return 1;
}

int procinfo_close(void *procinfo_cb) {
  if (procinfo_cb != NULL) {
    free(procinfo_cb);
  }
  return 0;
}

Fid_t sys_OpenInfo() {
  Fid_t fid;
  FCB *fcb;

  // If fcb reservation fails return
  if (!FCB_reserve(1, &fid, &fcb)) {
    return NOFILE;
  }

  // Allocate memory for the procinfo
  procinfo_cb *pi_cb = xmalloc(sizeof(procinfo_cb));
  // If null then return (error)
  if (pi_cb == NULL) {
    FCB_unreserve(1, &fid, &fcb);
    return NOFILE;
  }
  pi_cb->cursor = 1; // Cursor is 1 so that we start from root
  fcb->streamobj = pi_cb;
  fcb->streamfunc = &procinfo_fo;

  return fid;
}
