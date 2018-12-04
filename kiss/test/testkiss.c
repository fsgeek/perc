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
        TEST(NULL, NULL, NULL),
    };
    static const MunitSuite suite = {
        .prefix = "/nicfs",
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
