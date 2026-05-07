#ifndef __KERNEL_STREAMS_H
#define __KERNEL_STREAMS_H

#include "tinyos.h"
#include "util.h"

typedef struct file_ops {
    void* (*Open)(uint minor);
    int (*Read)(void* obj, char* buf, unsigned int size);
    int (*Write)(void* obj, const char* buf, unsigned int size);
    int (*Close)(void* obj);
} file_ops;

typedef struct file_control_block {
    void* streamobj;
    file_ops* streamfunc;
    int refcount;
    rlnode freelist_node;
} FCB;

void initialize_files();
FCB* acquire_FCB();
void release_FCB(FCB* fcb);
void FCB_incref(FCB* fcb);
int FCB_decref(FCB* fcb);
int FCB_reserve(size_t num, Fid_t* fid, FCB** fcb);
void FCB_unreserve(size_t num, Fid_t* fid, FCB** fcb);

int sys_Read(Fid_t fd, char* buf, unsigned int size);
int sys_Write(Fid_t fd, const char* buf, unsigned int size);
int sys_Close(Fid_t fd);
Fid_t sys_OpenTerminal(unsigned int termno);
Fid_t sys_OpenNull();

#endif
