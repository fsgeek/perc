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
#include <uuid/uuid.h>
#include <stddef.h>
#include <cpu.h>

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

#if !defined(LARGE_PAGE_SIZE)
#define LARGE_PAGE_SIZE (512 * PAGE_SIZE)
#endif

#if !defined(HUGE_PAGE_SIZE)
#define HUGE_PAGE_SIZE (512 * LARGE_PAGE_SIZE)
#endif

#if !defined(MAP_HUGE_2MB)
#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#endif

#if !defined(MAP_HUGE_1GB)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)
#endif


const char *USAGE = "\
usage:             \n\
\t%s [options]     \n\
options:           \n\
\t-d    Directory to use (should be NVM)\n\
\t-l    Log file to use (default=stderr)\n\
\t-s    Size of allocation blocks default=256)\n\
\t-h    Help (this message)\n\
";

// options information
static struct option gLongOptions[] = {
    {"directory", required_argument, NULL, 'd'},
    {"help",    no_argument, NULL, 'h'},
    {"log",     optional_argument, NULL, 'l'},
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

// note: this is an *ephemeral* structure
typedef struct alloc_block {
    struct alloc_block *next;
    struct alloc_block *prev;
    void *base_addr;
    size_t length;
    uuid_t Uuid;
    size_t unit_size;
    unsigned last_alloc; // hint
    unsigned free_blocks; // hint
    unsigned file_name_length;
    char file_name[1];
} *alloc_block_t;

// this is a persistent block
typedef struct nvm_block_header {
    uuid_t Uuid; // 16 bytes
    size_t unit_size; // 8 bytes
    uintptr_t map_address; // 8 bytes -- note this could also be embedded in the name instead

    // unused space
    char Unused[32];

    // Bitset follows.
    unsigned long long Bitset[0];
} *nvm_block_header_t;


C_ASSERT(64 == sizeof(struct nvm_block_header));

static void makepath(const char *fqp)
{
    char *p = strdup(fqp);
    char *s = NULL;
    struct stat st;
    int code;

    assert(NULL != p);
    s = strrchr(p, '/');
    assert(NULL != s);
    *s = '\0';

    if (-1 == stat(p, &st)) {
        makepath(p);
    }
    else {
        assert(S_ISDIR(st.st_mode));
        return;
    }
    code = mkdir(p, 0700);
    assert(0 == code);
}

static void close_alloc_map(alloc_block_t alloc_block)
{
    int code;

    if (NULL == alloc_block) {
        return;
    }

    assert(alloc_block->base_addr);
    errno = 0;
    code = munmap(alloc_block->base_addr, alloc_block->length);
    assert(0 == code);
    alloc_block->base_addr = 0;

    // TODO: remove this from the linked list

    free(alloc_block);
    alloc_block = NULL;
}

static void delete_alloc_map(alloc_block_t alloc_block)
{
    int code;

    if (NULL == alloc_block) {
        return;
    }

    code = unlink(alloc_block->file_name);
    assert(0 == code);

    close_alloc_map(alloc_block);

}

static unsigned find_first_available(alloc_block_t alloc_block)
{
    nvm_block_header_t block = (nvm_block_header_t) alloc_block->base_addr;
    unsigned bitset_length = alloc_block->length / (8 * sizeof(unsigned long long) * alloc_block->unit_size);
    unsigned allocated_block = ~0;

    assert(bitset_length * 64 * alloc_block->unit_size == alloc_block->length); // sanity

    // Open question: perhaps we should add a hint to the ephemeral header
    // 2MB 
    for (unsigned index = 0; index < bitset_length; index++) {
        if (0 != block->Bitset[index]) {
            unsigned long long current,update;
            
            current = update = block->Bitset[index];

            allocated_block = __builtin_ctzll(update);
            update &= ~(1<<allocated_block);
            if (current == __sync_val_compare_and_swap(&block->Bitset[index], current, update)) {
                alloc_block->free_blocks--; // update hint
                alloc_block->last_alloc = allocated_block; // update hint
                break;
            }
            // this is where we lost the race and need to search again.
            allocated_block = ~0;
       }
    }
    return allocated_block;
}


static int layout_alloc_map(alloc_block_t alloc_block)
{
    nvm_block_header_t block = (nvm_block_header_t) alloc_block->base_addr;
    unsigned bitset_length = alloc_block->length / (8 * sizeof(unsigned long long) * alloc_block->unit_size);
    uintptr_t header_end = (uintptr_t) alloc_block->base_addr;
    uintptr_t offset = 0;

    // set the fields
    uuid_copy(block->Uuid, alloc_block->Uuid);
    block->unit_size = alloc_block->unit_size;
    block->map_address = (uintptr_t) alloc_block->base_addr;

    // make sure allocation map is clear
    header_end += offsetof(struct nvm_block_header, Bitset);
    header_end += ((63 + (bitset_length * sizeof(unsigned long long))) & ~63); // round up
    memset(block->Bitset, 0, (size_t)(header_end - (uintptr_t)block->Bitset));

    cpu_clwb(block);

    // now we have to allocate the region we've used.
    while (offset < header_end) {
        // allocate a block
        offset = ((uintptr_t)block) + (alloc_block->unit_size * find_first_available(alloc_block));
    }

    cpu_sfence(); // make the changes persistent.

    return 0;
}

// Ref: https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSet64
static const unsigned char BitsSetTable256[256] = 
{
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
};

static alloc_block_t open_alloc_map(const char *nvm_path, size_t unit_size)
{
    int memfd = -1;
    char buf[4096];
    alloc_block_t ab = NULL;
    ssize_t len;
    nvm_block_header_t bh = NULL;
    uintptr_t header_end;
    int code;
    struct stat st;
    unsigned bitset_length= 0;
    uintptr_t offset = 0;
    unsigned index;

    memfd = open(nvm_path, O_RDWR);
    assert(memfd >= 0);

    len = read(memfd, buf, sizeof(buf));
    assert(len >= sizeof(struct nvm_block_header));

    bh = (nvm_block_header_t) buf;
    len = (sizeof(struct alloc_block) + strlen(nvm_path) + 8) & ~7;
    ab = (alloc_block_t) malloc(len);
    assert(NULL != ab);

    ab->next = ab;
    ab->prev = ab;
    ab->base_addr = NULL;
    ab->length = 0;
    uuid_copy(ab->Uuid, bh->Uuid);
    ab->unit_size = bh->unit_size;
    
    code = fstat(memfd, &st);
    assert(0 <= code);
    ab->length = st.st_size;
    assert(0 == (ab->length % LARGE_PAGE_SIZE));
    bitset_length = ab->length / (8 * sizeof(unsigned long long) * ab->unit_size);

    ab->base_addr = mmap((void *)bh->map_address, ab->length, PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_HUGETLB | MAP_HUGE_2MB, memfd, 0);
    if ((void *)-1 == ab->base_addr) {
        printf("warning: unable to use large pages, falling back\n");
        ab->base_addr = mmap((void *)bh->map_address, ab->length, PROT_READ | PROT_WRITE, 
                             MAP_SHARED, memfd, 0);
    }
    assert((uintptr_t)ab->base_addr == bh->map_address); // if not, need to figure out why

    // I need to compute the free block count.
    ab->free_blocks = 0;
    header_end = (uintptr_t) bh->Bitset;
    header_end += ((63 + (bitset_length * sizeof(unsigned long long))) & ~63); // round up

    offset = (uintptr_t) bh->Bitset;
    index = 0;
    while (offset < header_end) {
        unsigned setbits = 0;

        if (0 != bh->Bitset[index]) {

            setbits += BitsSetTable256[bh->Bitset[index] * 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 8) & 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 16) & 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 24) & 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 32) & 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 40) & 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 48) & 0xff];
            setbits += BitsSetTable256[(bh->Bitset[index] >> 56) & 0xff];

            ab->free_blocks += setbits;
        }
        // TODO: at this point we can seed the hint
        // Probably should switch to an index scheme rather than the specific
        // bit as it's easier to reason about.
        // ah->last_alloc = 0;
        index++;
        offset += sizeof(unsigned long long);
    }

#if 0
    header_end += offsetof(struct nvm_block_header, Bitset);
    header_end += ((63 + (bitset_length * sizeof(unsigned long long))) & ~63); // round up
    memset(block->Bitset, 0, (size_t)(header_end - (uintptr_t)block->Bitset));

    cpu_clwb(block);

    // now we have to allocate the region we've used.
    while (offset < header_end) {
        // allocate a block
        offset = ((uintptr_t)block) + (alloc_block->unit_size * find_first_available(alloc_block));
    }
#endif // 0

    return NULL;
}


static alloc_block_t create_alloc_map(const char *nvm_dir, size_t unit_size, unsigned large_page_count)
{
    int memfd = -1;
    size_t ab_length = offsetof(struct alloc_block, file_name) + strlen(nvm_dir) * 2;
    uuid_t uuid;
    alloc_block_t ab = NULL;
    char uuid_string[37];
    int code;

    uuid_generate(uuid);
    assert(!uuid_is_null(uuid));
    uuid_unparse(uuid, uuid_string);

    while (NULL == ab) {
        ab = (alloc_block_t) malloc(ab_length);
        assert(NULL != ab);
        ab->file_name_length = snprintf(ab->file_name, ab_length - offsetof(struct alloc_block, file_name), 
                                        "%s/%s/%d.nvm", nvm_dir, uuid_string, getuid());
        if (ab->file_name_length >= ab_length - offsetof(struct alloc_block, file_name)) {
            free(ab);
            ab = NULL;
            ab_length *= 2;
            continue; // try again!
        }

        ab->next = ab;
        ab->prev = ab;
        ab->base_addr = NULL;
        ab->length = 0;
        uuid_copy(ab->Uuid, uuid);
        ab->unit_size = unit_size;
    }

    // make sure directory hierarchy exists
    makepath(ab->file_name);

    // Note: this must not exist already
    errno = 0;
    memfd = open(ab->file_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    assert(-1 < memfd);
    code = posix_fallocate(memfd, 0, LARGE_PAGE_SIZE * large_page_count);
    assert(-1 < code);
    errno = 0;
    ab->base_addr = mmap(NULL, LARGE_PAGE_SIZE * large_page_count, PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_HUGETLB | MAP_HUGE_2MB, memfd, 0);
    if ((void *)-1 == ab->base_addr) {
        printf("warning: unable to use large pages, falling back\n");
        ab->base_addr = mmap(NULL, LARGE_PAGE_SIZE * large_page_count, PROT_READ | PROT_WRITE, 
                             MAP_SHARED, memfd, 0);
    }
    assert((void *)-1 != ab->base_addr);
    ab->length = LARGE_PAGE_SIZE * large_page_count;
    close(memfd);
    code = layout_alloc_map(ab);
    assert(0 == code);
    return ab;
}


int main(int argc, char **argv) 
{
    int option_char = '\0';
    char stime[32];
    const char *nvm_dir = NULL;
    alloc_block_t ab = NULL;
    size_t alloc_size = 256;

    cpu_init();

    setbuf(stdout, NULL);
    logfile = stdout;

    while (-1 != (option_char = getopt_long(argc, argv, "d:hl:", gLongOptions, NULL))) {
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
            case 'd': // work directory
                nvm_dir = strdup(optarg);
                assert(NULL != nvm_dir);
                break;
            case 's': // allocation size
                alloc_size = (size_t)atoi(optarg);
                assert((64 == alloc_size) || (256 == alloc_size) || (1024 == alloc_size));
                break;
        }
    }

    if (NULL == nvm_dir) {
        printf("must provide directory (-d)\n");
        printf(USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }

    ab = create_alloc_map(nvm_dir, alloc_size, 1);

    if (NULL != ab) {
        delete_alloc_map(ab);
    }

    return 0;
}
