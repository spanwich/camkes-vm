/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <libfdt.h>
#include <utils/util.h>

int fdt_generate_memory_node(void *fdt, uintptr_t base, size_t size)
{
    int root_offset = fdt_path_offset(fdt, "/");
    int address_cells = fdt_address_cells(fdt, root_offset);
    int size_cells = fdt_size_cells(fdt, root_offset);

    int this = fdt_add_subnode(fdt, root_offset, "memory");
    if (this < 0) {
        return this;
    }
    int err = fdt_appendprop_string(fdt, this, "device_type", "memory");
    if (err) {
        return err;
    }
    err = fdt_appendprop_uint(fdt, this, "reg", base, address_cells);
    if (err) {
        return err;
    }
    err = fdt_appendprop_uint(fdt, this, "reg", size, size_cells);
    if (err) {
        return err;
    }

    return 0;
}

int fdt_generate_chosen_node(void *fdt, const char *stdout_path, const char *bootargs, const unsigned int maxcpus)
{
    int root_offset = fdt_path_offset(fdt, "/");
    int this = fdt_add_subnode(fdt, root_offset, "chosen");
    int err;

    if (stdout_path && strlen(stdout_path) > 0) {
        err = fdt_appendprop_string(fdt, this, "stdout-path", stdout_path);
        if (err) {
            return err;
        }
        err = fdt_appendprop_string(fdt, this, "linux,stdout-path", stdout_path);
        if (err) {
            return err;
        }
    }

    size_t bootargs_len = strlen(bootargs);
    /*  +3*sizeof(int) is a cheap approximated formula for maximum number of characters in a UINT_MAX
     *  +1 for null character, +9 for ' maxcpus='
     */
    size_t updated_bootargs_len = bootargs_len + 9 + (3 * sizeof(unsigned int) + 1);
    char *updated_bootargs = calloc(1, updated_bootargs_len);
    if (!updated_bootargs) {
        ZF_LOGE("Failed to generate chosen node: Unable to allocate updated bootargs");
        return err;
    }
    int res = snprintf(updated_bootargs, updated_bootargs_len, "%s maxcpus=%u", bootargs, maxcpus);
    if (res < 0) {
        ZF_LOGE("Failed to generate chosen node: Unable to allocate updated bootargs");
        free(updated_bootargs);
        return -1;
    }

    err = fdt_appendprop_string(fdt, this, "bootargs", updated_bootargs);
    if (err) {
        ZF_LOGE("Failed to generate chosen node: Unable to create updated bootargs");
        free(updated_bootargs);
        return err;
    }
    free(updated_bootargs);

    return 0;
}

int fdt_append_chosen_node_with_initrd_info(void *fdt, uintptr_t base, size_t size)
{
    int root_offset = fdt_path_offset(fdt, "/");
    int address_cells = fdt_address_cells(fdt, root_offset);
    int this = fdt_path_offset(fdt, "/chosen");
    int err = fdt_appendprop_uint(fdt, this, "linux,initrd-start", base, address_cells);
    if (err) {
        return err;
    }
    err = fdt_appendprop_uint(fdt, this, "linux,initrd-end", base + size, address_cells);
    if (err) {
        return err;
    }

    return 0;
}



#define ALIGN_(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((void *)(ALIGN_((unsigned long)(p), (a))))
#define GET_CELL(p) (p += 4, *((const uint32_t *)(p-4)))
int util_is_printable_string(const void *data, int len);

static void print_data(const char *data, int len)
{
    int i;
    const char *p = data;

    /* no data, don't print */
    if (len == 0)
        return;

    if (util_is_printable_string(data, len)) {
        printf(" = \"%s\"", (const char *)data);
        const char *s = data+strlen(data)+1;
        const char *se = data+len;
        while (s < se) {
            printf(", \"%s\"", s);
            s+=strlen(s);
            s++;
        }
    } else if ((len % 4) == 0) {
        printf(" = <");
        for (i = 0; i < len; i += 4)
            printf("0x%08x%s", fdt32_to_cpu(GET_CELL(p)),
                   i < (len - 4) ? " " : "");
        printf(">");
    } else {
        printf(" = [");
        for (i = 0; i < len; i++)
            printf("%02x%s", *p++, i < len - 1 ? " " : "");
        printf("]");
    }
}

void fdt_dump_blob(void *blob)
{
    struct fdt_header *bph = blob;
    uint32_t off_mem_rsvmap = fdt32_to_cpu(bph->off_mem_rsvmap);
    uint32_t off_dt = fdt32_to_cpu(bph->off_dt_struct);
    uint32_t off_str = fdt32_to_cpu(bph->off_dt_strings);
    struct fdt_reserve_entry *p_rsvmap =
        (struct fdt_reserve_entry *)((char *)blob + off_mem_rsvmap);
    const char *p_struct = (const char *)blob + off_dt;
    const char *p_strings = (const char *)blob + off_str;
    uint32_t version = fdt32_to_cpu(bph->version);
    uint32_t totalsize = fdt32_to_cpu(bph->totalsize);
    uint32_t tag;
    const char *p, *s, *t;
    int depth, sz, shift;
    int i;
    uint64_t addr, size;

    depth = 0;
    shift = 4;

    printf("/dts-v1/;\n");
    printf("// magic:\t\t0x%x\n", fdt32_to_cpu(bph->magic));
    printf("// totalsize:\t\t0x%x (%d)\n", totalsize, totalsize);
    printf("// off_dt_struct:\t0x%x\n", off_dt);
    printf("// off_dt_strings:\t0x%x\n", off_str);
    printf("// off_mem_rsvmap:\t0x%x\n", off_mem_rsvmap);
    printf("// version:\t\t%d\n", version);
    printf("// last_comp_version:\t%d\n",
           fdt32_to_cpu(bph->last_comp_version));
    if (version >= 2)
        printf("// boot_cpuid_phys:\t0x%x\n",
               fdt32_to_cpu(bph->boot_cpuid_phys));

    if (version >= 3)
        printf("// size_dt_strings:\t0x%x\n",
               fdt32_to_cpu(bph->size_dt_strings));
    if (version >= 17)
        printf("// size_dt_struct:\t0x%x\n",
               fdt32_to_cpu(bph->size_dt_struct));
    printf("\n");

    for (i = 0; ; i++) {
        addr = fdt64_to_cpu(p_rsvmap[i].address);
        size = fdt64_to_cpu(p_rsvmap[i].size);
        if (addr == 0 && size == 0)
            break;

        printf("/memreserve/ %llx %llx;\n",
               (unsigned long long)addr, (unsigned long long)size);
    }

    p = p_struct;
    while ((tag = fdt32_to_cpu(GET_CELL(p))) != FDT_END) {

        /* printf("tag: 0x%08x (%d)\n", tag, p - p_struct); */

        if (tag == FDT_BEGIN_NODE) {
            s = p;
            p = PALIGN(p + strlen(s) + 1, 4);

            if (*s == '\0')
                s = "/";

            printf("%*s%s {\n", depth * shift, "", s);

            depth++;
            continue;
        }

        if (tag == FDT_END_NODE) {
            depth--;

            printf("%*s};\n", depth * shift, "");
            continue;
        }

        if (tag == FDT_NOP) {
            printf("%*s// [NOP]\n", depth * shift, "");
            continue;
        }

        if (tag != FDT_PROP) {
            fprintf(stderr, "%*s ** Unknown tag 0x%08x\n", depth * shift, "", tag);
            break;
        }
        sz = fdt32_to_cpu(GET_CELL(p));
        s = p_strings + fdt32_to_cpu(GET_CELL(p));
        if (version < 16 && sz >= 8)
            p = PALIGN(p, 8);
        t = p;

        p = PALIGN(p + sz, 4);

        printf("%*s%s", depth * shift, "", s);
        print_data(t, sz);
        printf(";\n");
    }
}
