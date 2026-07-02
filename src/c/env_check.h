#ifndef NTAP_C_ENV_CHECK_H
#define NTAP_C_ENV_CHECK_H

#include <stddef.h>
#include <stdio.h>

int ntap_c_env_check(FILE *out, const char *tap_name, char *err, size_t err_len);

#endif
