#include "kernel_proc.h"
#include "kernel_sys.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "tinyos.h"
#include "util.h"
#include <string.h>

/* The process table */
static PCB PT[MAX_PROC];

/* Static argument buffers for each process */
#define MAX_ARG_SIZE 32
static uint8_t proc_args[MAX_PROC][MAX_ARG_SIZE];

PCB *get_pcb(Pid_t pid) { 
    if (pid < 0 || pid >= MAX_PROC) return NULL;
    return PT[pid].pstate == FREE ? NULL : &PT[pid]; 
}

Pid_t get_pid(PCB *pcb) { 
    if (pcb == NULL) return NOPROC;
    return pcb - PT; 
}

/* Initialize a PCB */
static void initialize_PCB(PCB *pcb) {
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

void initialize_processes() {
  for (Pid_t p = 0; p < MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }
  
  /* Reserve PID 0 for the system/idle process */
  PT[0].pstate = ALIVE;
  PT[0].parent = NULL;
}

PCB *acquire_PCB() {
  for (Pid_t p = 0; p < MAX_PROC; p++) {
    if (PT[p].pstate == FREE) {
      PT[p].pstate = ALIVE;
      return &PT[p];
    }
  }
  return NULL;
}

void release_PCB(PCB *pcb) {
  pcb->pstate = FREE;
}

/*
        This function is provided as an argument to spawn,
        to execute the main thread of a process.
*/
static void start_main_thread() {
  int exitval;
  PCB *pcb = cur_thread()->owner_pcb;

  Task call = pcb->main_task;
  int argl = pcb->argl;
  void *args = pcb->args;

  exitval = call(argl, args);
  Exit(exitval);
}

/*
        System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void *args) {
  PCB *curproc, *newproc;

  newproc = acquire_PCB();
  if (newproc == NULL) return NOPROC;

  Pid_t newpid = get_pid(newproc);

  if (newpid <= 1) {
    newproc->parent = NULL;
  } else {
    /* Get current PCB safely */
    TCB* cur = cur_thread();
    if (cur && cur->owner_pcb) {
        curproc = cur->owner_pcb;
        newproc->parent = curproc;
        rlist_push_front(&curproc->children_list, &newproc->children_node);

        /* Inherit file descriptors (stubs) */
        for (int i = 0; i < MAX_FILEID; i++) {
          newproc->FIDT[i] = curproc->FIDT[i];
        }
    } else {
        newproc->parent = NULL;
    }
  }

  newproc->main_task = call;
  newproc->argl = argl;
  
  if (argl > 0 && args != NULL) {
      int copy_size = (argl > MAX_ARG_SIZE) ? MAX_ARG_SIZE : argl;
      memcpy(proc_args[newpid], args, copy_size);
      newproc->args = proc_args[newpid];
  } else {
      newproc->args = NULL;
  }

  if (call != NULL) {
    newproc->main_thread = spawn_thread(newproc, start_main_thread);
    if (newproc->main_thread == NULL) {
        release_PCB(newproc);
        return NOPROC;
    }
    create_ptcb(newproc->main_thread, call, argl, newproc->args);
    newproc->thread_count++;
    wakeup(newproc->main_thread);
  }

  return newpid;
}

Pid_t sys_GetPid() { return get_pid(cur_thread()->owner_pcb); }

Pid_t sys_GetPPid() { 
    PCB *parent = cur_thread()->owner_pcb->parent;
    return get_pid(parent); 
}

static void cleanup_zombie(PCB *pcb, int *status) {
  if (status != NULL)
    *status = pcb->exitval;

  rlist_remove(&pcb->children_node);
  rlist_remove(&pcb->exited_node);

  release_PCB(pcb);
}

Pid_t sys_WaitChild(Pid_t cpid, int *status) {
  PCB *parent = cur_thread()->owner_pcb;

  if (cpid != NOPROC) {
      /* Wait for specific child */
      PCB *child = get_pcb(cpid);
      if (child == NULL || child->parent != parent) return NOPROC;
      
      while (child->pstate == ALIVE) {
          /* In a real Mesa-style CV, we would sleep here. 
             Since we have a single core and cooperative-ish for now, 
             or preemptive, we can use Cond_Wait if implemented.
          */
          yield(SCHED_USER); 
      }
      cleanup_zombie(child, status);
      return cpid;
  } else {
      /* Wait for any child */
      while (is_rlist_empty(&parent->children_list)) {
          /* No children at all? */
          return NOPROC;
      }
      
      while (is_rlist_empty(&parent->exited_list)) {
          yield(SCHED_USER);
      }
      
      PCB *child = parent->exited_list.next->pcb;
      Pid_t pid = get_pid(child);
      cleanup_zombie(child, status);
      return pid;
  }
}

void sys_Exit(int exitval) {
  PCB *curproc = cur_thread()->owner_pcb;
  curproc->exitval = exitval;
  curproc->pstate = ZOMBIE;

  if (curproc->parent) {
      rlist_remove(&curproc->children_node);
      rlist_push_back(&curproc->parent->exited_list, &curproc->exited_node);
  }

  /* For now, just exit the current thread */
  sys_ThreadExit(exitval);
}

/* 
 * Threads 
 */

void start_main_pthread() {
  int exitval;
  Task call = cur_thread()->ptcb->task;
  int argl = cur_thread()->ptcb->argl;
  void *args = cur_thread()->ptcb->args;

  exitval = call(argl, args);
  sys_ThreadExit(exitval);
}

Tid_t sys_CreateThread(Task task, int argl, void *args) {
  if (task == NULL) return NOTHREAD;

  PCB *cur_pcb = cur_thread()->owner_pcb;
  TCB *new_tcb = spawn_thread(cur_pcb, start_main_pthread);
  if (new_tcb == NULL) return NOTHREAD;

  create_ptcb(new_tcb, task, argl, args);
  cur_pcb->thread_count++;
  wakeup(new_tcb);

  return (Tid_t)(new_tcb->ptcb);
}

Tid_t sys_ThreadSelf() { return (Tid_t)cur_thread()->ptcb; }

int sys_ThreadJoin(Tid_t tid, int *exitval) {
  PTCB *ptcb = (PTCB *)tid;
  if (ptcb == NULL) return -1;
  
  while (!ptcb->exited) {
      yield(SCHED_USER);
  }

  if (exitval != NULL) *exitval = ptcb->exitval;
  return 0;
}

int sys_ThreadDetach(Tid_t tid) {
  PTCB *ptcb = (PTCB *)tid;
  if (ptcb == NULL) return -1;
  ptcb->detached = 1;
  return 0;
}

void sys_ThreadExit(int exitval) {
  TCB *tcb = cur_thread();
  PTCB *ptcb = tcb->ptcb;

  if (ptcb) {
      ptcb->exitval = exitval;
      ptcb->exited = 1;
  }

  tcb->owner_pcb->thread_count--;
  
  /* Mark thread for scheduler to release */
  sleep_releasing(EXITED, NULL, SCHED_USER, NO_TIMEOUT);
}
