#if !defined(__KISS_ALLOC_H__)
#define __KISS_ALLOC_H__ (1)

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

void init_kiss_allocator(const char *nvm_dir, size_t unit_size, size_t object_count);
void stop_kiss_allocator(void);
void *kiss_malloc(size_t size);
void *kiss_preserve(size_t size);
void *kiss_pcommit(void *address);
void kiss_free(void *address);

#endif // __KISS_ALLOC_H__