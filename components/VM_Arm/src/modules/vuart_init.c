/*
 * Copyright 2019, DornerWorks
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <vmlinux.h>
#include <camkes.h>

#include <sel4vmmplatsupport/plat/devices.h>

//this matches the size of the buffer in serial server
/* 4088 because the serial_shmem_t struct has to be 0x1000 bytes big */
#define BUFSIZE (0x1000 - 2 * sizeof(uint32_t))

typedef struct serial_shmem {
    uint32_t head;
    uint32_t tail;
    char buf[BUFSIZE];
} serial_shmem_t;
compile_time_assert(serial_shmem_4k_size, sizeof(serial_shmem_t) == 0x1000);


extern void *serial_getchar_buf WEAK;
extern serial_shmem_t *batch_buf WEAK;
seL4_Word serial_getchar_notification_badge(void) WEAK;


static int handle_serial_console(vm_t *vmm, void *cookie UNUSED)
{
    struct {
        uint32_t head;
        uint32_t tail;
        char buf[BUFSIZE];
    } volatile *buffer = serial_getchar_buf;
    char c;
    while (buffer->head != buffer->tail) {
        c = buffer->buf[buffer->head];
        buffer->head = (buffer->head + 1) % sizeof(buffer->buf);
        vuart_handle_irq(c);
    }
}

static void handle_serial_putchar(int c)
{
    serial_shmem_t *buffer = batch_buf;
    buffer->buf[buffer->tail] = c;
    __atomic_thread_fence(__ATOMIC_ACQ_REL);
    buffer->tail = (buffer->tail + 1) % BUFSIZE;
    batch_batch();
}

/* Install vuart */
static void vconsole_init_module(vm_t *vm, void *cookie)
{
    if (!serial_getchar_notification_badge || !batch_buf || !batch_batch) {
        // Interface is not connected
        if (vm->is_multikernel) {
            // We're a multikernel and the serial interface is on a different core..
            vm_install_vconsole(vm, NULL);

        }
        return;
    }
    int err = register_async_event_handler(serial_getchar_notification_badge(), handle_serial_console, NULL);
    ZF_LOGF_IF(err, "Failed to register_async_event_handler for make_virtio_con.");
    vm_install_vconsole(vm, handle_serial_putchar);
}

DEFINE_MODULE(vconsole, NULL, vconsole_init_module)
