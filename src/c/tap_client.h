#ifndef NTAP_C_TAP_CLIENT_H
#define NTAP_C_TAP_CLIENT_H

#include <stddef.h>

#include "c/config.h"

int ntap_c_tap_run(const ntap_c_config_t *cfg, char *err, size_t err_len);
int ntap_c_direct_tap_run(const ntap_c_config_t *cfg, const char *direct_addr,
                          const char *direct_token, char *err, size_t err_len);

#endif
