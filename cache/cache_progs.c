// #define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>
#include <x86intrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <float.h>
#include <pthread.h>
#include <stdarg.h>
#include "cache.h"
#include "cpu.h"
#include "setprocessor.h"

// This is here to quiet the IDE complaining this isn't defined.
#if !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS (0x20)
#endif

static FILE *logfile;

/// This is experimental code that I'll move elsewhere at some point
typedef union {
    struct {
        void *next;
        unsigned counter;
    } s;
    unsigned char data[64];
} record_t;

#if !defined(C_ASSERT)
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif // C_ASSERT

#if !defined(PAGE_SIZE)
#define PAGE_SIZE (4096)
#endif // PAGE_SIZE

C_ASSERT(64 == sizeof(record_t));

#define RECORDS_PER_PAGE (PAGE_SIZE / sizeof(record_t))
typedef struct {
    record_t records[RECORDS_PER_PAGE];
} record_page_t;


C_ASSERT(PAGE_SIZE == sizeof(record_page_t));

#define LOG_RESULTS(pages, runs, time, description) \
printf("\t\t\t\"%s\": { \"pagecount\": %d, \"runs\": %u, \"time\": %f},\n", description, pages, runs, time);
// printf("\"%s\" : {\"runs\" : %d, \"pages\":  %d, \"time\": %f, \"description\": \"%s\"},\n", __PRETTY_FUNCTION__, runs, pages, time, description);;
// printf("%3.3d, %4d, %14.2f, %24.24s, %s\n", pages, runs, time, __PRETTY_FUNCTION__, description)

#define LOG_TIME(time, description) \
printf("\t\t\t\"%s\": {\"time\": %f},\n", description, time);



static void flush_cache(void)
{
    const size_t buffer_length = 32 * 1024 * 1024;
    static char buffer[buffer_length];
    static char c;

    memset(buffer, c++, buffer_length);
}

typedef void (*init_memory)(const unsigned pagecount, void *memory);

static void init_cache_test_memory(const unsigned pagecount, void *memory)
{
    record_page_t *recpages = (record_page_t *)memory;

    // printf("memory is at 0x%p", memory);

    memset(memory, 0, pagecount * PAGE_SIZE);
    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        record_t *r = &recpages[0].records[index];
        // printf("initialize page 0x%p, index %u\n", r, index);
        for (unsigned index2 = 0; index2 < pagecount; index2++) {
            uintptr_t diff1, diff2;

            r->s.next = &recpages[(index2 + 1) % pagecount].records[index];
            r->s.counter = 0;
            diff1 = (uintptr_t)r - (uintptr_t)r->s.next;
            diff2 = (uintptr_t)r->s.next - (uintptr_t)r;
            // printf("set %p to point to %p (difference %lu)\n", r, r->s.next, diff1 < diff2 ? diff1 : diff2);
            r = r->s.next; // advance to the next location. 
        }
    }
    
    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        record_t *r = &recpages[0].records[index];

        do {
            r = r->s.next;
            assert(NULL != r);
        } while (r != &recpages[0].records[index]);
    }
}

//
// This routine initializes the records so they are threaded into the same cache set
//
static void init_cache_test_memory_same_set(const unsigned pagecount, void *memory)
{
    record_page_t *recpages = (record_page_t *)memory;

    // printf("memory is at 0x%p", memory);

    memset(memory, 0, pagecount * PAGE_SIZE);
    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        record_t *r = &recpages[0].records[index];
        // printf("initialize page 0x%p, index %u\n", r, index);
        for (unsigned index2 = 0; index2 < pagecount; index2++) {
            uintptr_t diff1, diff2;

            r->s.next = &recpages[(index2 + 1) % pagecount].records[index];
            r->s.counter = 0;
            diff1 = (uintptr_t)r - (uintptr_t)r->s.next;
            diff2 = (uintptr_t)r->s.next - (uintptr_t)r;
            // printf("set %p to point to %p (difference %lu)\n", r, r->s.next, diff1 < diff2 ? diff1 : diff2);
            r = r->s.next; // advance to the next location. 
        }
    }
    
    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        record_t *r = &recpages[0].records[index];

        do {
            r = r->s.next;
            assert(NULL != r);
        } while (r != &recpages[0].records[index]);
    }
}

//
// This routine initializes the records so they are threaded into different cache sets
// (note this is done in a diagonal fashion).
//
static void init_cache_test_memory_different_set(const unsigned pagecount, void *memory)
{
    record_page_t *recpages = (record_page_t *)memory;

    // printf("memory is at 0x%p", memory);

    memset(memory, 0, pagecount * PAGE_SIZE);
    for (unsigned index1 = 0; index1 < RECORDS_PER_PAGE; index1++) {
        record_t *r = &recpages[0].records[index1];
        for (unsigned index2 = 0; index2 < pagecount; index2++) {
            unsigned offset = (index2 + 1) % pagecount;
            r->s.next = &recpages[offset].records[(index1 + offset) % RECORDS_PER_PAGE];
            // printf( "%s: %p -> %p\n", __PRETTY_FUNCTION__, r, r->s.next);
            r = r->s.next;
        }
    }

    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        record_t *r = &recpages[0].records[index];

        do {
            r = r->s.next;
            assert(NULL != r);
        } while (r != &recpages[0].records[index]);
    }
}



//
// This is a skeleton for cache test code
//
static unsigned long test_cache_skel(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        start = _rdtsc();
        /* this is where you put the specfic cache logic */
        /* 
         * Options would include:
         *   - flushing: none, clflush, clflushopt, clflushwb
         *   - fencing: none, sfence, lfence, mfence
         *   - fence frequency
         */
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_noflush_nofence(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}


static unsigned long test_cache_noflush_sfence_end(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }
    start = _rdtsc();
    _mm_sfence();
    end = _rdtsc();
    time += (end - start);

    // report the amount of CPU time
    return time;

}


static unsigned long test_cache_noflush_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {

        // change memory value, follow pointer
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        end = _rdtsc();
        time += end - start;

        // check to see if we are at the end of the loop
    
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }

        // update counter
        count++;

        // is it time to flush?

        if (0 == (count % n)) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
        }
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_clflush_nofence(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        _mm_clflush(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_clflush_sfence_end(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clflush()) {
        return time;
    }


    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        _mm_clflush(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }
    start = _rdtsc();
    _mm_sfence();
    end = _rdtsc();
    time += (end - start);

    // report the amount of CPU time
    return time;

}


static unsigned long test_cache_clflush_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clflush()) {
        return time;
    }

    while (r != &rp->records[0]) {

        // change memory value, follow pointer
        start = _rdtsc();
        r->s.counter++;
        _mm_clflush(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;

        // check to see if we are at the end of the loop
    
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }

        // update counter
        count++;

        // is it time to flush?

        if (0 == (count % n)) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
        }
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_clflushopt_nofence(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clflushopt()) {
        return time;
    }

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        _mm_clflush(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_clflushopt_sfence_end(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clflushopt()) {
        return time;
    }

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        _mm_clflushopt(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }
    start = _rdtsc();
    _mm_sfence();
    end = _rdtsc();
    time += (end - start);

    // report the amount of CPU time
    return time;

}


static unsigned long test_cache_clflushopt_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clflushopt()) {
        return time;
    }

    while (r != &rp->records[0]) {

        // change memory value, follow pointer
        start = _rdtsc();
        r->s.counter++;
        _mm_clflushopt(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;

        // check to see if we are at the end of the loop
    
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }

        // update counter
        count++;

        // is it time to fence?

        if (0 == (count % n)) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
        }
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_clwb_nofence(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clwb()) {
        return time;
    }


    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        _mm_clwb(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_clwb_sfence_end(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clwb()) {
        return time;
    }

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        _mm_clwb(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
    }
    start = _rdtsc();
    _mm_sfence();
    end = _rdtsc();
    time += (end - start);

    // report the amount of CPU time
    return time;

}


static unsigned long test_cache_clwb_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    if (!cpu_has_clwb()) {
        return time;
    }

    while (r != &rp->records[0]) {

        // change memory value, follow pointer
        start = _rdtsc();
        r->s.counter++;
        _mm_clwb(r);
        r = r->s.next;
        end = _rdtsc();
        time += end - start;

        // check to see if we are at the end of the loop
    
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }

        // update counter
        count++;

        // is it time to flush?

        if (0 == (count % n)) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
        }
    }

    // report the amount of CPU time
    return time;

}

static struct {
    const char *name;
    int (*check)(void); // can we run this test
    unsigned long (*test)(record_page_t *rp);
} tests[] = {
    {"noflush, nofence", cpu_has_clflush, test_cache_noflush_nofence},
    {"noflush, sfence end", cpu_has_clflush, test_cache_noflush_sfence_end},
    {"clflush, nofence", cpu_has_clflush, test_cache_clflush_nofence},
    {"clflush, sfence end", cpu_has_clflush, test_cache_clflush_sfence_end},
    {"clflushopt, nofence", cpu_has_clflushopt, test_cache_clflushopt_nofence},
    {"clflushopt, sfence end", cpu_has_clflushopt, test_cache_clflushopt_sfence_end},
    {"clwb, nofence", cpu_has_clwb, test_cache_clwb_nofence},
    {"clwb, sfence end", cpu_has_clwb, test_cache_clwb_sfence_end},
    {NULL, NULL},
};

static struct {
    const char *name;
    int (*check)(void); // can we even run this test
    unsigned long (*test)(record_page_t *rp, unsigned frequency); // test to run
} freq_tests[] = {
    {"noflush, sfence periodic", cpu_has_clflush, test_cache_noflush_sfence_every_n_updates},
    {"clflush, sfence periodic", cpu_has_clflush, test_cache_clflush_sfence_every_n_updates},
    {"clflushopt, sfence periodic", cpu_has_clflushopt, test_cache_clflushopt_sfence_every_n_updates},
    {"clwb, sfence periodic", cpu_has_clflushopt, test_cache_clwb_sfence_every_n_updates},
    {NULL, NULL},
};

static const unsigned test_frequencies[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 16, 32, 64, 128,
};

static void test_cache_behavior_9(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned t = 0;
    unsigned long long time_same, time_different;
    unsigned frequency = 0;

    printf("\t\t\"%s\":{\n", __PRETTY_FUNCTION__);
    printf("\t\t\t\"frequency tests\": {\n");
    while (freq_tests[t].name && freq_tests[t].test) {
        if (0 != freq_tests[t].check) {
            if (0 == freq_tests[t].check()) {
                // cannot run it on this system
                t++;
                continue;
            }
        }
        if (t > 0) {
            printf(",\n");
        }
        printf("\t\t\t\t\"%s\": {\n", freq_tests[t].name);
        for (unsigned findex = 0; findex < (sizeof(test_frequencies) / sizeof(unsigned)); findex++) {
            time_same = time_different = 0;
            frequency = test_frequencies[findex];
            for (unsigned run = 0; run < runs; run++) {
                init_cache_test_memory_same_set(pagecount, memory);
                time_same += freq_tests[t].test((record_page_t *)memory, frequency);
                init_cache_test_memory_different_set(pagecount, memory);
                time_different += freq_tests[t].test((record_page_t *)memory, frequency);
            }
            if ((time_same > 0) || (time_different > 0)) {
                if (findex > 0) {
                    printf(",\n");
                }
                // printf("\t\t\t\t\t\"frequency %u\": {\"same cache set\": %lu, \"different cache set\": %lu}", frequency, time_same, time_different);
                printf("\t\t\t\t\t\"%u\": {\"same cache set\": %lu, \"different cache set\": %lu}", frequency, time_same, time_different);
            }
        }
        printf("\n\t\t\t\t}");
        t++;
    }
    printf("\n\t\t\t},\n");
    printf("\t\t\t\"basic tests\": {\n");
    t=0;
    while (tests[t].name && tests[t].test) {
        if (0 != tests[t].check) {
            if (0 == tests[t].check()) {
                // cannot run it on this system
                t++;
                continue;
            }
        } 
        time_same = time_different = 0;
        for (unsigned run = 0; run < runs; run++) {
            init_cache_test_memory_same_set(pagecount, memory);
            time_same += tests[t].test((record_page_t *)memory);
            init_cache_test_memory_different_set(pagecount, memory);
            time_different += tests[t].test((record_page_t *)memory);
        }
        if (t>0) {
            printf(",\n");
        }
        if ((time_same > 0) || (time_different > 0)) {
            printf("\t\t\t\t\t\"%s\": {\"same cache set\": %lu, \"different cache set\": %lu}", tests[t].name, time_same, time_different);
        }
        t++;
    }
    printf("\n\t\t\t\t\t},\n");

    printf("\t\t\t\"pagecount\": %u,\n", pagecount);
    printf("\t\t\t\"runs\": %u\n", runs);
    printf("\t\t}");

}

static void test_cache_behavior_8(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned t = 0;
    unsigned long long time_same, time_different;

    printf("\t\t\"%s\":{\n", __PRETTY_FUNCTION__);

    while (tests[t].name && tests[t].test) {
        time_same = time_different = 0;
        for (unsigned run = 0; run < runs; run++) {
            init_cache_test_memory_same_set(pagecount, memory);
            time_same += tests[t].test((record_page_t *)memory);
            init_cache_test_memory_different_set(pagecount, memory);
            time_different += tests[t].test((record_page_t *)memory);
        }
        if ((time_same > 0) || (time_different > 0)) {
            if (t>0) {
                printf("\n");
            }
            printf("\t\t\t\"%s\": {\"same cache set\": %lu, \"different cache set\": %lu},", tests[t].name, time_same, time_different);
        }
        t++;
    }
    printf("\n\t\t\t\"pagecount\": %u,", pagecount);
    printf("\n\t\t\t\"runs\": %u", runs);
    printf("\n\t\t\t}");

}

#if 0
static void test_cache_behavior_9(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned t = 0;
    unsigned long time_same, time_different;

    // printf( "%s(%u, %u, 0x%p)\n", __PRETTY_FUNCTION__, pagecount, runs, memory);

    while (tests[t].name && tests[t].test) {
        time_same = time_different = 0;
        for (unsigned run = 0; run < runs; run++) {
            init_cache_test_memory_same_set(pagecount, memory);
            time_same += tests[t].test((record_page_t *)memory);
            init_cache_test_memory_different_set(pagecount, memory);
            time_different += tests[t].test((record_page_t *)memory);
        }
        if ((time_same > 0) || (time_different > 0)) {
            // this format is intended to be a simple json/python style output emission.
            printf( "{\"%s\": ((\"test\", \"%s\"), (\"page count\", %u), (\"runs\", %u), (\"time same cache set\", %lu), (\"time different cache set\", %lu)) }, \n", 
                    __PRETTY_FUNCTION__, tests[t].name, pagecount, runs, time_same, time_different);
        }
        t++;
    }

}
#endif // 0

static void test_cache_behavior_7(const unsigned pagecount, const unsigned runs, void *memory)
{
    //if (!cpu_has_clwb()) {
    //    return;
    //}

    init_cache_test_memory_different_set(pagecount, memory);
    record_page_t *rp = (record_page_t *)memory;
    record_t *r = &rp->records[0];

#if 0
    // This is debug logic - these should point to the next entry on the next page
    // (with wrapping)

    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        do {
            printf( "%s: record %p points to %p\n", __PRETTY_FUNCTION__, r, r->s.next);
            r = r->s.next;
        } while (r != &rp->records[0]);

        double time = test_cache_noflush_nofence(rp);
        printf( "%s: time to walk list: %f\n", __PRETTY_FUNCTION__, time);
    }
#endif // 0
    printf("\t\t\"%s\":{ \"results\": \"\"}", __PRETTY_FUNCTION__);

}

//
// Reference to: http://blog.stuffedcow.net/2013/01/ivb-cache-replacement/
// this is about cache replacement policies and includes some code on determining specific policy.
//
// It might be possible to use a hybrid approach here: a probabalistic 

static void test_cache_behavior_6(const unsigned pagecount, const unsigned runs, void *memory)
{
    record_page_t *recpages = (record_page_t *)memory;
    record_t *r0_start = &recpages[0].records[0];
    record_t *r0 = r0_start;
    double time = 0.0;
    unsigned start, end;
    unsigned run;
 
    printf("\t\t\"%s\":{", __PRETTY_FUNCTION__);
    if (12 != pagecount) {
        // only need to test this one case.
        printf(" \"results\": \"\"}");
        return;
    }

    init_cache_test_memory(pagecount, memory);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        flush_cache();
        // prime the cache
        start = _rdtsc();
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0->s.next;
        r0 = r0_start;
        end = _rdtsc();
        time += (double)(end - start);
    }

    printf("\t\t\t\"%s in ticks\": %f,\n", "run list (cold cache)", time);
    // printf( "%s: priming step took %f ticks\n", __PRETTY_FUNCTION__, time);

    // now re-run it
    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 9
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 10
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 11
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();
        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "run list (warm cache)", time);


    time = 0.0;
    for (run = 0; run < runs; run++) {
        start = _rdtsc();
        _mm_clflush(r0);
        end = _rdtsc();
        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clean cache line flush", time);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) four writes on same cache line", time);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double) (end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush+sfence ticks for 4 writes on same cache line", time);
    // printf( "%s: clflush took %f ticks for 4 writes on same cache line with clflush and fence\n", __PRETTY_FUNCTION__, time);


    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 6 writes on same cache line", time);

    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 7 writes on same cache line", time);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 8 writes on same cache line", time);


    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 9
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 9 writes on same cache line", time);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 9
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 10
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 10 writes on same cache line", time);

    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 9
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 10
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 11
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();
        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 11 writes on same cache line", time);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 9
        _mm_clflush(r0);
        r0 = r0->s.next; 
        r0->s.counter++; // 10
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 11
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 12
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 13
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 14
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 15
        _mm_clflush(r0);
        r0 = r0->s.next;
        r0->s.counter++; // 16
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,\n", "clflush (final sfence) 16 writes on same cache line", time);

    time = 0.0;
    for (run = 0; run < runs; run++) {
        r0 = r0_start;
        start = _rdtsc();
        r0->s.counter++; // 1
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next; 
        r0->s.counter++; // 2
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 3
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 4
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 5
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 6
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 7
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 8
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 9
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next; 
        r0->s.counter++; // 10
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 11
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 12
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 13
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 14
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 15
        _mm_clflush(r0);
        _mm_sfence();
        r0 = r0->s.next;
        r0->s.counter++; // 16
        _mm_clflush(r0);
        _mm_sfence();
        end = _rdtsc();

        time += (double)(end - start);
    }
    printf("\t\t\t\"%s in ticks\": %f,", "clflush+sfence 16 writes on same cache line", time);

    printf("\n\t\t\t\"pagecount\": %u,", pagecount);
    printf("\n\t\t\t\"runs\": %u", runs);
    printf("\n\t\t\t}");


}

static void test_cache_behavior_5(const unsigned pagecount, const unsigned runs, void *memory)
{
    static unsigned done = 0;
    record_t *r, *r2;
    int xresult = 36;
    int hits = 0;
    record_page_t *recpages = (record_page_t *)memory;
    struct perf_timers { // do this to avoid the cache line we're trying to test
        unsigned long long start;
        unsigned long long end;
        unsigned long long time;
    } *timers = (struct perf_timers *)&recpages[0].records[1];
    struct perf_timers *ttimer = &timers[0], *optimer = &timers[1];
    unsigned status;
    unsigned committed = 0;
    unsigned aborted = 0;

    printf("\t\t\"%s\":", __PRETTY_FUNCTION__);

    if (pagecount != 12) {
        // this is our magic number - ignore all others.
        printf("{ \"results\": \"\"}");
        return;
    }

    if (0 == cpu_has_rtm()) { // no RTM, nothing to do
        printf("{\"results\": \"\"}");
        return;
    }

    //
    // Build a list of starting addresses.  Staggering is my attempt to foil the prefetcher
    //
    init_cache_test_memory(pagecount, memory);
    printf("\n\t\t\t{ \"results\": {\n");
    for (unsigned index = 2; index < pagecount; index++) {
        ttimer->time = 0;
        optimer->time = 0;
        committed = 0;
        aborted = 0;

        for (unsigned run = 0; run < runs; run++) {
            record_t *r = &recpages[0].records[0];

            ttimer->start = _rdtsc();
            status = _xbegin();
            if (_XBEGIN_STARTED == status) {
                optimer->start = _rdtsc();
                for (unsigned index2 = 0; index2 < index; index2++) {
                    r->s.counter++;
                    r = r->s.next;
                }
                _xend();
                optimer->end = _rdtsc();
                optimer->time += optimer->end - optimer->start;
                committed++;
            }
            else {
                aborted++;
            }
            ttimer->end = _rdtsc();
            ttimer->time += ttimer->end - ttimer->start;
        }
        if (index > 2) {
            printf(",\n");
        }
        printf("\t\t\t\t\"transaction length %u\": {\"committed count\": %u, \"aborted count\": %u, \"total time\": %lu, \"op time\": %lu}",
            index, committed, aborted, ttimer->time, optimer->time);
    }

    // close json
    printf("\n\t\t\t},");
    printf("\n\t\t\t\"pagecount\": %u,", pagecount);
    printf("\n\t\t\t\"runs\": %u", runs);
    printf("\n\t\t\t}\n\t\t}");

}

static void test_cache_behavior_4(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    double time = 0.0;

    start = cpu_rdtsc();
    end = cpu_rdtsc();
    time = ((double)(end - start));
    printf("\t\t\"%s\":{ \"results\": \"\"}", __PRETTY_FUNCTION__);

}

static void test_cache_behavior_3(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    static double time = 0.0;
    static int done = 0;

    if (done) {
        printf("\t\t\"%s\":{ \"ticks per nop (over 1 billion)\": %f}", __PRETTY_FUNCTION__, time);
        return;
    }

    start = cpu_rdtsc();
    for (unsigned index = 1000 * 1000 * 1000; index > 0; index--) {
        asm("nop\n\t");
    }
    end = cpu_rdtsc();
    time = ((double)(end - start));
    time /= (double)(1000 * 1000 * 1000);
    printf("\t\t\"%s\":{ \"ticks per nop (over 1 billion)\": %f}", __PRETTY_FUNCTION__, time);

    done = 1;
}

static void test_cache_behavior_2(const unsigned pagecount, const unsigned runs, void *memory)
{
    uintptr_t eom = ((uintptr_t)memory + PAGE_SIZE * pagecount);
    record_page_t *rp = (record_page_t *)memory;
    unsigned start, end;
    double time;
    record_t *r;
    record_t *record_array[RECORDS_PER_PAGE];
    double min_time, max_time;
    unsigned min_index, max_index;

    printf("\t\t\"%s\":{\n", __PRETTY_FUNCTION__);

    //
    // Build a list of starting addresses.  Staggering is my attempt to foil the prefetcher
    //
    memset(record_array, 0, sizeof(record_array));
    for (unsigned index = 0, index2 = 0; index < RECORDS_PER_PAGE; index++) {
        // printf("%u\n", index2);
        assert(NULL == record_array[index]);
        record_array[index] = &rp->records[index2];
        index2 = (index2 + 17) % 64; // forms a ring
    }
    flush_cache();

    // link all the blocks together in a linear fashion
    time = 0.0;
    for (unsigned run = 0; run < runs; run ++) {
        flush_cache();
        for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
            start = cpu_rdtsc();
            r = record_array[index];
            for (unsigned index2 = 0; index2 < pagecount; index2++) {
                record_t *n;
                n = r->s.next = (void *)(((uintptr_t)memory) + (((index2 + 1) % pagecount) <<12));
                r->s.counter = 0;
                r = n;
            }
            end = cpu_rdtsc();
            time += ((double)(end - start));
        }
    }
    time /= runs;
    LOG_RESULTS(pagecount, runs, time, "initialize all blocks");
    
    // run one list
    time = 0.0;
    for (unsigned run = 0; run < runs; run++) {
        record_t *r1 = record_array[0];

        flush_cache();
        start = cpu_rdtsc();
        do {
            r1->s.counter++;
            r1 = r1->s.next;
        } while (r1 != record_array[0]);
        end = cpu_rdtsc();
        time += ((double)(end - start));
    }
    cpu_sfence();
    time /= runs;
    LOG_RESULTS(pagecount, runs, time, "run one cache line with fence at end");

    // run two lists
    time = 0.0;
    for (unsigned run = 0; run < runs; run++) {
        record_t *r1 = record_array[0];
        record_t *r2 = record_array[1];

        flush_cache();
        start = cpu_rdtsc();
        do {
            r1->s.counter++;
            r2->s.counter++;
            r1 = r1->s.next;
            r2 = r2->s.next;
        } while (r1 != record_array[0]);
        end = cpu_rdtsc();
        time += ((double)(end - start));
    }
    cpu_sfence();
    time /= runs;
    LOG_RESULTS(pagecount, runs, time, "run two cache lines with fence at end");

    // run four lists
    time = 0.0;
    for (unsigned run = 0; run < runs; run++) {
        record_t *r1 = record_array[0];
        record_t *r2 = record_array[1];
        record_t *r3 = record_array[2];
        record_t *r4 = record_array[3];

        flush_cache();
        start = cpu_rdtsc();
        do {
            r1->s.counter++;
            r2->s.counter++;
            r3->s.counter++;
            r4->s.counter++;
            r1 = r1->s.next;
            r2 = r2->s.next;
            r3 = r3->s.next;
            r4 = r4->s.next;
        } while (r1 != record_array[0]);
        end = cpu_rdtsc();
        time += ((double)(end - start));
    }
    cpu_sfence();
    time /= runs;
    LOG_RESULTS(pagecount, runs, time, "run four cache lines with fence at end");

    // run eight lists
    time = 0.0;
    for (unsigned run = 0; run < runs; run++) {
        record_t *r1 = record_array[0];
        record_t *r2 = record_array[1];
        record_t *r3 = record_array[2];
        record_t *r4 = record_array[3];
        record_t *r5 = record_array[4];
        record_t *r6 = record_array[5];
        record_t *r7 = record_array[6];
        record_t *r8 = record_array[7];

        flush_cache();
        start = cpu_rdtsc();
        do {
            r1->s.counter++;
            r2->s.counter++;
            r3->s.counter++;
            r4->s.counter++;
            r5->s.counter++;
            r6->s.counter++;
            r7->s.counter++;
            r8->s.counter++;
            r1 = r1->s.next;
            r2 = r2->s.next;
            r3 = r3->s.next;
            r4 = r4->s.next;
            r5 = r5->s.next;
            r6 = r6->s.next;
            r7 = r7->s.next;
            r8 = r8->s.next;
        } while (r1 != record_array[0]);
        end = cpu_rdtsc();
        time += ((double)(end - start));
    }
    cpu_sfence();
    time /= runs;
    LOG_RESULTS(pagecount, runs, time, "run eight cache lines with fence at end");

    // look at results for two lists - attempt to find cache line overlap
    min_time = DBL_MAX;
    max_time = 0;
    min_index = max_index = 0;

    for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
        char buffer[128];
        unsigned modcount = 0;

        time = 0.0;
        for (unsigned run = 0; run < runs; run++) {
            record_t *r1 = record_array[0];
            record_t *r2 = record_array[index];

            flush_cache();

            start = cpu_rdtsc();
            do {
                r1->s.counter++;
                r2->s.counter++;
                r1 = r1->s.next;
                r2 = r2->s.next;
            } while (r1 != record_array[0]);
            cpu_sfence();
            end = cpu_rdtsc();
            time += ((double)(end - start));
        }
        time /= runs;
        if (time < min_time) {
            min_time = time;
            min_index = index;
        }

        if (time > max_time) {
            max_time = time;
            max_index = index;
        }

        snprintf(buffer, sizeof(buffer), "run stride %u", index);
        LOG_RESULTS(pagecount, runs, time, buffer);
    }

    printf("\t\t\t\"summary\": { \"min\": { \"time\": %f, \"stride\": %u }, \"max\": { \"time\": %f, \"stride\": %u } }\n\t\t}", 
            min_time, min_index, max_time, max_index);

}

static void test_cache_behavior_10(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    double time;
    record_t *r = NULL;
    assert(NULL != memory);

    flush_cache();

    printf("\t\t\"%s\":{\n", __PRETTY_FUNCTION__);

    init_cache_test_memory_different_set(pagecount, memory);

    flush_cache();

    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            r = r->s.next;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list without update");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list");

    flush_cache();
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            r->s.counter++;
            r = r->s.next;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f}\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list");

#if 0
    flush_cache();

    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            cpu_prefetch_l1(r->s.next);
            r->s.counter++;
            r = r->s.next;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + prefetch next entry");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + prefetch next entry");
    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            cpu_prefetch_l1(n);
            r->s.counter++;
            cpu_clflush(r);
            r = n;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + prefetch next entry + clflush after each write");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + prefetch next entry + clflush after each write");
    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            r->s.counter++;
            cpu_clflush(r);
            r = n;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + clflush after each write");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    //LOG_RESULTS(pagecount, runs, time, "walk list + clflush after each write");

    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            r->s.counter++;
            cpu_sfence();
            r = n;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + sfence after each write");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + sfence after each write");
    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            r->s.counter++;
            r = n;
        } while(r != memory);
        cpu_sfence();
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + sfence after each run");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f}\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + sfence after each run");
#endif // 0

    // close json
    printf("\t\t}");
}

static void test_cache_behavior_1(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    double time;
    record_t *r = NULL;
    assert(NULL != memory);

    flush_cache();

    printf("\t\t\"%s\":{\n", __PRETTY_FUNCTION__);

    time = 0.0;
    start = cpu_rdtsc();
    r = (record_t *)memory;
    for (unsigned index = 0; index < pagecount; index++) {
        record_t *n;
        n = r->s.next = (void *)(((uintptr_t)memory) + (((index + 1) % pagecount) <<12));
        r->s.counter = 0;
        r = n;
    }
    // cpu_sfence();
    end = cpu_rdtsc();
    time = ((double)(end - start));
    printf("\t\t\t\"%s\": {", "initialize first record of each page");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": 1,");
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, 1, time, "initialize first record of each page");

    flush_cache();

    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            r = r->s.next;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list without update");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list");

    flush_cache();
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            r->s.counter++;
            r = r->s.next;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list");

    flush_cache();

    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            cpu_prefetch_l1(r->s.next);
            r->s.counter++;
            r = r->s.next;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + prefetch next entry");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + prefetch next entry");
    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            cpu_prefetch_l1(n);
            r->s.counter++;
            cpu_clflush(r);
            r = n;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + prefetch next entry + clflush after each write");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + prefetch next entry + clflush after each write");
    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            r->s.counter++;
            cpu_clflush(r);
            r = n;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + clflush after each write");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    //LOG_RESULTS(pagecount, runs, time, "walk list + clflush after each write");

    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            r->s.counter++;
            cpu_sfence();
            r = n;
        } while(r != memory);
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + sfence after each write");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f},\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + sfence after each write");
    flush_cache();

    //
    time = 0.0;
    start = cpu_rdtsc();
    for (unsigned index = 0; index < runs; index++) {
        r = (record_t *)memory;
        do {
            record_t *n = r->s.next;
            r->s.counter++;
            r = n;
        } while(r != memory);
        cpu_sfence();
    }
    end = cpu_rdtsc();
    time = ((double)(end - start)) / (double)runs;
    printf("\t\t\t\"%s\": {", "walk list + sfence after each run");
    printf("\"pagecount\": %d,", pagecount);
    printf("\"runs\": %d,", runs);
    printf("\"time\": %f}\n", time);
    // LOG_RESULTS(pagecount, runs, time, "walk list + sfence after each run");
    
    // close json
    printf("\t\t}");
}


static void test_nontemporal_behavior(const unsigned pagecount, const unsigned runs, void *memory)
{
    size_t size = PAGE_SIZE * pagecount;
    __m256i *data = (__m256i *)memory; 
    __m256i fill;
    unsigned start, end;
    unsigned long long time = 0;

    // printf( "\"%s runs %u pages %u\": {", __PRETTY_FUNCTION__, runs, pagecount);
    for (unsigned run = 0; run < runs; run++) {
        for (unsigned index = 0; index < size / sizeof(__m256i); index++) {
            memset(&fill, index, sizeof(fill));
            start = _rdtsc();
            _mm256_stream_si256(&data[index], fill);
            end = _rdtsc();
            time += end - start;
        }
    }

    printf("\t\t\"%s\": {\"size\": %zu, \"ticks\": %lu}", __PRETTY_FUNCTION__, size, time);
    
    // printf( "\"non-temporal move\":");
    // printf( "{\"size\": %zu, \"time\": %lu}\n", size, time);
    // printf( " },\n");
}

typedef void (*cache_test_t)(const unsigned pagecount, const unsigned runs, const void *memory);

cache_test_t cache_tests[] = {
#if 0
    (cache_test_t)test_cache_behavior_1,
    (cache_test_t)test_cache_behavior_2,
    (cache_test_t)test_cache_behavior_3,
    (cache_test_t)test_cache_behavior_4,
    (cache_test_t)test_cache_behavior_5,
    (cache_test_t)test_cache_behavior_6,
    (cache_test_t)test_cache_behavior_7,
    (cache_test_t)test_cache_behavior_8,
    (cache_test_t)test_cache_behavior_9,
    (cache_test_t)test_nontemporal_behavior, 
#endif // 0
    (cache_test_t)test_cache_behavior_10,
    NULL,
};

void test_cache_behavior(unsigned specific_test, const unsigned pagecount, int fd, const unsigned runs)
{
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    unsigned length = pagecount;
    unsigned start, end;
    double time;
    record_t *r = NULL;
    int mmflags = MAP_PRIVATE;

    assert(PAGE_SIZE == pagesize);

    if (-1 == fd) {
        mmflags |= MAP_ANONYMOUS;
    }

    // we must create new mappings each time or we could be seeing caching artifacts
    for (unsigned index = 0; NULL != cache_tests[index]; index++) {

        // ok, not efficient, but it is easy to implement - single test choice option
        if ((specific_test != (unsigned) ~0) && (index != specific_test)) {
            continue;
        }
        void *memory = mmap(NULL, pagecount * pagesize, PROT_READ | PROT_WRITE, mmflags, fd, 0);

        assert(NULL != memory);

        if (MAP_FAILED == memory) {
            printf( "%s: mmap failed fd = %d, errno = %d (%s)\n", __PRETTY_FUNCTION__, fd, errno, strerror(errno));
            return;
        }

        memset(memory, 0, pagecount * pagesize);

#if 0
        if ((((unsigned)~0) == specific_test) && (index > 0)) {
            printf(",\n");
        }
#endif // 0
        cache_tests[index](pagecount, runs, memory);
        if (NULL != cache_tests[index+1]) {
            printf(",\n");
        }
        if (munmap(memory, pagecount * pagesize) < 0) {
            perror("munmap");
            exit(-1);
        }
        memory = NULL;
    }
  
}

/// end experimental code

const char *USAGE = "\
usage:             \n\
\t%s [options]     \n\
options:           \n\
\t-d    Direct Access Memory file to use. [default=DRAM]\n\
\t-h    Show this help message.\n\
\t-l    Write output to the specified log file. [default=stdout]\n\
\t-p    Logical processor to use. [default=2]\n\
\t-t    Specific test to run (default is all)\n\
\t-s    Specific sample to use (default is all)\n\
";

// options information
static struct option gLongOptions[] = {
    {"daxmem",  required_argument, NULL, 'd'},
    {"help",    no_argument, NULL, 'h'},
    {"processor",     required_argument, NULL, 'p'},
    {"log",     required_argument, NULL, 'l'},
    {"runs",    required_argument, NULL, 'r'},
    {"test",    required_argument, NULL, 't'},
    {"sample",  required_argument, NULL, 's'},
    {NULL, 0, NULL, 0} // marks end of the array
};

int main(int argc, char **argv) 
{
    int option_char = '\0';
    int clsize = 0;
    unsigned long long timestamp1;
    // unsigned long long timestamp2;
    cpu_cache_data_t *cd;
    char *daxmem = NULL;
    char *logfname = NULL;
    // static const unsigned samples[] = { 4, 6, 8, 10, 12, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
    static const unsigned samples[] = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576};
    unsigned processor = 2;
    cpu_set_t cpuset;
    pthread_t self = pthread_self();
    int status;
    unsigned runs = 10;
    unsigned test = (unsigned) ~0;
    unsigned sample = (unsigned) ~0;

    logfile = stderr;

    while (-1 != (option_char = getopt_long(argc, argv, "s:t:r:d:hl:p:", gLongOptions, NULL))) {
        switch(option_char) {
            default:
                printf( "Unknown option -%c\n", option_char);
            case 'h': // help
                printf(USAGE, argv[0]);
                exit(EXIT_FAILURE);
                break;
            case 'l': // logfile
                logfname = strdup(optarg);
                break;
            case 'd': // file to map (presumably dax)
                daxmem = strdup(optarg);
                break;
            case 'r': { // number of runs
                int r = atoi(optarg);
                if (r < 1 || r > 1000000) {
                    printf("number of runs must be between 1 and 100000\n");
                    printf(USAGE, argv[0]);
                    exit(EXIT_FAILURE);
                }
                runs = (unsigned) r;
                break;
            }
            case 'p': { // CPU to use
                    int cpu = atoi(optarg);
                    if (cpu < 0) {
                        printf("processor number (%d) must be >= 0\n", cpu);
                        printf(USAGE, argv[0]);
                        exit(EXIT_FAILURE);
                    }
                    if (cpu >= get_nprocs()) {
                        printf("processor number (%d) must be < %d\n", cpu, get_nprocs());
                        printf(USAGE, argv[0]);
                        exit(EXIT_FAILURE);
                    }
                    processor = (unsigned)cpu;
                }
                break;
            case 's': { // specific sample to use
                int s = atoi(optarg);
                unsigned max_sample = sizeof(samples) / sizeof(unsigned);
                if ((s < 1) || (s > max_sample)) {
                    printf("sample number must be between 1 and %u\n", max_sample);
                    printf(USAGE, argv[0]);
                    exit(EXIT_FAILURE);
                }
                sample = (unsigned)s - 1;
                break;
            }
            case 't': { // specific test to use
                int t = atoi(optarg);
                unsigned max_test = sizeof(cache_tests) / sizeof(cache_test_t);
                if ((t < 1) || (t >= max_test)) {
                    printf("test number must be between 1 and %u\n", max_test-1);
                    printf(USAGE, argv[0]);
                    exit(EXIT_FAILURE);
                }
                test = (unsigned)t-1;
                break;
            }
        }
    }

    setbuf(stdout, NULL);
    
    // CPU_ZERO(&cpuset);
    // CPU_SET(processor, &cpuset);
    // status = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    status = setprocessor(processor);
    if (0 != status) {
        printf("pthread_setaffinty_np failed (%d): %s\n", status, strerror(status));
        exit(EXIT_FAILURE);
    }

    if (NULL != logfname) {
        logfile = fopen(logfname, "w");
        if (NULL == logfname) {
            printf( "%s: Unable to open logfile %s (errno %d, %s)\n", __PRETTY_FUNCTION__, logfname, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
        fclose(logfile);
        logfile = NULL;

        logfile = freopen(logfname, "a+", stdout);
        assert(logfile == stdout);
        logfile = freopen(logfname, "a+", stderr);
        assert(logfile == stderr);
        setbuf(stderr, NULL);
    }

    cpu_init();
    timestamp1 = cpu_rdtsc();
  	clsize = cpu_cacheline_size();

    printf( "{ \"system description\": {\n");
    printf( "\"processor\": %u,\n", processor);
    if (NULL != daxmem) {
        printf( "\"file backing memory\": \"%s\",\n", daxmem);
    }
    printf( "\"RTM\" : %d,\n", cpu_has_rtm() ? 1 : 0);
    printf( "\"HLE\" : %d,\n", cpu_has_hle() ? 1 : 0);
    printf( "\"CLFLUSHOPT\": %d,\n", cpu_has_clflushopt() ? 1 : 0);
    printf( "\"CLWB\": %d,\n", cpu_has_clwb() ? 1 : 0);
    printf( "\"cache line size\": %d,\n", clsize);
    for (unsigned index = 0; index < 6; index++) {
        unsigned int size = __cacheSize(index);
        if (size > 0) {
            printf( "\"L%u cache size (KB)\": %u,\n", index, size);
        }
    }

    assert(clsize == sizeof(record_t)); // sanity check - if this is wrong, the code needs to be fixed
    // printf("%s: Ticks per second: 0x%d\n", __PRETTY_FUNCTION__, cpu_frequency()); // <- this seems to be useless

    // (void) cache_test();
    // timestamp2 = cpu_rdtsc();
    // printf("%s: Preamble elapsed time is %llu\n", __PRETTY_FUNCTION__, timestamp2 - timestamp1);

    printf( "\"cache info\": \" ");
    for (unsigned index = 0; ; index++) {
        cd = cpu_get_cache_info(index);
        if (NULL == cd) {
            break;
        }

        // TODO: add more data dump logic here?
        cpu_free_cache_info(cd);
        cd = NULL;
    }
    printf( "\"},\n");

    if (NULL != daxmem) {
        printf( "\"backing file test\": {\n");
    }
    else {
        printf("\"memory test\": {\n");
    }

    for (unsigned index = 0; index < sizeof(samples)/sizeof(samples[0]); index++) {
        int memfd = -1;

        // only process the requested sample size
        if (((unsigned)~0 != sample)  && (index != sample)) {
            continue;
        }

        if (NULL != daxmem) {
            char *zero = NULL;
            unsigned start, end;
            double time;

            printf( "\t\"size %dKB\": {\n", 4 * samples[index]);
            (void) unlink(daxmem);
            memfd = open(daxmem, O_CREAT | O_RDWR, 0644);
            if (0 > memfd) {
                printf( "%s: unable to create %s (%d %s)\n", __PRETTY_FUNCTION__, daxmem, errno, strerror(errno));
                exit(EXIT_FAILURE);
            }
            zero = malloc(PAGE_SIZE);
            assert(NULL != zero);
            memset(zero, 0, PAGE_SIZE);

            printf( "\t\t\"write time test\": [");
            for (unsigned run = 0; run < runs; run++) {
                time = 0.0;
                for (unsigned index2 = 0; index2 < samples[index]; index2++) {
                    start = _rdtsc();
                    write(memfd, zero, PAGE_SIZE);
                    end = _rdtsc();
                    time += end - start;
                }
                printf( "%f%s", time, run + 1 < runs ? ", ": "],\n");
            }
            printf( "\t\t\"fsync time test\": [");
            for (unsigned run = 0; run < runs; run++) {
                time = 0.0;
                start = _rdtsc();
                fsync(memfd);
                end = _rdtsc();
                time = end - start;

                printf( "%f%s", time, run + 1 < runs ? ", " : "],\n");
                // printf( "%s: fsync ticks for %dKB blocks is %f\n", __PRETTY_FUNCTION__, 4 * samples[index], time);
            }
        }
        test_cache_behavior(test, samples[index], memfd, runs);
        if (index < sizeof(samples)/sizeof(samples[0])) {
            printf(",");
        }
        printf("\n");
        if (NULL != daxmem) {
        }
        close(memfd);
        memfd = -1;
    }

    if (NULL != daxmem) {
        printf( "\t\t\"backing file\": \"%s\",\n", daxmem);
    }
    else {
        printf("\t\"backing file\": \"anonymous\",\n");
    }
    printf("\t\"number of runs\": %d\n}", runs);

#if 0
    _mm_sfence();
    _mm_stream_pi();
    _mm_stream_ps();
    _mm256_stream_ps();
    _mm_prefetch();
    _mm_clflush();
    _mm_clflushopt();
    _mm_clwb();
#endif // 0

    return (0);
}
