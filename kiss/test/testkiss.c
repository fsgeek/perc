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


#if 0
static MunitResult test_lookup_table_create(const MunitParameter params[], void *arg)
{

    (void) params;
    (void) arg;

    munit_assert(0 == nicfs_init_file_state_mgr());

    nicfs_terminate_file_state_mgr();

    return MUNIT_OK;
}

typedef struct {
    int min_fd;
    int max_fd;
    unsigned lookup_iterations;
} lt_test_params_t;

static void * ltworker(void *arg)
{
    nicfs_file_state_t *fs = NULL;
    lt_test_params_t *params = (lt_test_params_t *)arg;
    char buf[128];
    char buf2[128];

    while (NULL != params) {

        // First - let's create them
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            uuid_t uuid;

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "/test/%016u", index);
            uuid_generate(uuid);
            fs = nicfs_create_file_state(index, (void *)&uuid, buf);

            assert (NULL != fs);
        }

        // now let's read them back
        for (unsigned count = 0; count < params->lookup_iterations; count++) {
            for (int index = params->min_fd; index <= params->max_fd; index++) {

                memset(buf2, 0, sizeof(buf2));
                snprintf(buf2, sizeof(buf2), "/test/%016u", index);
                fs = nicfs_lookup_file_state(index);
                assert(NULL != fs);
                assert(fs->fd == index);
                assert(0 == strcmp(buf2, fs->pathname));        
            }
        }

        // mow let's delete them
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            if (0 == (index & 0x1)) {
                continue; // skip evens
            }

            fs = nicfs_lookup_file_state(index);
            assert(NULL != fs);
            nicfs_delete_file_state(fs);

            fs = nicfs_lookup_file_state(index);
            assert(NULL == fs);
        }

        // let's make sure we cn still find the even ones
        for (int index = params->min_fd; index <= params->max_fd; index++) {
            if (0 != (index & 0x1)) {
                continue;
            }

            fs = nicfs_lookup_file_state(index);
            assert(NULL != fs);
        }

        break;
        
    }

    // pthread_exit(NULL);
    return NULL;
}


static MunitResult test_lookup_table(const MunitParameter params[], void *arg)
{
    static const int fd_max_test = 65536;
    unsigned thread_count = 12;
    pthread_t threads[thread_count];
    pthread_attr_t thread_attr;
    int status;
    lt_test_params_t test_params[thread_count];

    (void) params;
    (void) arg;

    munit_assert(0);

    nicfs_init();

    pthread_attr_init(&thread_attr);

    memset(&threads, 0, sizeof(threads));

    for (unsigned index = 0; index < thread_count; index++) {
        if (index > 0) {
            test_params[index].min_fd = test_params[index-1].max_fd + 1;
        }
        else {
            test_params[index].min_fd = 0;
        }

        if (index < thread_count - 1) {
            test_params[index].max_fd = test_params[index].min_fd + fd_max_test / thread_count;
        }
        else {
            test_params[index].max_fd = fd_max_test;
        }
        test_params[index].lookup_iterations = 100;
        status = pthread_create(&threads[index], &thread_attr, ltworker, &test_params[index]);
        munit_assert(0 == status);
    }

    pthread_attr_destroy(&thread_attr);

    for (unsigned index = 0; index < thread_count; index++) {
        void *result;
        status = pthread_join(threads[index], &result);
        munit_assert(0 == status);
        munit_assert(NULL == result);
    }

    nicfs_shutdown();

    return MUNIT_OK;
}

#define TEST_MOUNT_PREFIX "mountprefix"
#define TEST_OPEN_FILE_PARAM_FILE "file"
#endif // 0

#define TEST_OPEN_FILE_PARAM_DIR "dir"

#if 0
#define TEST_FILE_COUNT "filecount"
#define TEST_NICCOLUM_OPTION "niccolum"

#define TEST_NICCOLUM_OPTION_TRUE "true"
#define TEST_NICCOLUM_OPTION_FALSE "false"

static int get_niccolum_option(const MunitParameter params[]) 
{
    const char *niccolum = munit_parameters_get(params, TEST_NICCOLUM_OPTION);
    int enabled = 0; // default is false

    if (NULL != niccolum) {
        if (0 == strcmp(TEST_NICCOLUM_OPTION_TRUE, niccolum)) {
            enabled = 1;
        }
    }    

    return enabled;
}

static char **test_files;

static void generate_files(const char *test_dir, const char *base_file_name, unsigned short filecount)
{
    size_t fn_length, table_length, td_length, bf_length;
    char **file_list = NULL;
    unsigned index;

    // 37 = size of a UUID - 32 digts + 4 dashes + 1 for the '\0' at the end
    td_length = strlen(test_dir);
    bf_length = strlen(base_file_name);
    fn_length = td_length + bf_length + 34 + 1;

    table_length = sizeof(char *) * (filecount + 1);
    file_list = (char **)malloc(table_length);
    munit_assert_not_null(file_list);

    for (index = 0; index < filecount; index++) {
        uuid_t test_uuid;
        char uuid_string[40];

        uuid_generate_time_safe(test_uuid);
        uuid_unparse_lower(test_uuid, uuid_string);
        munit_assert(strlen(uuid_string) == 36);
        file_list[index] = (char *)malloc(fn_length);
        munit_assert_not_null(file_list[index]);
        snprintf(file_list[index], fn_length, "%s/%s%s", test_dir, base_file_name, uuid_string);
    }
    file_list[filecount] = NULL;

    test_files = file_list;
}

static void cleanup_files(void)
{
    unsigned index;

    while (NULL != test_files) {    

        index = 0;
        while (NULL != test_files[index]) {
            free(test_files[index]);
            index++;
        }

        free(test_files);
        test_files = NULL;
    }
}

static void setup_test(const MunitParameter params[])
{
    const char *dir;
    const char *file;
    const char *filecount;
    unsigned file_count; 
    int dfd;

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    file = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_FILE);
    munit_assert_not_null(file);

    filecount = munit_parameters_get(params, TEST_FILE_COUNT);
    munit_assert_not_null(filecount);
    file_count = strtoul(filecount, NULL, 0);
    munit_assert(file_count > 0);
    munit_assert(file_count < 65536); // arbitrary

    niccolum_enabled = get_niccolum_option(params);

    dfd = open(dir, 0);
    if (dfd < 0) {
        dfd = mkdir(dir, 0775);
    }
    munit_assert_int(dfd, >=, 0);
    munit_assert_int(close(dfd), >=, 0);

    generate_files(dir, file, file_count);

}

static void cleanup_test(const MunitParameter params[])
{
    const char *dir;
    int enabled = get_niccolum_option(params);

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    cleanup_files();
    if (enabled) {
        nicfs_unlink(dir);
    }
    else {
        unlink(dir);
    }
    niccolum_enabled = 0;
}

static MunitResult
test_open_existing_files(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    int index;
    char scratch[512];


    niccolum_enabled = get_niccolum_option(params);

    nicfs_init();
    setup_test(params);

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    munit_assert_not_null(test_files);
    
    // create files
    index = 0;
    while (NULL != test_files[index]) {
        if (niccolum_enabled) {
            fd = nicfs_open(test_files[index], O_CREAT, 0664); // existing
        }
        else {
            fd = open(test_files[index], O_CREAT, 0664); // existing           
        }
        munit_assert_int(fd, >=, 0);
        if (niccolum_enabled) {
            munit_assert_int(nicfs_close(fd), >=, 0);
        }
        else {
            munit_assert_int(close(fd), >=, 0);
        }
        index++;
    }

    // now open the files
    index = 0;
    while (NULL != test_files[index]) {
        strncpy(scratch, prefix, sizeof(scratch));
        strncat(scratch, test_files[index], sizeof(scratch) - strlen(scratch));

        if (niccolum_enabled) {
          fd = nicfs_open(scratch, 0); // existing
        }
        else {
          fd = open(scratch, 0); // existing            
        }
        if (0 > fd) {
            perror("nicfs_open");
        }
        munit_assert_int(fd, >=, 0);
        if (niccolum_enabled) {
            munit_assert_int(nicfs_close(fd), >=, 0);
        }
        else {
            munit_assert_int(close(fd), >=, 0);
        }
        index++;
    }

    cleanup_test(params);

    nicfs_shutdown();

    return MUNIT_OK;
}

static MunitResult
test_open_nonexistant_files(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    const char *dir;
    const char *file;
    const char *filecount;
    char scratch[512];
    unsigned index;
    unsigned long file_count;

    nicfs_init();

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    dir = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(dir);

    file = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_FILE);
    munit_assert_not_null(file);

    filecount = munit_parameters_get(params, TEST_FILE_COUNT);
    munit_assert_not_null(filecount);
    file_count = strtoul(filecount, NULL, 0);
    munit_assert(file_count > 0);
    munit_assert(file_count < 65536); // arbitrary

    generate_files(dir, file, file_count);
    munit_assert(strlen(prefix) < 256);

    index = 0;
    while (NULL != test_files[index]) {
        strncpy(scratch, prefix, sizeof(scratch));
        strncat(scratch, test_files[index], sizeof(scratch) - strlen(scratch));

        if (niccolum_enabled) {
            fd = nicfs_open(scratch, 0);
        }
        else {
            fd = open(scratch, 0);
        }
        munit_assert_int(fd, <, 0);
        munit_assert_int(errno, ==, ENOENT);
        index++;
    }

    nicfs_shutdown();

    return MUNIT_OK;
}


static MunitResult
test_open_dir(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    int fd;
    const char *prefix;
    const char *pathname;
    char scratch[512];

    nicfs_init();

    setup_test(params);

    prefix = munit_parameters_get(params, TEST_MOUNT_PREFIX);
    munit_assert_not_null(prefix);

    pathname = munit_parameters_get(params, TEST_OPEN_FILE_PARAM_DIR);
    munit_assert_not_null(pathname);

    strncpy(scratch, prefix, sizeof(scratch));
    strncat(scratch, pathname, sizeof(scratch) - strlen(scratch));

    if (niccolum_enabled) {
        fd = nicfs_open(scratch, 0);
    }
    else {
        fd = open(scratch, 0);
    }
    munit_assert_int(fd, >=, 0);

    if (niccolum_enabled) {
        munit_assert_int(nicfs_close(fd), >=, 0);
    }
    else {
        munit_assert_int(close(fd), >=, 0);
    }
    nicfs_shutdown();

    return MUNIT_OK;
}

static char *ld_library_path[] = {
"/usr/lib/x86_64-linux-gnu/libfakeroot",
"/usr/lib/i686-linux-gnu",
"/lib/i386-linux-gnu",
"/usr/lib/i686-linux-gnu",
"/usr/lib/i386-linux-gnu/mesa",
"/usr/local/lib",
"/lib/x86_64-linux-gnu",
"/usr/lib/x86_64-linux-gnu",
"/usr/lib/x86_64-linux-gnu/mesa-egl",
"/usr/lib/x86_64-linux-gnu/mesa",
"/lib32",
"/usr/lib32",
"/libx32",
"/usr/libx32",
};


//
// given a list of files and paths, find the first occurrence of the file in the path
//
static int find_file_in_path(char **files, char **paths, char *prefix, char **file_found, char **path_found) 
{
    unsigned file_index, path_index;
    struct stat stat_buf;
    char scratch[4096]; // TODO: make this handle arbitrary long paths

    for (file_index = 0; NULL != files[file_index]; file_index++) {
        for (path_index = 0; NULL != paths[path_index]; path_index++) {
            /* this is done using the stat call */
            if (NULL != prefix) {
                sprintf(scratch, "%s/%s/%s", prefix, paths[path_index], files[file_index]);
            }
            else {
                sprintf(scratch, "%s/%s", paths[path_index], files[file_index]);
            }

            if (0 == stat(scratch, &stat_buf)) {
                // found it
                // TODO: may want to make this more resilient against "wrong kind of content" errors
                *file_found = files[file_index];
                *path_found = paths[path_index];
                return 0;
            }
        }
    }

    return -1;
}

static char **get_env_path(void)
{
    const char *path = getenv("PATH");
    size_t path_len;
    char *path_copy = NULL;
    unsigned index;
    unsigned index2;
    unsigned count;
    char **paths = NULL;
    char *cwd = NULL;

    munit_assert_not_null(path);

    path_len = strlen(path);
    path_copy = malloc(path_len + pathconf(".", _PC_PATH_MAX) + 2);
    munit_assert_not_null(path_copy);
    strcpy(path_copy, path);
    munit_assert(':' != path_copy[0]); // don't handle this case now
    cwd = getcwd(&path_copy[path_len+2], pathconf(".", _PC_PATH_MAX + 1));
    munit_assert_not_null(cwd);

    count = 0;
    // skip char 0 because if it is a colon then there's nothing in front of it anyway
    for (index = 0; index < path_len; index++) {
        if (':' == path_copy[index]) {
            count++;
        }
    }

    index2 = 0;
    paths = (char **)malloc(sizeof(char *) * (count + 1));
    paths[0] = path_copy;
    for (index = 0; index < path_len; index++) {
        if (':' == path_copy[index]) {
            path_copy[index] = '\0';
            munit_assert(':' != path_copy[index+1]); // don't handle this case
            if (0 == strcmp(paths[index2], ".")) {
                paths[++index2] = cwd;
            }
            else {
                paths[++index2] = &path_copy[index+1];
            }
        }
    }
    munit_assert(index2 == count);
    paths[index2] = NULL;

    return paths;    
}

static MunitResult
lib_search_internal(char *prefix)
{
    // char **paths = get_env_path();
    char *file_found, *path_found;
    char *files_to_find[2];
    unsigned found, not_found;
    unsigned index;

    munit_assert_not_null(ld_library_path);

    /* now let's search it for a list of things */

    found = 0;
    not_found = 0;
    for (index = 0; NULL != libs_in_path[index]; index++) {
        files_to_find[0] = libs_in_path[index];
        files_to_find[1] = NULL;

        if (0 == find_file_in_path(files_to_find, ld_library_path, prefix, &file_found, &path_found)) {
            found++;
        }
        else {
            not_found++;
        }
    }

    munit_assert(found + not_found == index);

    return MUNIT_OK;
}

static MunitResult
path_search_internal(char *prefix)
{
    char **paths = get_env_path();
    char *file_found, *path_found;
    char *files_to_find[2];
    unsigned found, not_found;
    unsigned index;

    munit_assert_not_null(paths);

    /* now let's search it for a list of things */

    found = 0;
    not_found = 0;
    for (index = 0; NULL != files_in_path[index]; index++) {
        files_to_find[0] = files_in_path[index];
        files_to_find[1] = NULL;

        if (0 == find_file_in_path(files_to_find, paths, prefix, &file_found, &path_found)) {
            found++;
        }
        else {
            not_found++;
        }
    }

    munit_assert(found + not_found == index);

    return MUNIT_OK;
}

static MunitResult
test_path_search_native(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index; 

    // munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == path_search_internal(NULL));
    }
    return MUNIT_OK;
}

static MunitResult
test_path_search_pt(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index; 

    // munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == path_search_internal("/mnt/pt"));
    }
    return MUNIT_OK;
}

static MunitResult
test_library_search_native(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index;

    // munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == lib_search_internal(NULL));
    }
    return MUNIT_OK;
}

static MunitResult
test_library_search_pt(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index;


    // munit_assert(0);

    for (index = 0; index < 100; index++) {
        munit_assert(MUNIT_OK == lib_search_internal("/mnt/pt"));
    }
    return MUNIT_OK;
}

static MunitResult
old_test_niccolum_search(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    unsigned index;
    unsigned index2;

    nicfs_init();

    for (index = 0; index < 100; index++) {
        for (index2 = 0; NULL != files_in_path[index2]; index2++) {
            munit_assert(MUNIT_OK == nicfs_test_server());
        }
    }

    nicfs_shutdown();

    return MUNIT_OK;
}

int niccolum_pack_strings(char **strings, void **data, size_t *data_length);
int niccolum_unpack_strings(void *data, size_t max_length, char ***strings);

static MunitResult
test_niccolum_search(
    const MunitParameter params[] __notused,
    void *prv __notused)
{
    size_t packed_file_length;
    size_t packed_path_length;
    void *packed_files;
    void *packed_paths;
    char **unpacked_files = NULL;
    char **unpacked_paths = NULL;

    char *files[] = {
        "file1",
        "file2",
        "file3",
        "file4",
        "file5",
        NULL,
    };

    char *paths[] = {
        "/tmp",
        "/bin",
        "/usr/bin",
        "/sbin",
        NULL,
    };

    munit_assert(0 == niccolum_pack_strings(files, &packed_files, &packed_file_length));
    munit_assert_not_null(packed_files);
    munit_assert(0 == niccolum_pack_strings(paths, &packed_paths, &packed_path_length));
    munit_assert_not_null(packed_paths);

    munit_assert(0 == niccolum_unpack_strings(packed_files, packed_file_length, &unpacked_files));
    munit_assert(0 == niccolum_unpack_strings(packed_paths, packed_path_length, &unpacked_paths));

    munit_assert_not_null(unpacked_files);
    munit_assert_not_null(unpacked_paths);

    for (unsigned index = 0; NULL != unpacked_files[index]; index++) {
        size_t flen = strlen(files[index]);
        size_t uflen = strlen(unpacked_files[index]);

        munit_assert(flen == uflen);
        munit_assert(0 == memcmp(files[index], unpacked_files[index], strlen(files[index])));
    }

    for (unsigned index = 0; NULL != unpacked_paths[index]; index++) {       
        munit_assert(strlen(paths[index]) == strlen(unpacked_paths[index]));
        munit_assert(0 == memcmp(paths[index], unpacked_paths[index], strlen(paths[index])));
    }


    free(unpacked_files);
    free(unpacked_paths);
    free(packed_files);
    free(packed_paths);

    return MUNIT_OK;

    old_test_niccolum_search(params, prv);
}
#endif // 0

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
