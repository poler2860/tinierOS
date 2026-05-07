#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_sched.h"
#include "kernel_dev.h"
#include "tinyos.h"
#include "util.h"

#define MAX_FILES 4
static FCB FT[MAX_FILES];
static rlnode FCB_freelist;

void initialize_files() {
  rlnode_init(&FCB_freelist, NULL);
  for (int i = 0; i < MAX_FILES; i++) {
    FT[i].refcount = 0;
    rlnode_init(&FT[i].freelist_node, &FT[i]);
    rlist_push_back(&FCB_freelist, &FT[i].freelist_node);
  }
}

FCB *acquire_FCB() {
  if (!is_rlist_empty(&FCB_freelist)) {
    FCB *fcb = rlist_pop_front(&FCB_freelist)->fcb;
    fcb->refcount = 0;
    return fcb;
  } else
    return NULL;
}

void release_FCB(FCB *fcb) {
  rlist_push_back(&FCB_freelist, &fcb->freelist_node);
}

void FCB_incref(FCB *fcb) {
  if (fcb) fcb->refcount++;
}

int FCB_decref(FCB *fcb) {
  if (!fcb) return 0;
  fcb->refcount--;
  if (fcb->refcount == 0) {
    int retval = 0;
    if (fcb->streamfunc && fcb->streamfunc->Close)
        retval = fcb->streamfunc->Close(fcb->streamobj);
    release_FCB(fcb);
    return retval;
  }
  return 0;
}

int FCB_reserve(size_t num, Fid_t *fid, FCB **fcb) {
  PCB *cur = cur_thread()->owner_pcb;
  size_t f = 0;
  uint i;

  for (i = 0; i < num; i++) {
    while (f < MAX_FILEID && cur->FIDT[f] != NULL)
      f++;
    if (f == MAX_FILEID)
      break;
    fid[i] = f;
    f++;
  }
  if (i < num)
    return 0;

  for (i = 0; i < num; i++)
    if ((fcb[i] = acquire_FCB()) == NULL)
      break;

  if (i < num) {
    while (i > 0) {
      release_FCB(fcb[i - 1]);
      i--;
    }
    return 0;
  }

  for (i = 0; i < num; i++) {
    cur->FIDT[fid[i]] = fcb[i];
    FCB_incref(fcb[i]);
  }
  return 1;
}

void FCB_unreserve(size_t num, Fid_t *fid, FCB **fcb) {
  PCB *cur = cur_thread()->owner_pcb;
  for (size_t i = 0; i < num; i++) {
    cur->FIDT[fid[i]] = NULL;
    release_FCB(fcb[i]);
  }
}

FCB *get_stream_fcb(Fid_t fid) {
  if (fid < 0 || fid >= MAX_FILEID)
    return NULL;
  return cur_thread()->owner_pcb->FIDT[fid];
}

int sys_Read(Fid_t fd, char *buf, unsigned int size) {
  FCB *fcb = get_stream_fcb(fd);
  if (fcb && fcb->streamfunc && fcb->streamfunc->Read) {
    return fcb->streamfunc->Read(fcb->streamobj, buf, size);
  }
  return -1;
}

int sys_Write(Fid_t fd, const char *buf, unsigned int size) {
  FCB *fcb = get_stream_fcb(fd);
  if (fcb && fcb->streamfunc && fcb->streamfunc->Write) {
    return fcb->streamfunc->Write(fcb->streamobj, buf, size);
  }
  return -1;
}

int sys_Close(int fd) {
  FCB *fcb = get_stream_fcb(fd);
  if (fcb) {
    cur_thread()->owner_pcb->FIDT[fd] = NULL;
    return FCB_decref(fcb);
  }
  return 0;
}

Fid_t open_stream(Device_type major, unsigned int minor) {
  Fid_t fid;
  FCB *fcb;

  if (!FCB_reserve(1, &fid, &fcb))
    return NOFILE;

  if (device_open(major, minor, &fcb->streamobj, &fcb->streamfunc)) {
    FCB_unreserve(1, &fid, &fcb);
    return NOFILE;
  }

  return fid;
}

Fid_t sys_OpenNull() { return open_stream(DEV_NULL, 0); }

Fid_t sys_OpenTerminal(unsigned int termno) {
  return open_stream(DEV_SERIAL, termno);
}
