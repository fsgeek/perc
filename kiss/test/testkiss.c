/*
 * Copyright (c) 2017, Tony Mason. All rights reserved.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include <munit/munit.h>
#include <kiss_alloc.h>

extern char *files_in_path[];

#define __notused


static MunitResult
test_one(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    return MUNIT_OK;
}

static MunitResult
test_init(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    init_kiss_allocator("/tmp/foo", 64, 1000);
    stop_kiss_allocator();

    return MUNIT_OK;
}

static MunitResult
test_alloc(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *memory;
    init_kiss_allocator("/tmp/foo", 64, 1000);

    memory = kiss_malloc(64);
    munit_assert_not_null(memory);
    stop_kiss_allocator();

    return MUNIT_OK;
}

static MunitResult
test_alloc_free(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *memory;
    init_kiss_allocator("/tmp/foo", 64, 1000);

    memory = kiss_malloc(64);
    munit_assert_not_null(memory);
    kiss_free(memory);
    memory = NULL;
    kiss_free(memory);
    stop_kiss_allocator();

    return MUNIT_OK;
}

static void thread_test_alloc_free_helper(unsigned iterations, int free)
{
    void *memory = NULL;
    void **memtable = malloc(iterations * sizeof(void *));
    unsigned index;

    munit_assert_not_null(memtable);
    memset(memtable, 0, iterations * sizeof(void *));

    for (index = 0; index < 63; index++) { 
        memory = kiss_malloc(64); // TODO: parameterize this
        munit_assert_not_null(memory);
        memtable[index] = memory;
    }

    for (; index < iterations; index++) { 
        memory = kiss_malloc(64); // TODO: parameterize this
        munit_assert_not_null(memory);
        memtable[index] = memory;
    }

    if (free) {
        for (index = 0; index < iterations; index++) {
            kiss_free(memtable[index]);
            memtable[index] = NULL;
        }
    }

    return;
}

static void *thread_test_alloc_free(void *arg)
{
    unsigned iterations = 100; // TODO: make this dynamic

    sleep(2);

    thread_test_alloc_free_helper(iterations, 1);

    return arg;

}

static void *thread_test_alloc(void *arg)
{
    unsigned iterations = 100; // TODO: make this dynamic

    sleep(2);

    thread_test_alloc_free_helper(iterations, 0);

    return arg;

}

static MunitResult
test_multi_alloc_free(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *memory;
    init_kiss_allocator("/tmp/foo", 64, 1000);

    (void)thread_test_alloc_free(NULL);

    kiss_verify();
    
    stop_kiss_allocator();

    return MUNIT_OK;
}

static MunitResult
test_alloc_free_limits(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *memory;
    unsigned iterations = 32703; // we use 65 entries
    void **memtable = malloc(iterations * sizeof(void *));

    init_kiss_allocator("/tmp/foo", 64, iterations);

    for (unsigned index = 0; index < iterations; index++) {
        memtable[index] = kiss_malloc(64);
        munit_assert_not_null(memtable[index]);
    }

    // we should have allocated everything at this point
    memory = kiss_malloc(64);
    munit_assert_null(memory);

    for (unsigned index = 0; index < iterations; index++) {
        kiss_free(memtable[index]);
        memtable[index] = NULL;
    }

    kiss_verify();

    stop_kiss_allocator();

    return MUNIT_OK;
}


static MunitResult
test_multi_alloc_free_mt(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *memory;
    unsigned threadcount = 5;
    pthread_t threads[threadcount];

    init_kiss_allocator("/tmp/foo", 64, 1000);

    for (unsigned index = 0; index < threadcount; index++) {
        munit_assert(pthread_create(&threads[index], NULL, thread_test_alloc_free, NULL) >= 0);
    }

    for (unsigned index = 0; index < threadcount; index++) {
        munit_assert(pthread_join(threads[index], NULL) >= 0);
    }

    kiss_verify();

    stop_kiss_allocator();

    return MUNIT_OK;
}

static MunitResult
test_multi_alloc_mt(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    void *memory;
    unsigned threadcount = 5;
    pthread_t threads[threadcount];

    init_kiss_allocator("/tmp/foo", 64, 1000);

    for (unsigned index = 0; index < threadcount; index++) {
        munit_assert(pthread_create(&threads[index], NULL, thread_test_alloc, NULL) >= 0);
    }

    for (unsigned index = 0; index < threadcount; index++) {
        munit_assert(pthread_join(threads[index], NULL) >= 0);
    }

    kiss_verify();

    stop_kiss_allocator();

    return MUNIT_OK;
}

#define TEST_OPEN_FILE_PARAM_DIR "dir"

static char *dirs[] = { "/mnt/pmem0", "/mnt/pmem1", NULL};

MunitParameterEnum open_params[] = 
{
    {.name = TEST_OPEN_FILE_PARAM_DIR,  .values = dirs},
    {.name = NULL, .values = NULL},
};


#define TEST(_name, _func, _params)             \
    {                                           \
        .name = (_name),                        \
        .test = (_func),                        \
        .setup = NULL,                          \
        .tear_down = NULL,                      \
        .options = MUNIT_TEST_OPTION_NONE,      \
        .parameters = (_params),                     \
    }

int
main(
    int argc,
    char **argv)
{
    static MunitTest tests[] = {
        TEST("/one", test_one, NULL),
        TEST("/init", test_init, NULL),
        TEST("/alloc", test_alloc, NULL),
        TEST("/alloc_free", test_alloc_free, NULL),
        TEST("/multi_alloc_free", test_multi_alloc_free, NULL),
        TEST("/alloc_free_limits", test_alloc_free_limits, NULL),
        TEST("/mt_alloc", test_multi_alloc_mt, NULL),
        TEST("/mt_alloc_free", test_multi_alloc_free_mt, NULL),
        TEST(NULL, NULL, NULL),
    };
    static const MunitSuite suite = {
        .prefix = "/kissalloc",
        .tests = tests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE,
    };

    return munit_suite_main(&suite, NULL, argc, argv);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
