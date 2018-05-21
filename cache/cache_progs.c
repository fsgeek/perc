#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>

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

    printf("Finished\n");

    return (0);
}