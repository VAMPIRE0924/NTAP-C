#include "c/config.h"

#include "common/proto.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_optional(char *out, size_t out_len, const char *value)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    if (value == NULL) {
        out[0] = '\0';
        return;
    }
    (void)snprintf(out, out_len, "%s", value);
}

static int parse_optional_u32(const ntap_config_t *cfg, const char *section,
                              const char *key, uint32_t fallback,
                              uint32_t *out, char *err, size_t err_len)
{
    const char *value = ntap_config_get(cfg, section, key);
    char *end = NULL;
    unsigned long parsed = 0;

    if (out == NULL) {
        return -1;
    }
    if (value == NULL || *value == '\0') {
        *out = fallback;
        return 0;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > 0xfffffffful) {
        (void)snprintf(err, err_len, "invalid integer in %s.%s", section, key);
        return -1;
    }
    *out = (uint32_t)parsed;
    return 0;
}

int ntap_c_config_load(ntap_c_config_t *out, const char *path, char *err, size_t err_len)
{
    ntap_config_t cfg;

    if (out == NULL) {
        return -1;
    }
    (void)memset(out, 0, sizeof(*out));
    if (path == NULL || *path == '\0') {
        path = "ntap-c.conf";
    }
    (void)snprintf(out->path, sizeof(out->path), "%s", path);

    if (ntap_config_load(&cfg, path, err, err_len) != 0) {
        return -1;
    }
    if (ntap_config_require_addr(&cfg, "client", "server_addr",
                                 out->server_addr, sizeof(out->server_addr), err, err_len) != 0 ||
        ntap_config_require(&cfg, "client", "username",
                            out->username, sizeof(out->username), err, err_len) != 0 ||
        ntap_config_require(&cfg, "client", "tap_name",
                            out->tap_name, sizeof(out->tap_name), err, err_len) != 0) {
        return -1;
    }

    copy_optional(out->password, sizeof(out->password),
                  ntap_config_get(&cfg, "client", "password"));
    if (parse_optional_u32(&cfg, "client", "network_id", 0, &out->network_id,
                           err, err_len) != 0 ||
        ntap_config_get_u16(&cfg, "client", "mtu", NTAP_DEFAULT_MTU,
                            NTAP_MIN_MTU, NTAP_MAX_MTU, &out->mtu,
                            err, err_len) != 0) {
        return -1;
    }
    copy_optional(out->log_level, sizeof(out->log_level),
                  ntap_config_get(&cfg, "log", "level"));
    copy_optional(out->log_file, sizeof(out->log_file),
                  ntap_config_get(&cfg, "log", "file"));
    if (out->log_level[0] == '\0') {
        (void)snprintf(out->log_level, sizeof(out->log_level), "info");
    }
    return 0;
}
