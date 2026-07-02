#include "c/config.h"
#include "c/tap_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void usage(FILE *out)
{
    (void)fprintf(out,
                  "usage:\n"
                  "  ntap-c -c <config> -t\n"
                  "  ntap-c -c <config> run\n");
}

int main(int argc, char **argv)
{
    const char *config_path = NULL;
    bool test_config = false;
    ntap_c_config_t cfg;
    char err[256];
    int i = 0;
    int command_start = 0;

    err[0] = '\0';
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                usage(stderr);
                return 2;
            }
            config_path = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            test_config = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        } else {
            command_start = i;
            break;
        }
    }

    if (ntap_c_config_load(&cfg, config_path, err, sizeof(err)) != 0) {
        (void)fprintf(stderr, "ntap-c: config error: %s\n", err);
        return 1;
    }

    if (test_config) {
        (void)printf("ntap-c: config ok (%s)\n", cfg.path);
        return 0;
    }

    if (command_start > 0) {
        if (strcmp(argv[command_start], "run") == 0) {
            int rc = ntap_c_tap_run(&cfg, err, sizeof(err));

            if (rc != 0) {
                (void)fprintf(stderr, "ntap-c: run failed: %s\n", err);
            }
            return rc;
        }
        (void)fprintf(stderr, "ntap-c: unknown command: %s\n", argv[command_start]);
        usage(stderr);
        return 2;
    }

    (void)printf("ntap-c: phase 0 skeleton ready; use -t to validate config\n");
    return 0;
}
