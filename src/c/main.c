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
                  "  ntap-c -c <config> run\n"
                  "  ntap-c -c <config> run --direct-only --direct-addr <addr> --direct-token <token>\n"
                  "  ntap-c -c <config> run --direct-first --direct-addr <addr> --direct-token <token>\n");
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
            const char *direct_addr = NULL;
            const char *direct_token = NULL;
            bool direct_only = false;
            bool direct_first = false;
            int j = 0;
            int rc = 0;

            for (j = command_start + 1; j < argc; j++) {
                if (strcmp(argv[j], "--direct-only") == 0) {
                    direct_only = true;
                } else if (strcmp(argv[j], "--direct-first") == 0) {
                    direct_first = true;
                } else if (strcmp(argv[j], "--direct-addr") == 0) {
                    if (j + 1 >= argc) {
                        usage(stderr);
                        return 2;
                    }
                    direct_addr = argv[++j];
                } else if (strcmp(argv[j], "--direct-token") == 0) {
                    if (j + 1 >= argc) {
                        usage(stderr);
                        return 2;
                    }
                    direct_token = argv[++j];
                } else {
                    (void)fprintf(stderr, "ntap-c: unknown run option: %s\n", argv[j]);
                    usage(stderr);
                    return 2;
                }
            }
            if (direct_only && direct_first) {
                (void)fprintf(stderr,
                              "ntap-c: --direct-only and --direct-first cannot be used together\n");
                return 2;
            }
            if (direct_only || direct_first || direct_addr != NULL || direct_token != NULL) {
                if ((!direct_only && !direct_first) ||
                    direct_addr == NULL || direct_token == NULL) {
                    (void)fprintf(stderr,
                                  "ntap-c: direct mode requires --direct-addr and --direct-token\n");
                    return 2;
                }
                if (direct_only) {
                    rc = ntap_c_direct_tap_run(&cfg, direct_addr, direct_token,
                                               err, sizeof(err));
                } else {
                    char direct_err[256];

                    direct_err[0] = '\0';
                    rc = ntap_c_direct_tap_run(&cfg, direct_addr, direct_token,
                                               direct_err, sizeof(direct_err));
                    if (rc != 0) {
                        (void)fprintf(stderr,
                                      "ntap-c: direct-first failed: %s; falling back to relay\n",
                                      direct_err[0] == '\0' ? "unknown error" : direct_err);
                        err[0] = '\0';
                        rc = ntap_c_tap_run(&cfg, err, sizeof(err));
                    }
                }
            } else {
                rc = ntap_c_tap_run(&cfg, err, sizeof(err));
            }

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
