#define _GNU_SOURCE
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
#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>
#include <libpmem.h>
#include <uuid/uuid.h>

//
// The purpose of this program is to figure out how to find dax devices
//

static const char *get_name_for_persistence_domain(enum ndctl_persistence_domain pd) 
{
    const char *name = "Unknown";
    switch(pd) {
        default:
        case PERSISTENCE_UNKNOWN:
            break; // use default string
        case PERSISTENCE_NONE:
            name = "None";
            break;
        case PERSISTENCE_MEM_CTRL:
            name = "Memory Controller";
            break;
        case PERSISTENCE_CPU_CACHE:
            name = "Processor Cache (EADR)";
            break;
    }

    return name;
}

static const char *get_dax_location(enum ndctl_pfn_loc npl) 
{
    const char *name = "Unknown";

    switch (npl) {
        default:
            break; // use the default name unknown
        case NDCTL_PFN_LOC_NONE:
            name = "None";
            break;
        case NDCTL_PFN_LOC_PMEM:
            name = "PMEM";
            break;
        case NDCTL_PFN_LOC_RAM:
            name = "RAM";
            break;
    }

    return name;
}

int main(int argc, char **argv) 
{
    struct ndctl_ctx *ndctl_ctx = NULL;
    char devname[128];
    struct ndctl_bus *bus = NULL;
    struct ndctl_region *region = NULL;
    struct ndctl_dax *dax = NULL;
    struct dactl_region *dax_region = NULL;
    enum ndctl_persistence_domain pd;
    uuid_t dax_uuid;
    char dax_uuid_string[40];
    enum ndctl_pfn_loc npl;


    fprintf(stderr, "Starting\n");
    if (argc != 2) {
        fprintf(stderr, "Expected one argument, received %d\n", argc-1);
        exit(1);
    }
    if (strlen(argv[1]) < 1 || strlen(argv[1]) > sizeof(devname)-1) {
        fprintf(stderr, "Invalid device name\n");
        exit(1);
    }
    strcpy(devname, argv[1]);

    if (NULL == memmem(devname,strlen(devname),"/",sizeof(char))) {
        assert(strlen(argv[1]) + 5 < sizeof(devname));
        strcpy(devname, "/dev/");
        strcat(devname, argv[1]);
    }
    fprintf(stderr, "Looking for device %s\n", devname);
    assert(!ndctl_new(&ndctl_ctx));
    fprintf(stderr, "ndctl_ctx = 0x%p\n", ndctl_ctx);

    //
    // iterate
    //
    ndctl_bus_foreach(ndctl_ctx, bus) {
        assert(bus);
        fprintf(stderr, "bus %s has major %u, minor %u\n", ndctl_bus_get_devname(bus), ndctl_bus_get_major(bus), ndctl_bus_get_minor(bus));
        ndctl_region_foreach(bus, region) {
            assert(region);
            fprintf(stderr, "\tregion id %u\n",ndctl_region_get_id(region));
            fprintf(stderr, "\tdevname is %s\n",ndctl_region_get_devname(region));
            fprintf(stderr, "\tinterleave ways %u\n",ndctl_region_get_interleave_ways(region));
            fprintf(stderr, "\tmappings %u\n",ndctl_region_get_mappings(region));
            fprintf(stderr, "\tsize %zu\n",ndctl_region_get_size(region));
            fprintf(stderr, "\tavailable size %zu\n", ndctl_region_get_available_size(region));
            fprintf(stderr, "\tmax available extent %zu\n", ndctl_region_get_max_available_extent(region));
            fprintf(stderr, "\trange index %u\n",ndctl_region_get_range_index(region));
            fprintf(stderr, "\ttype %u\n",ndctl_region_get_type(region));
            fprintf(stderr, "\tnamespace seed: <todo> 0x%p\n", ndctl_region_get_namespace_seed(region));
            fprintf(stderr, "\tro %d\n",ndctl_region_get_ro(region));
            fprintf(stderr, "\tresource %lu\n",ndctl_region_get_resource);
            fprintf(stderr, "\tbtt seed: <todo> 0x%p\n",ndctl_region_get_btt_seed(region));
            fprintf(stderr, "\tpfn seed: <todo> 0x%p\n", ndctl_region_get_pfn_seed(region));
            fprintf(stderr, "\tnstype %u\n", ndctl_region_get_nstype(region));
            fprintf(stderr, "\ttype name %s\n", ndctl_region_get_type_name(region));
            // skipped ndctl_region_get_bus
            fprintf(stderr, "\tctx: <todo> 0x%p\n",ndctl_region_get_bus(region));
            // skipped ndctl_region_get_{first,next}_dimm
            fprintf(stderr, "\tnuma node %d\n",ndctl_region_get_numa_node(region));
            pd = ndctl_region_get_persistence_domain(region);
            fprintf(stderr, "\tpersistence domain %u (%s)\n",pd, get_name_for_persistence_domain(pd));
            fprintf(stderr, "\tenabled %d\n", ndctl_region_is_enabled(region));
            fprintf(stderr, "\tdax seed: <todo> 0x%p\n",ndctl_region_get_dax_seed(region));
            ndctl_dax_foreach(region, dax) {
                fprintf(stderr, "\t\tid: %u\n",ndctl_dax_get_id(dax));
                fprintf(stderr, "\t\tnamespace: <todo> 0x%p\n",ndctl_dax_get_namespace(dax));
                ndctl_dax_get_uuid(dax, dax_uuid);
                uuid_unparse(dax_uuid, dax_uuid_string);
                fprintf(stderr, "\t\tuuid %s\n", dax_uuid_string);
                fprintf(stderr, "\t\tnsize %zu\n", ndctl_dax_get_size(dax));
                fprintf(stderr, "\t\tresource %lu\n", ndctl_dax_get_resource(dax));
                npl = ndctl_dax_get_location(dax);
                fprintf(stderr, "\t\tlocation %d (%s)\n", npl, get_dax_location(npl));
                fprintf(stderr, "\t\tnum alignments %d\n", ndctl_dax_get_num_alignments(dax));
                fprintf(stderr, "\t\talign %lu\n",ndctl_dax_get_align(dax));
                // todo - unsigned long ndctl_dax_get_supported_alignment(struct ndctl_dax *dax, int i);
                // fprintf(stderr, "\t\tsupported alignment %lu\n", ndctl_dax_get_supported_alignment(dax));
                fprintf(stderr, "\t\thas align %d\n", ndctl_dax_has_align(dax));
                fprintf(stderr, "\t\tctx: <todo> 0x%p\n", ndctl_dax_get_ctx(dax));
                fprintf(stderr, "\t\tdevname %s\n", ndctl_dax_get_devname(dax));
                fprintf(stderr, "\t\tvalid %d\n", ndctl_dax_is_valid(dax));
                fprintf(stderr, "\t\tenabled %d\n", ndctl_dax_is_enabled(dax));
                fprintf(stderr, "\t\tregion: <todo> 0x%p\n", ndctl_dax_get_region(dax));
                fprintf(stderr, "\t\tconfigured %d\n", ndctl_dax_is_configured(dax));
                fprintf(stderr, "\t\tdaxctl region: <todo> 0x%p\n", ndctl_dax_get_daxctl_region(dax));
                fprintf(stderr, "\n");
            }
        }
    }

    return 0;
}