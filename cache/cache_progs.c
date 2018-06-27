#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>
#include <x86intrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <float.h>
#include "cache.h"
#include "cpu.h"



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
printf("%3.3d, %4d, %14.2f, %24.24s, %s\n", pages, runs, time, __PRETTY_FUNCTION__, description)

static void flush_cache(void)
{
    const size_t buffer_length = 32 * 1024 * 1024;
    static char buffer[buffer_length];
    static char c;

    memset(buffer, c++, buffer_length);
}

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


static unsigned long test_cache_clflush_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    assert(cpu_has_clflush());

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

static unsigned long test_cache_clflushopt_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    assert (cpu_has_clflushopt());

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

static unsigned long test_cache_clwb_sfence_every_n_updates(record_page_t *rp, unsigned n)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    assert(cpu_has_clwb());

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

static void test_cache_behavior_clflush(const unsigned pagecount, const unsigned runs, void *memory)
{
    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;
    unsigned time, start, end;

    if (!cpu_has_clflush()) {
        // can't test clflushopt if it isn't supported
        return;
    }

    // TODO: finish this (I've just started doing so)

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                /* stick the calls to the various stubs in here. */
            }
        }
    }


}



static unsigned long test_cache_noflush_sfence_every_update(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        _mm_sfence();
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_noflush_sfence_every_2_updates(record_page_t *rp)
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
        time += (end - start);
        count++;
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        _mm_sfence();
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_noflush_sfence_every_3_updates(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next; // note this assumes there is an even number of  
        end = _rdtsc();
        time += end - start;
        count++;
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next; // note this assumes there is an even number of  
        end = _rdtsc();
        time += end - start;
        count++;
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        _mm_sfence();
        end = _rdtsc();
        time += end - start;
        count++;
    }

    // report the amount of CPU time
    return time;

}

static unsigned long test_cache_noflush_sfence_every_4_updates(record_page_t *rp)
{
    record_t *r = rp->records[0].s.next;
    unsigned long start, end, time;
    unsigned count = 0;

    time = 0;

    while (r != &rp->records[0]) {
        // 1
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }
        // 2
        start = _rdtsc();
        r->s.counter++;
        r = r->s.next;
        end = _rdtsc();
        time += end - start;
        count++;
        if (r == &rp->records[0]) {
            start = _rdtsc();
            _mm_sfence();
            end = _rdtsc();
            time += end - start;
            break;
        }
        // 3
        start = _rdtsc();
        r->s.cou    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        r = r->s    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        end = _r    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        time +=     record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        count++;    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        if (r ==    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


            star    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


            _mm_    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


            end     record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


            time    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


            brea    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        }
        // 4
        start =     record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        r->s.cou    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        r = r->s    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        _mm_sfen    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        end = _r    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        time +=     record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


        count++;    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }


    }

    // report the amount of CPU time
    return time;

}



static void test_cache_clflushopt(const unsigned pagecount, const unsigned runs, void *memory) 
{
    record_page_t *rp = (record_page_t *)memory;
    record_t *r = NULL;

    if (!cpu_has_clflushopt()) {
        // can't test clflushopt if it isn't supported
        return;
    }
    unsigned time, start, end;

    // This weaves the offset cache blocks of the given memory into a linked list
    init_cache_test_memory(pagecount, memory); 

    for (unsigned index = 0; index < runs; index++) {
        for (unsigned index2 = 0; index2 < RECORDS_PER_PAGE ; index2++) {
            r = rp[index2].records[0].s.next;
            while (r != &rp[index2].records[0]) {
                start = _rdtsc();
                r->s.counter++;
                r = r->s.next;
                end = _rdtsc();
                time += (end - start);
            }
        }
    }

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
 
    if (12 != pagecount) {
        // only need to test this one case.
        return;
    }

    init_cache_test_memory(pagecount, memory);
    (void) runs;

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

    time = ((double) end - start);

    fprintf(stderr, "%s: priming step took %f ticks\n", __PRETTY_FUNCTION__, time);

    // now re-run it
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

    time = ((double) end - start);

    fprintf(stderr, "%s: first run sequence took %f ticks\n", __PRETTY_FUNCTION__, time);

    start = _rdtsc();
    _mm_clflush(r0);
    end = _rdtsc();

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for clean cache line flush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 4 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 4 writes on same cache line with clflush and fence\n", __PRETTY_FUNCTION__, time);


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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 6 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 7 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);


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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 8 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);


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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 9 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 10 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 11 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 16 writes on same cache line with clflush\n", __PRETTY_FUNCTION__, time);

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

    time = ((double) end - start);

    fprintf(stderr, "%s: clflush took %f ticks for 16 writes on same cache line with clflush + fence\n", __PRETTY_FUNCTION__, time);


}

static void test_cache_behavior_5(const unsigned pagecount, const unsigned runs, void *memory)
{
    static unsigned done = 0;
    unsigned start, end;
    double time = 0.0;
    record_t *r, *r2;
    int xresult = 36;
    int hits = 0;
    record_page_t *recpages = (record_page_t *)memory;


    if (pagecount != 12) {
        // this is our magic number - ignore all others.
        return;
    }

    if (0 == cpu_has_rtm()) { // no RTM, nothing to do
        if (!done) {
            LOG_RESULTS(pagecount, 0, 0.0, "No RTM support");
            done = 1;
        }
        return;
    }

    fprintf(stderr, "starting %s\n", __FUNCTION__);

    //
    // Build a list of starting addresses.  Staggering is my attempt to foil the prefetcher
    //
    init_cache_test_memory(pagecount, memory);
    
    for (unsigned run = 1; run < RECORDS_PER_PAGE; run++) {
        unsigned status = 0;
        unsigned retries = 0;
        char committed = 0;

        time = 0.0;
        // fprintf(stderr, "starting to test run %u\n", run);
        while (1) {
            record_t *r0_start = &recpages[0].records[0];
            record_t *r0 = r0_start;
            record_t *r1_start = &recpages[0].records[run];
            record_t *r1 = r1_start;

            // fprintf(stderr, "testing stride 0x%x\n", run);
#if 0
            do {
                fprintf(stderr, "0x%p points to 0x%p\n", r0, r0->s.next);
                r0 = r0->s.next;
            } while (r0 != r0_start);
#endif // 0

            // prime the cache
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0->s.next;
            r0 = r0_start;


            committed = -1;
            start = _rdtsc();
            status = _xbegin();
            if (_XBEGIN_STARTED == status) {
                r0->s.counter++; // 1
                r0 = r0->s.next;
                r0->s.counter++; // 2
                r0 = r0->s.next;
                r0->s.counter++; // 3
                r0 = r0->s.next;
                r0->s.counter++; // 4
                r0 = r0->s.next;
                r0->s.counter++; // 5
                r0 = r0->s.next;
                r0->s.counter++; // 6
                r0 = r0->s.next;
                r0->s.counter++; // 7
                r0 = r0->s.next;
                r0->s.counter++; // 8
                r0 = r0->s.next;
                r0->s.counter++; // 9               
                _xend();
                committed = 1;
            }
            end = _rdtsc();
            time = ((double)end - start);
            fprintf(stderr, "transaction time was %f\n", time);
            r0 = r0_start;
            start = _rdtsc();
            r0->s.counter++;
            end = _rdtsc();
            time = ((double)end - start);
            fprintf(stderr, "load time was %f\n", time);
            if (1 != committed) {

                if(0x8 == (0x8 & status)) {
                    retries++;
                    if (retries > 100) {
                        time /= (double) retries;
                        fprintf(stderr, "multiple aborts on line 0x%x\n", run);
                        break;
                    }
                    continue;
                }
                fprintf(stderr, "abort on line 0x%x (status 0x%x)\n", run, status);
            }

            // fprintf(stderr, "run 0x%x does not conflict\n", run);

            break;
        }
    }

#if 0
        unsigned retries = 0;
        unsigned original_count = record_array[0]->s.counter;
        char committed = 0;





        while (retries++ < 10) {
            committed = 0;
            r = record_array[0];
            r2 = record_array[index];
            xresult = ~0;
            flush_cache();
            if (_XBEGIN_STARTED ==_xbegin()) {
                do {
                    r->s.counter++;
                    r = r->s.next;
                } while(r != record_array[0]);
                r2->s.counter++;
                _xend();
                if (original_count == record_array[0]->s.counter) {
                    asm("movl %%eax, %0;" : "=r" (xresult): );
                    if (0x2 & xresult) continue;
                }
                else {
                    committed = 1;
                }
                break;
            }
        }

        if (1 == committed) {
            LOG_RESULTS(pagecount, xresult, (double)index, "Appears to be same associative set");
            hits++;
        }
        else {
            LOG_RESULTS(pagecount, xresult, (double)index, "Appears to be a different associative set");
        }
    }
    LOG_RESULTS(pagecount, hits, 0.0, "Hitcount (in runs)");
#endif // 0
}

static void test_cache_behavior_4(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    double time = 0.0;

    start = cpu_rdtsc();
    end = cpu_rdtsc();
    time = ((double)(end - start));
    // LOG_RESULTS(pagecount, 1, time, "run nop 1 billion times");

}

static void test_cache_behavior_3(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    double time = 0.0;
    static int done = 0;

    if (done) {
        return;
    }

    start = cpu_rdtsc();
    for (unsigned index = 1000 * 1000 * 1000; index > 0; index--) {
        asm("nop\n\t");
    }
    end = cpu_rdtsc();
    time = ((double)(end - start));
    time /= (double)(1000 * 1000 * 1000);
    LOG_RESULTS(pagecount, 1, time, "run nop 1 billion times");

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
#if 1
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
#else
        start = cpu_rdtsc();
        for (unsigned index = 0; index < RECORDS_PER_PAGE; index++) {
            r = ((record_t *)memory) + index;
            for (unsigned index2 = 0; index2 < pagecount; index2++) {
                record_t *n;
                n = r->s.next = (void *)(((uintptr_t)memory) + (((index2 + 1) % pagecount) <<12));
                r->s.counter = 0;
                r = n;
            }
        }
        end = cpu_rdtsc();
        time += ((double)(end-start));
#endif 
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

    for (unsigned index = 1; index < RECORDS_PER_PAGE; index++) {
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

        if (RECORDS_PER_PAGE - 1 == index) {
            snprintf(buffer, sizeof(buffer), "run min %f (%u) max %f (%u)", min_time, min_index, max_time, max_index);
            LOG_RESULTS(pagecount, runs, max_time, buffer);
        }
    }

}

static void test_cache_behavior_1(const unsigned pagecount, const unsigned runs, void *memory)
{
    unsigned start, end;
    double time;
    record_t *r = NULL;
    assert(NULL != memory);

    flush_cache();
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
    LOG_RESULTS(pagecount, 1, time, "initialize first record of each page");

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
    LOG_RESULTS(pagecount, runs, time, "walk list");

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
    LOG_RESULTS(pagecount, runs, time, "walk list + prefetch next entry");
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
    LOG_RESULTS(pagecount, runs, time, "walk list + prefetch next entry + clflush after each write");
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
    LOG_RESULTS(pagecount, runs, time, "walk list + clflush after each write");
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
    LOG_RESULTS(pagecount, runs, time, "walk list + sfence after each write");
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
    LOG_RESULTS(pagecount, runs, time, "walk list + sfence after each run");

}

typedef void (*cache_test_t)(const unsigned pagecount, const unsigned runs, const void *memory);

cache_test_t cache_tests[] = {
    // (cache_test_t)test_cache_behavior_1,
    // (cache_test_t)test_cache_behavior_2,
    // (cache_test_t)test_cache_behavior_3,
    // (cache_test_t)test_cache_behavior_4,
    // (cache_test_t)test_cache_behavior_5,
    (cache_test_t)test_cache_behavior_6,
    NULL,
};

void test_cache_behavior(const unsigned pagecount)
{
    const unsigned runs = 100;
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    unsigned length = pagecount;
    unsigned start, end;
    double time;
    record_t *r = NULL;

    assert(PAGE_SIZE == pagesize);

    // we must create new mappings each time or we could be seeing caching artifacts
    for (unsigned index = 0; NULL != cache_tests[index]; index++) {
        void *memory = mmap(NULL, pagecount * pagesize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        assert(NULL != memory);
        cache_tests[index](pagecount, runs, memory);
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
\t-h    Show this help message.\n\
";

// options information
static struct option gLongOptions[] = {
    {"help",    no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0} // marks end of the array
};

int main(int argc, char **argv) 
{
    int option_char = '\0';
    int clsize = 0;
    unsigned long long timestamp1, timestamp2;
    cpu_cache_data_t *cd;
    static const unsigned samples[] = {4, 6, 8, 12, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

    setbuf(stdout, NULL); // disable buffering

    while (-1 != (option_char = getopt_long(argc, argv, "h", gLongOptions, NULL))) {
        switch(option_char) {
            default:
                fprintf(stderr, "Unknown option -%c\n", option_char);
            case 'h': // help
                printf(USAGE, argv[0]);
                exit(1);
                break;
        }
    }

    printf("Starting\n");
    cpu_init();
    timestamp1 = cpu_rdtsc();
  	clsize = cpu_cacheline_size();

	printf("RTM: %s\n", cpu_has_rtm() ? "Yes" : "No");
	printf("HLE: %s\n", cpu_has_hle() ? "Yes" : "No");
	printf("CLFLUSHOPT: %s\n", cpu_has_clflushopt() ? "Yes" : "No");
	printf("CLWB: %s\n", cpu_has_clwb() ? "Yes" : "No");
	printf("CLFLUSH cache line: 0x%x\n", clsize);
    printf("Ticks per second: 0x%d\n", cpu_frequency());

    // (void) cache_test();
    timestamp2 = cpu_rdtsc();
    printf("elapsed time is %llu\n", timestamp2 - timestamp1);
    printf("Finished\n");


    for (unsigned index = 0; ; index++) {
        cd = cpu_get_cache_info(index);
        if (NULL == cd) {
            break;
        }
        
        cpu_free_cache_info(cd);
        cd = NULL;
        printf("index is %u\n", index);
    }

    for (unsigned index = 0; index < sizeof(samples)/sizeof(samples[0]); index++) {
        test_cache_behavior(samples[index]);
    }

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
