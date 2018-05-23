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

//
// Explicitly force definitions so we will load the intrinsics
// Note that we handle non-existence in this file (or we SHOULD)
//
#include <x86intrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <pthread.h>
#include <assert.h>
#include "cpuid.h" // need latest definitions

typedef struct {
    int init:1; 
    int has_sse2:1;
    int has_rtm:1;
    int has_hle:1;
    int has_clflushopt:1;
    int has_clwb:1;

    unsigned clsize;
} cpu_info_t;

static cpu_info_t cpu_info;

static void dummy_init(void);
static void real_init(void);
void (*cpu_init)(void) = real_init;

static void clflush(void const *addr);
static void clflushopt(void const *addr);
static void clwb(void const*addr);
static void dummy_clflush(void const *addr);
void (*cpu_clflush)(void const *addr) = dummy_clflush;
void (*cpu_clflushopt)(void const *addr) = dummy_clflush;
void (*cpu_clwb)(void const *addr) = dummy_clflush;


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


static void real_init(void)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    unsigned eax, ebx, ecx, edx;

    pthread_mutex_lock(&lock);

    while (0 == cpu_info.init) {

        if (__get_cpuid_max(0, NULL) >= 1) {
            __cpuid_count(1, 0, eax, ebx, ecx, edx);
            if (bit_SSE2 & edx) {
                cpu_info.has_sse2 = 1;
            }
        }

        cpu_info.has_rtm = cpu_test_level7_flag(bit_RTM);
        cpu_info.has_hle = cpu_test_level7_flag(bit_HLE);

        if (cpu_info.has_sse2) {
            cpu_clflush = clflush;
            cpu_clflushopt = clflush;
            cpu_clwb = clflush;
        }
        
        cpu_info.has_clflushopt = cpu_test_level7_flag(bit_CLFLUSHOPT);
        if (cpu_info.has_clflushopt) {
            cpu_clflushopt = clflushopt;
            cpu_clwb = clflushopt;
        }

        cpu_info.has_clwb = cpu_test_level7_flag(bit_CLWB);
        if (cpu_info.has_clwb) {
            cpu_clwb = clwb;
        }

        if (__get_cpuid_max(0, NULL) >= 1) {
            __cpuid(1, eax, ebx, ecx, edx);
            cpu_info.clsize = (((ebx>>8) & 0xFF) << 3);
        }

        cpu_init = dummy_init;
        cpu_info.init = 1;
    }

    pthread_mutex_unlock(&lock);

}

static void dummy_init(void)
{
    return;
}



int cpu_has_rtm(void)
{
    return cpu_info.has_rtm;
}

int cpu_has_hle(void)
{
    return cpu_info.has_hle;
}

int cpu_has_clflush(void)
{
    return cpu_info.has_sse2;
}

int cpu_has_clwb(void)
{
    return cpu_info.has_clwb;
}

int cpu_has_clflushopt(void) 
{
    return cpu_info.has_clflushopt;
}

size_t cpu_cacheline_size(void)
{
    return cpu_info.clsize;
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


static void dummy_clflush(void const *addr)
{
    assert(cpu_info.init);

    // TODO: implement this
    assert(0);
}

// https://elixir.bootlin.com/linux/latest/source/arch/x86/include/asm/special_insns.h

static void clflush(void const *addr)
{
    assert(cpu_info.has_sse2);

    _mm_clflush(addr);
}

static void clflushopt(void const *addr)
{
    assert(cpu_info.has_clflushopt);

    // _mm_clflushopt(addr);
    asm volatile(".byte 0x66; clflush %P0" : "+m" (*(volatile char *)addr));

}

static void clwb(void const *addr)
{
    assert(cpu_info.has_clwb);

    // this isn't compiling ("inlining failed")
    // _mm_clwb((void *)(uintptr_t)addr);
    // as the time of writing the more obvious 
    // asm volatile("clwb %0" : "+m" (*(volatile char *) addr));
    // wouldn't build.  I suspect the tool chain is too old.
    asm volatile(".byte 0x66, 0x0f, 0xae, 0x30" : "+m" (*(volatile char *)addr));
}
