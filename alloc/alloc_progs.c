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

    return 0;
}
