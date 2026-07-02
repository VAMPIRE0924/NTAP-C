#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include "c/env_check.h"

#include "common/proto.h"
#include "common/tap.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define NTAP_NET_CLASS_KEY \
    "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define NTAP_NET_CONNECTION_ROOT \
    "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

static char lower_ascii(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static int contains_ci(const char *haystack, const char *needle)
{
    size_t needle_len = 0;
    size_t i = 0;

    if (haystack == NULL || needle == NULL) {
        return 0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0) {
        return 1;
    }
    for (i = 0; haystack[i] != '\0'; i++) {
        size_t j = 0;

        for (j = 0; j < needle_len; j++) {
            if (haystack[i + j] == '\0' ||
                lower_ascii(haystack[i + j]) != lower_ascii(needle[j])) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int query_reg_string(HKEY key, const char *value_name,
                            char *out, size_t out_len)
{
    DWORD type = 0;
    DWORD bytes = 0;
    LONG rc = ERROR_SUCCESS;

    if (out == NULL || out_len == 0) {
        return 1;
    }
    out[0] = '\0';
    bytes = (DWORD)out_len;
    rc = RegQueryValueExA(key, value_name, NULL, &type, (LPBYTE)out, &bytes);
    if (rc != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ)) {
        out[0] = '\0';
        return 1;
    }
    out[out_len - 1u] = '\0';
    return 0;
}

static int value_looks_like_tap_windows(const char *value)
{
    return contains_ci(value, "tap");
}

static int value_looks_like_tun_only(const char *value)
{
    return contains_ci(value, "wintun") ||
           contains_ci(value, "wireguard");
}

static void query_connection_name(const char *guid, char *out, size_t out_len)
{
    char key_path[384];
    HKEY key = NULL;

    if (out == NULL || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (guid == NULL || *guid == '\0') {
        return;
    }
    (void)snprintf(key_path, sizeof(key_path), "%s\\%s\\Connection",
                   NTAP_NET_CONNECTION_ROOT, guid);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key_path, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }
    (void)query_reg_string(key, "Name", out, out_len);
    RegCloseKey(key);
}

static unsigned int enumerate_windows_tap_adapters(FILE *out, unsigned int *tun_only_count)
{
    HKEY class_key = NULL;
    DWORD index = 0;
    unsigned int count = 0;
    unsigned int tun_count = 0;
    LONG open_rc = ERROR_SUCCESS;

    open_rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, NTAP_NET_CLASS_KEY, 0,
                            KEY_READ, &class_key);
    if (open_rc != ERROR_SUCCESS) {
        (void)fprintf(out, "tap_registry_check=failed\n");
        (void)fprintf(out, "tap_registry_error=%ld\n", (long)open_rc);
        return 0;
    }

    for (;;) {
        char subkey_name[128];
        DWORD subkey_len = (DWORD)sizeof(subkey_name);
        HKEY adapter_key = NULL;
        char component_id[256];
        char matching_id[256];
        char driver_desc[256];
        char guid[128];
        char connection_name[256];
        const char *display_name = NULL;
        LONG enum_rc = ERROR_SUCCESS;

        enum_rc = RegEnumKeyExA(class_key, index, subkey_name, &subkey_len,
                                NULL, NULL, NULL, NULL);
        if (enum_rc == ERROR_NO_MORE_ITEMS) {
            break;
        }
        index++;
        if (enum_rc != ERROR_SUCCESS) {
            continue;
        }
        if (RegOpenKeyExA(class_key, subkey_name, 0, KEY_READ,
                          &adapter_key) != ERROR_SUCCESS) {
            continue;
        }

        (void)query_reg_string(adapter_key, "ComponentId",
                               component_id, sizeof(component_id));
        (void)query_reg_string(adapter_key, "MatchingDeviceId",
                               matching_id, sizeof(matching_id));
        (void)query_reg_string(adapter_key, "DriverDesc",
                               driver_desc, sizeof(driver_desc));
        (void)query_reg_string(adapter_key, "NetCfgInstanceId",
                               guid, sizeof(guid));
        if (value_looks_like_tap_windows(component_id) ||
            value_looks_like_tap_windows(matching_id) ||
            value_looks_like_tap_windows(driver_desc) ||
            value_looks_like_tun_only(component_id) ||
            value_looks_like_tun_only(matching_id) ||
            value_looks_like_tun_only(driver_desc)) {
            int supported = value_looks_like_tap_windows(component_id) ||
                            value_looks_like_tap_windows(matching_id) ||
                            value_looks_like_tap_windows(driver_desc);
            unsigned int display_index = 0;

            if (supported) {
                count++;
                display_index = count;
            } else {
                tun_count++;
                display_index = tun_count;
            }
            query_connection_name(guid, connection_name, sizeof(connection_name));
            display_name = connection_name[0] != '\0' ? connection_name : driver_desc;
            (void)fprintf(out, "%s_%u_name=%s\n",
                          supported ? "tap_windows_adapter" : "tun_only_adapter",
                          display_index,
                          display_name == NULL || *display_name == '\0' ?
                          "unknown" : display_name);
            (void)fprintf(out, "%s_%u_component_id=%s\n",
                          supported ? "tap_windows_adapter" : "tun_only_adapter",
                          display_index,
                          component_id[0] == '\0' ? "unknown" : component_id);
            (void)fprintf(out, "%s_%u_driver=%s\n",
                          supported ? "tap_windows_adapter" : "tun_only_adapter",
                          display_index,
                          driver_desc[0] == '\0' ? "unknown" : driver_desc);
            if (guid[0] != '\0') {
                (void)fprintf(out, "%s_%u_guid=%s\n",
                              supported ? "tap_windows_adapter" : "tun_only_adapter",
                              display_index, guid);
            }
        }
        RegCloseKey(adapter_key);
    }

    RegCloseKey(class_key);
    if (tun_only_count != NULL) {
        *tun_only_count = tun_count;
    }
    return count;
}

int ntap_c_env_check(FILE *out, const char *tap_name, char *err, size_t err_len)
{
    unsigned int tap_windows_count = 0;
    unsigned int tun_only_count = 0;

    if (out == NULL) {
        out = stdout;
    }
    (void)fprintf(out, "platform=windows\n");
    if (tap_name != NULL && *tap_name != '\0') {
        (void)fprintf(out, "tap_name_config=%s\n", tap_name);
    }
    tap_windows_count = enumerate_windows_tap_adapters(out, &tun_only_count);
    (void)fprintf(out, "tap_backend=tap-windows6\n");
    (void)fprintf(out, "tap_windows_adapter_count=%u\n", tap_windows_count);
    (void)fprintf(out, "tun_only_adapter_count=%u\n", tun_only_count);
    if (tap_windows_count == 0) {
        (void)fprintf(out, "tap_driver_check=missing\n");
        (void)fprintf(out, "tap_open_check=skipped\n");
        if (tun_only_count > 0) {
            (void)snprintf(err, err_len,
                           "Wintun/WireGuard adapter found, but this data-plane backend requires TAP-Windows6");
        } else {
            (void)snprintf(err, err_len,
                           "no TAP-Windows6 adapter found; install a supported Windows TAP driver");
        }
        return 1;
    }
    (void)fprintf(out, "tap_driver_check=ok\n");
    (void)fprintf(out, "tap_open_check=available_on_run\n");
    return 0;
}
#else
int ntap_c_env_check(FILE *out, const char *tap_name, char *err, size_t err_len)
{
    ntap_tap_t tap;

    if (out == NULL) {
        out = stdout;
    }
    tap.fd = -1;
    tap.name[0] = '\0';
    (void)fprintf(out, "platform=linux\n");
    if (tap_name != NULL && *tap_name != '\0') {
        (void)fprintf(out, "tap_name_config=%s\n", tap_name);
    }
    if (ntap_tap_open(&tap, "ntapcchk%d", NTAP_DEFAULT_MTU, err, err_len) != 0) {
        return 1;
    }
    (void)fprintf(out, "tun_check=ok\n");
    (void)fprintf(out, "privilege_check=ok\n");
    (void)fprintf(out, "tap_probe=%s\n", tap.name);
    ntap_tap_close(&tap);
    return 0;
}
#endif
