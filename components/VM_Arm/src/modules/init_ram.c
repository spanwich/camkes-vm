/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <camkes.h>
#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_memory_helpers.h>
#include <sel4vm/guest_ram.h>
#include <sel4vmmplatsupport/guest_memory_util.h>
#include <vmlinux.h>

extern dataport_caps_handle_t memdev_handle;

uintptr_t vm_paddr_to_host_vaddr(uintptr_t paddr) {
    return (paddr - vm_config.ram.base) + (uintptr_t) memdev;
}

static vm_frame_t dataport_memory_iterator(uintptr_t addr, void *cookie) {
    int ret;
    int error;

    vm_frame_t frame_result = { seL4_CapNull, seL4_NoRights, 0, 0 };
    uintptr_t frame_start = ROUND_DOWN(addr, BIT(vm_config.ram.page_size_bits));
    if (frame_start < vm_config.ram.base || frame_start > vm_config.ram.base + vm_config.ram.size) {
        ZF_LOGE("Error: Not dataport ram region");
        return frame_result;
    }
    int page_idx = (frame_start - vm_config.ram.base) / BIT(vm_config.ram.page_size_bits);
    frame_result.cptr = dataport_get_nth_frame_cap(&memdev_handle, page_idx);
    frame_result.rights = seL4_AllRights;
    frame_result.vaddr = frame_start;
    frame_result.size_bits = vm_config.ram.page_size_bits;

    return frame_result;
}

void WEAK init_ram_module(vm_t *vm, void *cookie)
{
    int err = vm_ram_register_at_custom_iterator(vm,
                                 vm_config.ram.base,
                                 vm_config.ram.size,
                                 dataport_memory_iterator, NULL);
    assert(!err);
}

DEFINE_MODULE(init_ram, NULL, init_ram_module)
