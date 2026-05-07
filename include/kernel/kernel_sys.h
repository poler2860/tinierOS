#ifndef __KERNEL_SYS_H
#define __KERNEL_SYS_H

#include "tinyos.h"

Pid_t sys_Exec(Task task, int argl, void *args);
void sys_Exit(int val);
Pid_t sys_WaitChild(Pid_t pid, int *exitval);
Pid_t sys_GetPid();
Pid_t sys_GetPPid();

int sys_Read(Fid_t fd, char *buf, unsigned int size);
int sys_Write(Fid_t fd, const char *buf, unsigned int size);
int sys_Close(int fd);
Fid_t sys_OpenTerminal(unsigned int termno);
Fid_t sys_OpenNull();

Tid_t sys_CreateThread(Task task, int argl, void *args);
Tid_t sys_ThreadSelf();
int sys_ThreadJoin(Tid_t tid, int *exitval);
int sys_ThreadDetach(Tid_t tid);
void sys_ThreadExit(int exitval);

#endif
