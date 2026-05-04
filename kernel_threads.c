#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "tinyos.h"
#include "util.h"

void start_main_pthread() {
  int exitval;

  // TCB *cur_tcb = cur_thread();
  Task call = cur_thread()->ptcb->task;
  int argl = cur_thread()->ptcb->argl;
  void *args = cur_thread()->ptcb->args;

  exitval = call(argl, args);
  ThreadExit(exitval);
}

/**
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void *args) {
  /*Return NOTHREAD if the task is null*/
  if (task == NULL)
    return NOTHREAD;

  PCB *cur_pcb = CURPROC;
  TCB *cur_tcb = spawn_thread(CURPROC, start_main_pthread);
  // Create a new pthread
  create_ptcb(cur_tcb, task, argl, args);
  cur_pcb->thread_count++;
  wakeup(cur_tcb);

  return (Tid_t)(cur_tcb->ptcb);
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf() { return (Tid_t)cur_thread()->ptcb; }

/**
  @brief Join the given thread.
 */
int sys_ThreadJoin(Tid_t tid, int *exitval) {

  /*Probably should not join itself */
  if (sys_ThreadSelf() == tid)
    return -1;

  rlnode *tmp = rlist_find(&CURPROC->ptcb_list, (PTCB *)tid, NULL);

  if (tmp == NULL)
    return -1;

  PTCB *ptcb = tmp->ptcb;

  /*Check if the thread is detached or exited*/
  if (ptcb->detached == 1)
    return -1;

  ptcb->refcount++;

  /*Wait for the thread to exit*/
  while (ptcb->exited != 1 && ptcb->detached != 1)
    kernel_wait(&ptcb->exit_cv, SCHED_USER);

  if (ptcb->detached == 1)
    return -1;

  ptcb->refcount--;

  if (exitval != NULL)
    *exitval = ptcb->exitval;

  // Clean up the zombie
  if (ptcb->refcount == 0) {
    rlist_remove(&ptcb->ptcb_node_list);
    free(ptcb);
  }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid) {
  PTCB *ptcb = NULL;

  rlnode *tmp = rlist_find(
      &CURPROC->ptcb_list, (PTCB *)tid,
      NULL); // search the ptcb_list for the given tid (see if it exists)

  if (tmp == NULL)
    return -1;

  ptcb = tmp->ptcb;

  /*Is the thread exited*/
  if (ptcb->exited == 1)
    return -1;

  /*Detach the thread*/
  ptcb->detached = 1;

  kernel_broadcast(&ptcb->exit_cv); // wakeup all waiting ptcbs

  // ptcb->refcount = 0; // set the refcount counter to 0 (no waiting ptcbs)

  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval) {

  // get current PCB and PTCB and decrement the thread count for the process
  PTCB *curptcb = cur_thread()->ptcb;

  // save the exitval, decrement the refcount and set the exited flag of the
  // PTCB
  curptcb->exitval = exitval;
  curptcb->exited = 1;

  // Wake up all threads waiting the current thread
  kernel_broadcast(&curptcb->exit_cv);

  PCB *curproc = CURPROC;
  curproc->thread_count--;

  if (curproc->thread_count == 0) {
    // if we are the last thread, do everything sys_Exit used to do in
    // the original project
    if (get_pid(curproc) != 1) {
      /* Reparent any children of the exiting process to the
        initial task */
      PCB *initpcb = get_pcb(1);
      while (!is_rlist_empty(&curproc->children_list)) {
        rlnode *child = rlist_pop_front(&curproc->children_list);
        child->pcb->parent = initpcb;
        rlist_push_front(&initpcb->children_list, child);
      }

      /* Add exited children to the initial task's exited list
         and signal the initial task */
      if (!is_rlist_empty(&curproc->exited_list)) {
        rlist_append(&initpcb->exited_list, &curproc->exited_list);
        kernel_broadcast(&initpcb->child_exit);
      }
      /* Put me into my parent's exited list */
      rlist_push_front(&curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(&curproc->parent->child_exit);
    }

    assert(is_rlist_empty(&curproc->children_list));
    assert(is_rlist_empty(&curproc->exited_list));

    /*
      Do all the other cleanup we want here, close files etc.
     */

    // free every ptcb since we dont need them anymore
    while (is_rlist_empty(&curproc->ptcb_list) != 0) {
      rlnode *temp;
      temp = rlist_pop_front(&curproc->ptcb_list);
      free(temp->ptcb);
    }

    /* Release the args data */
    if (curproc->args) {
      free(curproc->args);
      curproc->args = NULL;
    }

    /* Clean up FIDT */
    for (int i = 0; i < MAX_FILEID; i++) {
      if (curproc->FIDT[i] != NULL) {
        FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;
  }
  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER);
}
