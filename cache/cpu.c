/*
 * Copyright (C) 2018 Tony Mason
 */

//
// Some CPU functions
//
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <malloc.h>
#include "cpuid.h" // need latest definitions

static int cpu_test_level7_flag(unsigned flag)
{
    unsigned eax, ebx, ecx, edx;
    int retval = 0;

    if (__get_cpuid_max(0, NULL) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        if (flag & ebx) {
            retval = 1;
        }
    }
    return retval;
}

int cpu_has_rtm(void)
{
    return cpu_test_level7_flag(bit_RTM);
}

int cpu_has_hle(void)
{
    return cpu_test_level7_flag(bit_HLE);
}

int cpu_has_clwb(void)
{
    return cpu_test_level7_flag(bit_CLWB);
}

int cpu_has_clflushopt(void) 
{
    return cpu_test_level7_flag(bit_CLFLUSHOPT);
}


size_t cpu_cacheline_size(void)
{
    unsigned eax, ebx, ecx, edx;
    int retval = 0;

    if (__get_cpuid_max(0, NULL) >= 1) {
        __cpuid(1, eax, ebx, ecx, edx);
        retval = (((ebx>>8) & 0xFF) << 3);
    }
    return retval;

}