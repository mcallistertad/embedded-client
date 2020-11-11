#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "unittest.h"

static const char *optargs = "vh";

static struct option longopts[] = {
    { "help",       no_argument, NULL, 'h' },
    { "verbose",    no_argument, NULL, 'v' }
};

static int usage(char **argv) {
    fprintf(stderr,
        "Usage: %s [args]\n\n"
        "  -v\tDisplay all test results\n"
        "  -h\tDisplay this message\n",
        argv[0]);

    return -1;
}

int main(int argc, char **argv) {
    Test_opts opts;
    for (;;) {
        int c = getopt_long(argc, argv, optargs, longopts, NULL);
        if (c == -1) {
            if (optind != argc)
                return usage(argv);
            break;
        }

        switch (c) {
            case 'v':
                opts.verbose = 1;
                break;
            case 'h':
            default:
                return usage(argv);
        }
    }

    Test_rs rs = { 0, 0 };
    RUN_TEST(beacon_test);
    RUN_TEST(ap_plugin_vap_used);
    RUN_TEST(test_utilities);
    RUN_TEST(plugin_test);

    if (opts.verbose || rs.failed)
        fprintf(stdout, "%d tests run, %d failed\n", rs.ran, rs.failed);

    return rs.failed;
}

