#include "kernel_sys.h"
#include "kernel_cc.h"

Pid_t Exec(Task task, int argl, void *args) {
    Pid_t ret;
    kernel_lock();
    ret = sys_Exec(task, argl, args);
    kernel_unlock();
    return ret;
}

void Exit(int val) {
    kernel_lock();
    sys_Exit(val);
    kernel_unlock();
}

Pid_t WaitChild(Pid_t pid, int *exitval) {
    Pid_t ret;
    kernel_lock();
    ret = sys_WaitChild(pid, exitval);
    kernel_unlock();
    return ret;
}

Pid_t GetPid() {
    Pid_t ret;
    kernel_lock();
    ret = sys_GetPid();
    kernel_unlock();
    return ret;
}
Pid_t GetPPid() {
    Pid_t ret;
    kernel_lock();
    ret = sys_GetPPid();
    kernel_unlock();
    return ret;
}

int Read(Fid_t fd, char *buf, unsigned int size) {
    int ret;
    kernel_lock();
    ret = sys_Read(fd, buf, size);
    kernel_unlock();
    return ret;
}

int Write(Fid_t fd, const char *buf, unsigned int size) {
    int ret;
    kernel_lock();
    ret = sys_Write(fd, buf, size);
    kernel_unlock();
    return ret;
}

int Close(int fd) {
    int ret;
    kernel_lock();
    ret = sys_Close(fd);
    kernel_unlock();
    return ret;
}

Fid_t OpenTerminal(unsigned int termno) {
    Fid_t ret;
    kernel_lock();
    ret = sys_OpenTerminal(termno);
    kernel_unlock();
    return ret;
}

Fid_t OpenNull() {
    Fid_t ret;
    kernel_lock();
    ret = sys_OpenNull();
    kernel_unlock();
    return ret;
}

Tid_t CreateThread(Task task, int argl, void *args) {
    Tid_t ret;
    kernel_lock();
    ret = sys_CreateThread(task, argl, args);
    kernel_unlock();
    return ret;
}

Tid_t ThreadSelf() {
    Tid_t ret;
    kernel_lock();
    ret = sys_ThreadSelf();
    kernel_unlock();
    return ret;
}

int ThreadJoin(Tid_t tid, int *exitval) {
    int ret;
    kernel_lock();
    ret = sys_ThreadJoin(tid, exitval);
    kernel_unlock();
    return ret;
}

int ThreadDetach(Tid_t tid) {
    int ret;
    kernel_lock();
    ret = sys_ThreadDetach(tid);
    kernel_unlock();
    return ret;
}

void ThreadExit(int exitval) {
    kernel_lock();
    sys_ThreadExit(exitval);
    kernel_unlock();
}
