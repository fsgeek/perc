#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include "cache.h"
#include "cpu.h"

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