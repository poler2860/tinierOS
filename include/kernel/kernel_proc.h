#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

#include "kernel_sched.h"
#include "kernel_streams.h"
#include "tinyos.h"

typedef enum pid_state_e {
  FREE,
  ALIVE,
  ZOMBIE
} pid_state;

typedef struct process_control_block {
  pid_state pstate;
  struct process_control_block *parent;
  int exitval;

  TCB *main_thread;
  Task main_task;
  int argl;
  void *args;

  rlnode children_list;
  rlnode exited_list;
  rlnode children_node;
  rlnode exited_node;

  CondVar child_exit;

  FCB *FIDT[MAX_FILEID];

  rlnode ptcb_list;
  int thread_count;
} PCB;

void initialize_processes();
PCB *get_pcb(Pid_t pid);
Pid_t get_pid(PCB *pcb);
PCB *acquire_PCB();
void release_PCB(PCB *pcb);

#endif
