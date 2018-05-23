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
#include <unistd.h>
#include <x86intrin.h>
#include <xmmintrin.h>
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

int cpu_has_sse2(void)
{
    unsigned eax, ebx, ecx, edx;
    int retval = 0;

    if (__get_cpuid_max(0, NULL) >= 1) {
        __cpuid_count(1, 0, eax, ebx, ecx, edx);
        if (bit_SSE2 & edx) {
            retval = 1;
        }
    }
    return retval;
 
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


unsigned cpu_frequency(void)
{
    unsigned eax, ebx, ecx, edx;
    int retval = 0;

    // TODO: need to deal with the case where this isn't supported (pre-skylake)
    if (__get_cpuid_max(0, NULL) >= 0x16) {
        __cpuid(0x16, eax, ebx, ecx, edx);
        retval = eax;
    }
    else {
        retval = sysconf(_SC_CLK_TCK);
    }
    return retval;
}

unsigned cpu_rdtsc(void)
{
    return __rdtsc();
}

void cpu_prefetch_l1(const void *addr)
{
    _mm_prefetch(addr, _MM_HINT_T0);
}

void cpu_prefetch_l2(const void *addr)
{
    _mm_prefetch(addr, _MM_HINT_T1);
}

void cpu_prefetch_l3(const void *addr)
{
    _mm_prefetch(addr, _MM_HINT_T2);
}

void cpu_prefetch_oneuse(const void *addr)
{
    _mm_prefetch(addr, _MM_HINT_NTA);
}