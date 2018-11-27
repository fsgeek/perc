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
\t-p    Starting processor number for primary test group [default=0]\n\
\t-s    Starting processor number for secondary test [default=0]\n\
\t-i    Number of test iterations [default=1]\n\
\t-a    Number of instances to run primary test [default=1]\n\
\t-b    Number of instances to run secondary test [default=1]\n\
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
    {"primary-threads", optional_argument, NULL, 'a'},
    {"secondary-threads", optional_argument, NULL, 'b'},
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

typedef struct mixed_write_parameters {
    void *memory1;
    void *memory2;
    size_t length;
    unsigned long long iterations;
    unsigned __int64 m1time;
    unsigned __int64 m2time;
} *mixed_write_parameters_t;

// mixed_sequential_write: single thread mixing access to two memory regions
static void mixed_sequential_write(mixed_write_parameters_t parameters)
{
    assert(0); // not implemented yet
    return;
}

// mixed_random_write: single thread mixing access to two memory regions
static void mixed_random_write(mixed_write_parameters_t parameters)
{
    size_t offset;
    unsigned long long retries = 0;
    __int64 *source;
    __int64 *m1_sink, *m2_sink;
    unsigned __int64 start, end, m1total,m2total;
    unsigned __int64 random_data;
    size_t bytes_copied;

    assert(NULL != parameters);
    parameters->m1time = parameters->m2time = 0;
    for (unsigned long long index = 0; index < parameters->iterations; index++) {

        bytes_copied = 0;
        while (bytes_copied < parameters->length) {
            retries = 0;
            while(0 == _rdrand64_step((unsigned __int64 *)&offset)) {
                retries++;
                /* try again */
                assert(retries < 100);
            }
            offset %= (parameters->length-128);

            while (0 == _rdrand64_step((unsigned __int64 *)&random_data)) {
                assert(0);
            }

            m1_sink = ((__int64 *)parameters->memory1) + (offset / sizeof(__int64));
            m2_sink = ((__int64 *)parameters->memory2) + (offset / sizeof(__int64));

            for (unsigned l = 0; l < 16; l++) {
                start = __rdtsc();
                _mm_stream_si64(m1_sink++, random_data); // 1
                parameters->m1time += __rdtsc() - start;
                start = __rdtsc();
                _mm_stream_si64(m2_sink++, random_data); 
                parameters->m1time += __rdtsc() - start;               
            }
            //_mm_mfence();
            bytes_copied += 128;
        }
    }

}
typedef unsigned __int64 (*write_test_t)(void *destination, size_t length, unsigned long long iterations);

typedef struct {
    unsigned            which_cpu;
    pthread_t           which_thread;
    write_test_t        write_test;
    // void               *buffer;
    // size_t              buffer_length;
    size_t              pagecount;
    unsigned long long  iterations;
    unsigned long long  clock_ticks; // filled upon completion
    char *              file_name;
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

static const char *test_buf_name_string = "%s-%u.dat";

static void *create_test_buffer(const unsigned pagecount, const char *file_name, unsigned index)
{
    char namebuf[128];

    if (NULL == file_name) {
        return create_test_memory(pagecount, NULL);        
    }

    snprintf(namebuf, sizeof(namebuf), test_buf_name_string, file_name, index);
    assert(strlen(namebuf) < sizeof(namebuf) - 1);
    return create_test_memory(pagecount, namebuf);
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

static void cleanup_test_buffer(void *memory, const unsigned pagecount, const char *file_name, unsigned index)
{
    char namebuf[128];

    snprintf(namebuf, sizeof(namebuf), test_buf_name_string, file_name, index);
    assert(strlen(namebuf) < sizeof(namebuf) - 1);
    return  cleanup_test_memory(memory, pagecount, namebuf);
}

void *stream_worker(void *context)
{
    test_config_t *config = (test_config_t *)context;
    void *buffer = NULL;
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    size_t buffer_length;

    // sanity checks
    assert(config->write_test);
    assert(config->pagecount > 0);
    assert(config->iterations > 0);
    assert(config->which_cpu == sched_getcpu());

    buffer = create_test_buffer(config->pagecount, config->file_name, config->which_cpu);
    assert(NULL != buffer);
    buffer_length = pagesize * config->pagecount;

    // thread blocks and waits for the ready signal
    worker_wait();

    config->clock_ticks = config->write_test(buffer, buffer_length, config->iterations);

    if (NULL != buffer) {
        cleanup_test_buffer(buffer, config->pagecount, config->file_name, config->which_cpu);
    }

    pthread_exit(config);    
}


const char *gettimestamp(char *buffer, size_t buffer_length) 
{
    time_t ltime = time(NULL);
    struct tm timeptr;
    static char wday_name[7][3] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char mon_name[12][3] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    localtime_r(&ltime, &timeptr);

    snprintf(buffer, buffer_length, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
        wday_name[timeptr.tm_wday],
        mon_name[timeptr.tm_mon],
        timeptr.tm_mday, timeptr.tm_hour,
        timeptr.tm_min, timeptr.tm_sec,
        1900 + timeptr.tm_year);
    return buffer;
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
    size_t buffer_size = 1024 * 1024 * 1024; // 1GB
    static unsigned __int64 primary_time;
    unsigned long long iterations = 1; // low number for testing purposes
    int test_mode = 0;
    int code;
    struct {
        const char *name;
        write_test_t test;
    } test_data[] = {
        {"sequential_write", sequential_write},
        {"random_write", random_write},
        {"backwards_write", backwards_write},
        {NULL, NULL}
    };
    test_config_t *full_test_config = NULL;
    pthread_attr_t thread_attr;
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    unsigned current_cpu, current_node;
    unsigned primary_thread_count=1, secondary_thread_count=1;
    size_t pagecount = buffer_size / pagesize;
    time_t ltime;
    struct tm result;
    char stime[32];

    setbuf(stdout, NULL);
    logfile = stdout;

    while (-1 != (option_char = getopt_long(argc, argv, "tf:n:hl:p:s:i:a:b:", gLongOptions, NULL))) {
        switch(option_char) {
            default:
                printf( "Unknown option -%c\n", option_char);
            case 'h': // help
                printf(USAGE, argv[0]);
                exit(EXIT_FAILURE);
                break;
            case 'l': // logfile
                logfile = fopen(optarg, "a+");
                if (NULL == logfile) {
                    printf("could not open %s, fallback to stdout\n", optarg);
                    logfile = stdout;
                }
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
            case 'a': // primary thread count
                primary_thread_count = (unsigned)atoi(optarg);
                break;
            case 'b': // secondary thread count
                secondary_thread_count = (unsigned)atoi(optarg);
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
        test_config_t test_config[2];
        char *fname = primary_file;

        if (NULL == fname) {
            fname = "DRAM";
        }


        test_config[0].which_cpu = primary_core;
        test_config[0].pagecount = pagecount;
        test_config[0].iterations = iterations;
        test_config[0].clock_ticks = 0;
        test_config[0].file_name = primary_file;

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
        fprintf(logfile, "%s, sequential_write, %s, t, %u, %lu\n", gettimestamp(stime, sizeof(result)),
            fname,
            test_config[0].which_cpu, test_config[0].clock_ticks);

        worker_block();
        test_config[0].write_test = random_write;
        code = pthread_create(&test_config[0].which_thread, &thread_attr, stream_worker, &test_config[0]);
        assert(0 == code);
        // unblock the blocked worker threads
        worker_unblock();
        code = pthread_join(test_config[0].which_thread, NULL);
        assert(0 == code);
        primary_time = test_config[0].clock_ticks; 
        fprintf(logfile, "%s, random_write, %s, t, %u, %lu\n", gettimestamp(stime, sizeof(result)), 
            fname, test_config[0].which_cpu, test_config[0].clock_ticks);

        worker_block();
        test_config[0].write_test = backwards_write;
        code = pthread_create(&test_config[0].which_thread, &thread_attr, stream_worker, &test_config[0]);
        assert(0 == code);
        // unblock the blocked worker threads
        worker_unblock();
        code = pthread_join(test_config[0].which_thread, NULL);
        assert(0 == code);
        ltime = time(NULL);
        gettimestamp(stime, sizeof(result));
        fprintf(logfile, "%s, backwards_write, %s, t, %u, %lu\n", gettimestamp(stime, sizeof(result)), 
            fname, test_config[0].which_cpu, test_config[0].clock_ticks);

        return (0);
    }

    printf("using %u for primary core and %u for secondary core\n", primary_core, secondary_core);

    full_test_config = (test_config_t *)malloc(sizeof(test_config_t) * (primary_thread_count + secondary_thread_count + 1));
    assert(NULL != full_test_config);

    for (unsigned test = 0; test_data[test].test; test++) {
        unsigned index;

        
        for (index = 0; index < primary_thread_count; index++) {
            full_test_config[index].which_cpu = primary_core + index;
            full_test_config[index].write_test = test_data[test].test;
            full_test_config[index].pagecount = pagecount;
            full_test_config[index].iterations = iterations;
            full_test_config[index].clock_ticks = 0;
            full_test_config[index].file_name = primary_file;
        }

        for (; index < primary_thread_count+secondary_thread_count; index++) {
            full_test_config[index].which_cpu = secondary_core + index - primary_thread_count;
            full_test_config[index].write_test = test_data[test].test;
            full_test_config[index].pagecount = pagecount;
            full_test_config[index].iterations = iterations;
            full_test_config[index].clock_ticks = 0;
            full_test_config[index].file_name = secondary_file;
        }
        memset(&full_test_config[index], 0, sizeof(test_config_t)); // zero final entry

        worker_block();
        // spin up some threads
        for (index = 0; index < primary_thread_count + secondary_thread_count; index++) {
            pthread_attr_init(&thread_attr);
            CPU_ZERO(&cpuset);
            CPU_SET(full_test_config[index].which_cpu, &cpuset);
            code = pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset);
            assert(0 == code);
            code = pthread_create(&full_test_config[index].which_thread, &thread_attr, stream_worker, &full_test_config[index]);
            assert(0 == code);
        }

        // unblock the blocked worker threads
        worker_unblock();

        // wait for them all to finish
        for (index = 0; index < primary_thread_count + secondary_thread_count; index++) {
            code = pthread_join(full_test_config[index].which_thread, NULL);
            assert(0 == code);
        }

        // gather the results
        for (index = 0; index < primary_thread_count + secondary_thread_count; index++) {
            char *fname = index < primary_thread_count ? primary_file : secondary_file;

            if (NULL == fname) {
                fname = "DRAM";
            }
            fprintf(logfile, "%s, %s, %s, %c, %u, %lu\n", gettimestamp(stime, sizeof(result)), 
                test_data[test].name, 
                fname,
                index < primary_thread_count ? 'p' : 's',
                full_test_config[index].which_cpu, full_test_config[index].clock_ticks);            
        }

    }

    free(full_test_config);
    full_test_config = NULL;


#if 0
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
#endif // 0

    return 0;
}
