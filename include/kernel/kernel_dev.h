#ifndef __KERNEL_DEV_H
#define __KERNEL_DEV_H

#include "kernel_streams.h"

typedef enum {
    DEV_NULL,
    DEV_SERIAL,
    DEV_MAX
} Device_type;

typedef struct device_control_block {
    Device_type type;
    uint devnum;
    file_ops dev_fops;
} DCB;

void initialize_devices();
int device_open(Device_type major, uint minor, void** obj, file_ops** ops);
uint device_no(Device_type major);

#endif
