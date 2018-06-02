/*
 * Copyright (C) 2018 Tony Mason
 */

//
// Some CPU functions
//
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <unistd.h>

//
// Explicitly force definitions so we will load the intrinsics
// Note that we handle non-existence in this file (or we SHOULD)
//
#include <pthread.h>
#include <assert.h>
#include "cpuid.h" // need latest definitions
#include "cpu.h"

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

unsigned char cpu_xtest(void)
{
    if ((0 == cpu_info.has_rtm) && (0 == cpu_info.has_hle)) {
        return 0;
    }

    return _xtest();
}

unsigned int cpu_xbegin(void)
{
    if (0 == cpu_info.has_rtm) {
        return 0;
    }

    return _xbegin();
}

void cpu_xend(void)
{
    if (0 == cpu_info.has_rtm) {
        return;
    }

    _xend();
}

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

void cpu_sfence(void)
{
    _mm_sfence();
}

void cpu_mfence(void) {
    _mm_mfence();
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

#if defined(__INTEL_COMPILER)
    _mm_clflushopt(addr);
#else
    asm volatile(".byte 0x66; clflush %P0" : "+m" (*(volatile char *)addr));
#endif 

}

static void clwb(void const *addr)
{
    assert(cpu_info.has_clwb);

    // this isn't compiling ("inlining failed")
    // _mm_clwb((void *)(uintptr_t)addr);
    // as the time of writing the more obvious 
    // asm volatile("clwb %0" : "+m" (*(volatile char *) addr));
    // wouldn't build.  I suspect the tool chain is too old.
#if defined(__INTEL_COMPILER)
    _mm_clwb(addr);
#else
    asm volatile(".byte 0x66, 0x0f, 0xae, 0x30" : "+m" (*(volatile char *)addr));
#endif 

}

static const char *cache_types[] = {
    "Invalid Cache",
    "Data Cache",
    "Instruction Cache",
    "Unified Cache",
};

//
//  cpu_get_cache_info: this extracts the information for the given
//  cache (index)
//
cpu_cache_data_t *cpu_get_cache_info(unsigned cache_id)
{
    cpu_cache_data_t *cd = malloc(sizeof(cpu_cache_data_t));
    uint32_t eax, ebx, ecx, edx;
    unsigned  type;

    while (NULL != cd) {
        memset(cd, 0, sizeof(cpu_cache_data_t));

        eax = 4;
        ecx = cache_id;
        asm(
            "cpuid" : "+a" (eax), "=b" (ebx), "+c" (ecx), "=d" (edx)
        );

        type = cd->type = eax & 0x1F;

        if (0 == cd->type) {
            // invalid cache entry
            free(cd);
            cd = NULL;
            break;
        }

        if (type > sizeof(cache_types) / sizeof(char *)) {
            type = 0;
        }
        memcpy(cd->type_name, cache_types[type], strlen(cache_types[type]));

        cd->id = cache_id;
        cd->level = (eax >>=5) & 0x7;
        cd->sets = ecx + 1;
        cd->line_size = (ebx & 0xFFF) + 1;
        cd->partitions = ((ebx >>= 12) & 0x3FF) + 1;
        cd->associativity = ((ebx >>10) & 0x3FF) + 1;
        cd->cache_size = cd->associativity * cd->partitions * cd->line_size * cd->sets;
        cd->fully_associative = (eax >>= 1) & 0x1;
        cd->self_initializing = (eax >>= 3) & 0x1;
       
        // done.
        break;
    }

    return cd;
}

void cpu_free_cache_info(cpu_cache_data_t *cd) 
{ 
    free (cd); 
}

#if 0
//
// see https://stackoverflow.com/questions/21369381/measuring-cache-latencies
//
int i386_cpuid_caches (size_t * data_caches) {
    int i;
    int num_data_caches = 0;
    for (i = 0; i < 32; i++) {

        // Variables to hold the contents of the 4 i386 legacy registers
        uint32_t eax, ebx, ecx, edx; 

        eax = 4; // get cache info
        ecx = i; // cache id

        asm (
            "cpuid" // call i386 cpuid instruction
            : "+a" (eax) // contains the cpuid command code, 4 for cache query
            , "=b" (ebx)
            , "+c" (ecx) // contains the cache id
            , "=d" (edx)
        ); // generates output in 4 registers eax, ebx, ecx and edx 

        // taken from http://download.intel.com/products/processor/manual/325462.pdf Vol. 2A 3-149
        int cache_type = eax & 0x1F; 

        if (cache_type == 0) // end of valid cache identifiers
            break;

        char * cache_type_string;
        switch (cache_type) {
            case 1: cache_type_string = "Data Cache"; break;
            case 2: cache_type_string = "Instruction Cache"; break;
            case 3: cache_type_string = "Unified Cache"; break;
            default: cache_type_string = "Unknown Type Cache"; break;
        }

        int cache_level = (eax >>= 5) & 0x7;

        int cache_is_self_initializing = (eax >>= 3) & 0x1; // does not need SW initialization
        int cache_is_fully_associative = (eax >>= 1) & 0x1;


        // taken from http://download.intel.com/products/processor/manual/325462.pdf 3-166 Vol. 2A
        // ebx contains 3 integers of 10, 10 and 12 bits respectively
        unsigned int cache_sets = ecx + 1;
        unsigned int cache_coherency_line_size = (ebx & 0xFFF) + 1;
        unsigned int cache_physical_line_partitions = ((ebx >>= 12) & 0x3FF) + 1;
        unsigned int cache_ways_of_associativity = ((ebx >>= 10) & 0x3FF) + 1;

        // Total cache size is the product
        size_t cache_total_size = cache_ways_of_associativity * cache_physical_line_partitions * cache_coherency_line_size * cache_sets;

        if (cache_type == 1 || cache_type == 3) {
            data_caches[num_data_caches++] = cache_total_size;
        }

        printf(
            "Cache ID %d:\n"
            "- Level: %d\n"
            "- Type: %s\n"
            "- Sets: %d\n"
            "- System Coherency Line Size: %d bytes\n"
            "- Physical Line partitions: %d\n"
            "- Ways of associativity: %d\n"
            "- Total Size: %zu bytes (%zu kb)\n"
            "- Is fully associative: %s\n"
            "- Is Self Initializing: %s\n"
            "\n"
            , i
            , cache_level
            , cache_type_string
            , cache_sets
            , cache_coherency_line_size
            , cache_physical_line_partitions
            , cache_ways_of_associativity
            , cache_total_size, cache_total_size >> 10
            , cache_is_fully_associative ? "true" : "false"
            , cache_is_self_initializing ? "true" : "false"
        );
    }

    return num_data_caches;
}
#endif // 0

