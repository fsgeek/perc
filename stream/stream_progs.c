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
#include <numa.h>
#include "cache.h"
#include "cpu.h"
#include "setprocessor.h"

// This is here to quiet the IDE complaining this isn't defined.
#if !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS (0x20)
#endif

static FILE *logfile;

#if !defined(C_ASSERT)
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif // C_ASSERT

#if !defined(PAGE_SIZE)
#define PAGE_SIZE (4096)
#endif // PAGE_SIZE

const char *USAGE = "\
usage:             \n\
\t%s [options]     \n\
options:           \n\
\t-f    First file, used by primary [default=DRAM]\n\
\t-n    Next file, used by secondary [default=DRAM]\n\
\t-h    Show this help message.\n\
\t-l    Write output to the specified log file. [default=stdout]\n\
\t-p    Socket for primary test [default=0]\n\
\t-s    Socket for secondary test [default=0]\n\
";

// options information
static struct option gLongOptions[] = {
    {"first", optional_argument, NULL, 'f'},
    {"next", optional_argument, NULL, 'n'},
    {"help",    no_argument, NULL, 'h'},
    {"log",     optional_argument, NULL, 'l'},
    {"primary",     optional_argument, NULL, 'p'},
    {"secondary",  optional_argument, NULL, 's'},
    {NULL, 0, NULL, 0} // marks end of the array
};

static unsigned __int64 sequential_write(void *destination, size_t length, unsigned long long iterations)
{
    __int64 *random_data = malloc(length);
    unsigned retries = 0;
    unsigned __int64 start, end;

    assert(NULL != random_data);

    for (unsigned long long index; index < length / sizeof(__int64); index++) {
        while (0 == _rdrand64_step(((unsigned __int64 *)&random_data[index]))) {
            retries++;
        }
    }
    assert(0 == retries);

    start = __rdtsc();
    memcpy(destination, random_data, length);
    end = _rdtsc();

    free(random_data);
    random_data = NULL;

    return end - start;
}

static unsigned __int64 backwards_write(void *destination, size_t length, unsigned long long iterations)
{
    __int64 *random_data = malloc(length);
    unsigned retries = 0;
    unsigned __int64 start, end;
    char *target = (char *)destination;

    assert(NULL != random_data);

    for (unsigned long long index  = 0; index < length / sizeof(__int64); index++) {
        while (0 == _rdrand64_step(((unsigned __int64 *)&random_data[index]))) {
            retries++;
        }
    }
    assert(0 == retries);

    start = __rdtsc();
    for (unsigned long long index = length; index > 0; index--) {
        *(target + index) = *(((char *)random_data)+index);
    }
    end = _rdtsc();

    free(random_data);
    random_data = NULL;

    return end - start;
}


static unsigned __int64 random_write(void *destination, size_t length, unsigned long long iterations)
{
    size_t offset;
    unsigned long long retries = 0;
    __int64 *source, *sink;
    unsigned __int64 start, end, total;
    unsigned __int64 random_data;
    char *target = (char *)destination;

    total = 0;
    for (unsigned long long index = 0; index < iterations; index++) {

        while(0 == _rdrand64_step((unsigned __int64 *)&offset)) {
            retries++;
            /* try again */
        }
        offset %= length;

        while (0 == _rdrand64_step((unsigned __int64 *)&random_data)) {
            assert(0);
        }

        sink = ((__int64 *)destination) + (offset / sizeof(__int64));
        start = __rdtsc();
        _mm_stream_si64(sink++, random_data); // 1 
        _mm_stream_si64(sink++, random_data); // 2
        _mm_stream_si64(sink++, random_data); // 3
        _mm_stream_si64(sink++, random_data); // 4
        _mm_stream_si64(sink++, random_data); // 5
        _mm_stream_si64(sink++, random_data); // 6
        _mm_stream_si64(sink++, random_data); // 7
        _mm_stream_si64(sink++, random_data); // 8
        _mm_stream_si64(sink++, random_data); // 9
        _mm_stream_si64(sink++, random_data); // 10
        _mm_stream_si64(sink++, random_data); // 11
        _mm_stream_si64(sink++, random_data); // 12
        _mm_stream_si64(sink++, random_data); // 13
        _mm_stream_si64(sink++, random_data); // 14
        _mm_stream_si64(sink++, random_data); // 15
        _mm_stream_si64(sink, random_data); // 16
        _mm_mfence();
        end = _rdtsc();
        total += end - start;
    }

    return total;
}



int main(int argc, char **argv) 
{
    int option_char = '\0';
    int clsize = 0;
    unsigned long long timestamp1;
    // unsigned long long timestamp2;
    cpu_cache_data_t *cd;
    char *daxmem = NULL;
    char *logfname = NULL;
    static const unsigned samples[] = {4, 6, 8, 12, 16, 32, 64, /* 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 */};
    unsigned processor = 2;
    cpu_set_t cpuset;
    pthread_t self = pthread_self();
    int status;
    unsigned runs = 10;
    unsigned test = (unsigned) ~0;
    unsigned sample = (unsigned) ~0;
    unsigned node_count;
    unsigned primary_core = ~0;
    unsigned secondary_core = ~0;
    char *primary_file = NULL;
    char *secondary_file = NULL;
    void *primary_memory = NULL;
    void *secondary_memory = NULL;
    size_t buffer_size = 1024 * 1024 * 1024; // 1GB
    static unsigned __int64 primary_time;
    unsigned long long iterations = 1000000; // low number for testing purposes

    setbuf(stdout, NULL);
    logfile = stderr;

    while (-1 != (option_char = getopt_long(argc, argv, "f:n:hl:p:s:", gLongOptions, NULL))) {
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
            case 'f': // first mapping file
                primary_file = strdup(optarg);
                break;
            case 'n':  // second mapping file
                secondary_file = strdup(optarg);
                break;
            case 'p': // primary core
                primary_core = (unsigned)atoi(optarg);
                break;
            case 's': // secondary core
                secondary_core = (unsigned)atoi(optarg);
                break;
        }
    }

    if (numa_available() < 0) {
        node_count = 1;
    }
    else {
        node_count = numa_max_node();
    }

    /* for now I'm going to hard code this... */
    if (primary_core > get_nprocs()) {
        primary_core = 1;
    }

    if (secondary_core > get_nprocs()) {
        secondary_core = get_nprocs() - 1;
    }

    printf("using %u for primary core and %u for secondary core\n", primary_core, secondary_core);

    if (NULL == primary_file) {
        primary_memory = malloc(buffer_size);
    }

    cpu_init();

    /* initial pass is small numbers for testing purposes */
    primary_time = random_write(primary_memory, buffer_size, iterations);
    printf("random write time for primary is %lu over %lu iterations\n", primary_time, iterations);

    primary_time = sequential_write(primary_memory, buffer_size, iterations);
    printf("sequential write time for primary is %lu over %lu iterations\n", primary_time, iterations);

    return (0);
}
