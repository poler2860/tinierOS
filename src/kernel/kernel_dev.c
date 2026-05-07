#include "kernel_dev.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "cpu.h"
#include "util.h"
#include <string.h>

typedef struct serial_dcb_s {
    uint devno;
    CondVar rx_ready;
} serial_dcb_t;

static serial_dcb_t serial_dcbs[MAX_TERMINALS];

static void* nulldev_open(uint minor) { (void)minor; return NULL; }
static int nulldev_read(void* obj, char* buf, unsigned int size) { 
    (void)obj; memset(buf, 0, size); return size; 
}
static int nulldev_write(void* obj, const char* buf, unsigned int size) { 
    (void)obj; (void)buf; return size; 
}
static int nulldev_close(void* obj) { (void)obj; return 0; }

static file_ops nulldev_fops = {
    .Open = nulldev_open,
    .Read = nulldev_read,
    .Write = nulldev_write,
    .Close = nulldev_close
};

static void serial_rx_handler() {
    /* For now, we poll in the Read call, so just return.
     * In a later version, we will use a buffer and signal a Condition Variable.
     */
}

static void* serial_open(uint minor) {
    if (minor >= MAX_TERMINALS) return NULL;
    return &serial_dcbs[minor];
}

static int serial_read(void* obj, char* buf, unsigned int size) {
    serial_dcb_t* dcb = (serial_dcb_t*)obj;
    unsigned int count = 0;
    while (count < size) {
        if (bios_read_serial(dcb->devno, &buf[count])) {
            count++;
        } else {
            if (count > 0) break;
            /* Wait for data */
            yield(SCHED_IO);
        }
    }
    return count;
}

static int serial_write(void* obj, const char* buf, unsigned int size) {
    serial_dcb_t* dcb = (serial_dcb_t*)obj;
    unsigned int count = 0;
    while (count < size) {
        if (bios_write_serial(dcb->devno, buf[count])) {
            count++;
        } else {
            yield(SCHED_IO);
        }
    }
    return count;
}

static int serial_close(void* obj) { (void)obj; return 0; }

static file_ops serial_fops = {
    .Open = serial_open,
    .Read = serial_read,
    .Write = serial_write,
    .Close = serial_close
};

static DCB devtable[DEV_MAX];

void initialize_devices() {
    devtable[DEV_NULL].type = DEV_NULL;
    devtable[DEV_NULL].devnum = 1;
    devtable[DEV_NULL].dev_fops = nulldev_fops;

    devtable[DEV_SERIAL].type = DEV_SERIAL;
    devtable[DEV_SERIAL].devnum = MAX_TERMINALS;
    devtable[DEV_SERIAL].dev_fops = serial_fops;

    for (int i = 0; i < MAX_TERMINALS; i++) {
        serial_dcbs[i].devno = i;
        serial_dcbs[i].rx_ready = COND_INIT;
    }

    cpu_interrupt_handler(SERIAL_RX_READY, serial_rx_handler);
}

int device_open(Device_type major, uint minor, void** obj, file_ops** ops) {
    if (major >= DEV_MAX || minor >= devtable[major].devnum) return -1;
    *obj = devtable[major].dev_fops.Open(minor);
    *ops = &devtable[major].dev_fops;
    return 0;
}

uint device_no(Device_type major) {
    if (major >= DEV_MAX) return 0;
    return devtable[major].devnum;
}
