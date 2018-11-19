#define _GNU_SOURCE
#include <stdio.h>
#if !defined(_Float128)
#define _Float128 __float128
#endif
#if !defined(_Float64)
#define _Float64 long double
#endif
#if !defined(_Float64x)
#define _Float64x long double
#endif
#if !defined(_Float32)
#define _Float32 float
#endif
#if !defined(_Float32x)
#define _Float32x double
#endif

//-D_Float128=__float128
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
\t-i    Number of test iterations [default=1]\n\
";

// options information
static struct option gLongOptions[] = {
    {"first", optional_argument, NULL, 'f'},
    {"next", optional_argument, NULL, 'n'},
    {"help",    no_argument, NULL, 'h'},
    {"log",     optional_argument, NULL, 'l'},
    {"primary",     optional_argument, NULL, 'p'},
    {"secondary",  optional_argument, NULL, 's'},
    {"test", no_argument, NULL, 't'},
    {"iterations", optional_argument, NULL, 'i'},
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
    for (unsigned long long index = length-sizeof(unsigned __int64); index > 0; index--) {
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
    size_t bytes_copied;

    total = 0;
    for (unsigned long long index = 0; index < iterations; index++) {

        bytes_copied = 0;
        while (bytes_copied < length) {
            retries = 0;
            while(0 == _rdrand64_step((unsigned __int64 *)&offset)) {
                retries++;
                /* try again */
                assert(retries < 100);
            }
            offset %= (length-128);

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
            _mm_stream_si64(sink++, random_data); // 8 - 64 bytes total
            _mm_stream_si64(sink++, random_data); // 9
            _mm_stream_si64(sink++, random_data); // 10
            _mm_stream_si64(sink++, random_data); // 11
            _mm_stream_si64(sink++, random_data); // 12
            _mm_stream_si64(sink++, random_data); // 13
            _mm_stream_si64(sink++, random_data); // 14
            _mm_stream_si64(sink++, random_data); // 15
            _mm_stream_si64(sink, random_data); // 16 - 128 bytes total
            //_mm_mfence();
            end = _rdtsc();
            total += end - start;
            bytes_copied += 128;
        }
    }

    return total;
}

typedef unsigned __int64 (*write_test_t)(void *destination, size_t length, unsigned long long iterations);

typedef struct {
    unsigned            which_cpu;
    pthread_t           which_thread;
    write_test_t        write_test;
    void               *buffer;
    size_t              buffer_length;
    unsigned long long  iterations;
    unsigned long long  clock_ticks; // filled upon completion
} test_config_t;

static pthread_cond_t worker_wait_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t worker_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
static int worker_run = 0;

void worker_wait(void)
{
    while (0 == worker_run) {
        pthread_mutex_lock(&worker_wait_mutex);
        pthread_cond_wait(&worker_wait_cond, &worker_wait_mutex);
        pthread_mutex_unlock(&worker_wait_mutex);
    }
}

void worker_block(void) {
    worker_run = 0;
}

void worker_unblock(void) {
    pthread_mutex_lock(&worker_wait_mutex);
    worker_run = 1;
    pthread_mutex_unlock(&worker_wait_mutex);
    pthread_cond_broadcast(&worker_wait_cond);
}

void *stream_worker(void *context)
{
    test_config_t *config = (test_config_t *)context;

    // sanity checks
    assert(config->write_test);
    assert(config->buffer);
    assert(config->buffer_length > 0);
    assert(config->iterations > 0);

    // thread blocks and waits for the ready signal
    worker_wait();

    config->clock_ticks = config->write_test(config->buffer, config->buffer_length, config->iterations);

    pthread_exit(config);    
}

static void *create_test_memory(const unsigned pagecount, const char *file_name)
{
    int fd = -1;
    void *zero;
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    void *memory = NULL;

    if (NULL != file_name) {
        (void)unlink(file_name);
        fd = open(file_name, O_CREAT | O_RDWR, 0644);
        if (0 > fd) {
            printf( "%s: unable to create %s (%d %s)\n", __PRETTY_FUNCTION__, file_name, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        zero = malloc(PAGE_SIZE);
        assert(NULL != zero);
        memset(zero, 0, PAGE_SIZE);

        for (unsigned index = 0; index < pagecount; index++) {
            write(fd, zero, pagesize);
        }

        memory = mmap(NULL, pagecount * pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        assert(NULL != memory);

        close(fd);
        fd = -1;
     }
     else {
         memory = malloc(pagesize * pagecount);
         assert(NULL != memory);
         memset(memory, 0, pagesize * pagecount);
     }

     return memory;
}

static void cleanup_test_memory(void *memory, const unsigned pagecount, const char *file_name)
{
    const size_t pagesize = sysconf(_SC_PAGESIZE);

    assert(NULL != memory);
    if (NULL != file_name) {
        munmap(memory, pagesize * pagecount);
        unlink(file_name);
    }
    else {
        free(memory);
    }
    memory = NULL;
    return;
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
    unsigned long long iterations = 1; // low number for testing purposes
    int test_mode = 0;
    int code;
    test_config_t test_config[2];
    struct {
        const char *name;
        write_test_t test;
    } test_data[] = {
        {"sequential_write", sequential_write},
        {"random_write", random_write},
        {"backwards_write", backwards_write},
        {NULL, NULL}
    };
    pthread_attr_t thread_attr;
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    unsigned current_cpu, current_node;

    setbuf(stdout, NULL);
    logfile = stderr;

    while (-1 != (option_char = getopt_long(argc, argv, "tf:n:hl:p:s:i:", gLongOptions, NULL))) {
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
            case 't': // test mode;
                test_mode = 1;
                break;
            case 'i': //iteration count
                iterations = (unsigned)atoi(optarg);
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

    assert(primary_core != secondary_core);

    cpu_init();

    if (test_mode) {
        
        cpu_set_t cpuset;
        pthread_attr_t thread_attr;


        test_config[0].which_cpu = primary_core;
        test_config[0].buffer = create_test_memory(buffer_size / pagesize, primary_file);
        test_config[0].buffer_length = buffer_size;
        test_config[0].iterations = iterations;
        test_config[0].clock_ticks = 0;

        printf("single core test run, core %u, copy size %zu, iterations %lu\n", test_config[0].which_cpu, buffer_size, iterations);

        worker_block();
        pthread_attr_init(&thread_attr);
        CPU_ZERO(&cpuset);
        CPU_SET(test_config[0].which_cpu, &cpuset);
        code = pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset);
        assert(0 == code);

        test_config[0].write_test = sequential_write;
        code = pthread_create(&test_config[0].which_thread, &thread_attr, stream_worker, &test_config[0]);
        assert(0 == code);
        // unblock the blocked worker threads
        worker_unblock();

        code = pthread_join(test_config[0].which_thread, NULL);
        assert(0 == code);
        primary_time = test_config[0].clock_ticks; 
        printf("sequential write time is %lu\n", primary_time);

        worker_block();
        test_config[0].write_test = random_write;
        code = pthread_create(&test_config[0].which_thread, &thread_attr, stream_worker, &test_config[0]);
        assert(0 == code);
        // unblock the blocked worker threads
        worker_unblock();
        code = pthread_join(test_config[0].which_thread, NULL);
        assert(0 == code);
        primary_time = test_config[0].clock_ticks; 
        printf("random write time is %lu\n", primary_time);

        worker_block();
        test_config[0].write_test = backwards_write;
        code = pthread_create(&test_config[0].which_thread, &thread_attr, stream_worker, &test_config[0]);
        assert(0 == code);
        // unblock the blocked worker threads
        worker_unblock();
        code = pthread_join(test_config[0].which_thread, NULL);
        assert(0 == code);
        primary_time = test_config[0].clock_ticks; 
        printf("backwards write time is %lu\n", primary_time);

        cleanup_test_memory(test_config[0].buffer, buffer_size / pagesize, primary_file);
        primary_memory = NULL;

        return (0);
    }

    printf("using %u for primary core and %u for secondary core\n", primary_core, secondary_core);

    for (unsigned index = 0; test_data[index].test; index++) {
        cpu_set_t cpuset;
        pthread_attr_t thread_attr;

        test_config[0].which_cpu = primary_core;
        test_config[0].write_test = test_data[index].test;
        test_config[0].buffer = create_test_memory(buffer_size / pagesize, primary_file);
        test_config[0].buffer_length = buffer_size;
        test_config[0].iterations = 1; // TODO: pick an iteration count
        test_config[0].clock_ticks = 0;
        test_config[1] = test_config[0];
        test_config[1].which_cpu = secondary_core;
        test_config[1].buffer = create_test_memory(buffer_size / pagesize, secondary_file);

        // keep worker threads from running until I'm ready
        worker_block();
        pthread_attr_init(&thread_attr);
        CPU_ZERO(&cpuset);
        CPU_SET(test_config[0].which_cpu, &cpuset);
        code = pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset);
        assert(0 == code);

        code = pthread_create(&test_config[0].which_thread, &thread_attr, stream_worker, &test_config[0]);
        assert(0 == code);

        pthread_attr_init(&thread_attr);
        CPU_ZERO(&cpuset);
        CPU_SET(test_config[1].which_cpu, &cpuset);
        code = pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset);
        assert(0 == code);

        code = pthread_create(&test_config[1].which_thread, &thread_attr, stream_worker, &test_config[1]);
        assert(0 == code);

        // unblock the blocked worker threads
        worker_unblock();

        code = pthread_join(test_config[1].which_thread, NULL);
        assert(0 == code);

        code = pthread_join(test_config[0].which_thread, NULL);
        assert(0 == code);

        printf("(%s, %lu, %lu)\n", test_data[index].name, test_config[0].clock_ticks, test_config[1].clock_ticks);

        cleanup_test_memory(test_config[0].buffer, buffer_size / pagesize, primary_file);
        cleanup_test_memory(test_config[1].buffer, buffer_size / pagesize, secondary_file);
        test_config[0].buffer = NULL;
        test_config[1].buffer = NULL;

    }

    return 0;
}
