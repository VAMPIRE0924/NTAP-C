#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "c/tap_client.h"

#include "common/net.h"
#include "common/tap.h"
#include "common/wire.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int ntap_c_tap_run(const ntap_c_config_t *cfg, char *err, size_t err_len)
{
    (void)cfg;
    (void)snprintf(err, err_len, "TAP mode is not supported on Windows in current phase");
    return 1;
}
#else
#include <errno.h>
#include <sys/select.h>

static int recv_expected(ntap_socket_t fd, uint8_t expected_type, ntap_hdr_t *hdr,
                         uint8_t *payload, size_t payload_cap, size_t *payload_len,
                         char *err, size_t err_len)
{
    if (ntap_recv_msg(fd, hdr, payload, payload_cap, payload_len, err, err_len) != 0) {
        return -1;
    }
    if (hdr->type != expected_type) {
        (void)snprintf(err, err_len, "expected %s, got %s",
                       ntap_msg_type_name(expected_type), ntap_msg_type_name(hdr->type));
        return -1;
    }
    return 0;
}

static int send_tap_payload(ntap_socket_t fd, uint32_t session_id, uint32_t network_id,
                            const uint8_t *frame, uint16_t frame_len,
                            char *err, size_t err_len)
{
    uint8_t payload[NTAP_MAX_MTU + NTAP_TAP_PAYLOAD_EXTRA];
    size_t payload_len = 0;

    if (ntap_encode_tap_frame(payload, sizeof(payload), &payload_len,
                              network_id, frame, frame_len) != 0) {
        (void)snprintf(err, err_len, "failed to encode TAP_FRAME");
        return -1;
    }
    return ntap_send_msg(fd, NTAP_MSG_TAP_FRAME, session_id, payload,
                         (uint32_t)payload_len, err, err_len);
}

static int run_tap_loop(ntap_socket_t fd, const char *tap_name, uint16_t mtu,
                        const ntap_auth_ok_t *auth_ok, char *err, size_t err_len)
{
    ntap_tap_t tap;
    uint8_t payload[NTAP_PAYLOAD_MAX_CONTROL];
    uint8_t frame_buf[NTAP_MAX_MTU + 64u];
    ntap_hdr_t hdr;
    size_t payload_len = 0;

    tap.fd = -1;
    tap.name[0] = '\0';
    if (ntap_tap_open(&tap, tap_name, mtu, err, err_len) != 0) {
        return -1;
    }
    (void)printf("ntap-c: TAP opened name=%s mtu=%u\n", tap.name, mtu);
    (void)fflush(stdout);

    if (ntap_send_msg(fd, NTAP_MSG_PING, auth_ok->session_id, NULL, 0,
                      err, err_len) != 0) {
        ntap_tap_close(&tap);
        return -1;
    }

    for (;;) {
        fd_set readfds;
        int maxfd = fd > tap.fd ? fd : tap.fd;
        int selected = 0;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        FD_SET(tap.fd, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        selected = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (selected < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)snprintf(err, err_len, "select failed: %s", strerror(errno));
            ntap_tap_close(&tap);
            return -1;
        }
        if (selected == 0) {
            if (ntap_send_msg(fd, NTAP_MSG_PING, auth_ok->session_id, NULL, 0,
                              err, err_len) != 0) {
                ntap_tap_close(&tap);
                return -1;
            }
            continue;
        }
        if (FD_ISSET(tap.fd, &readfds)) {
            size_t frame_len = 0;

            if (ntap_tap_read(&tap, frame_buf, sizeof(frame_buf), &frame_len,
                              err, err_len) != 0) {
                ntap_tap_close(&tap);
                return -1;
            }
            if (frame_len >= 14u && frame_len <= (size_t)mtu + 18u) {
                if (send_tap_payload(fd, auth_ok->session_id, auth_ok->network_id,
                                     frame_buf, (uint16_t)frame_len,
                                     err, err_len) != 0) {
                    ntap_tap_close(&tap);
                    return -1;
                }
                (void)printf("ntap-c: TAP_FRAME sent len=%zu\n", frame_len);
                (void)fflush(stdout);
            }
        }
        if (FD_ISSET(fd, &readfds)) {
            if (ntap_recv_msg(fd, &hdr, payload, sizeof(payload), &payload_len,
                              err, err_len) != 0) {
                ntap_tap_close(&tap);
                return -1;
            }
            if (hdr.type == NTAP_MSG_PONG) {
                continue;
            }
            if (hdr.type == NTAP_MSG_TAP_FRAME) {
                ntap_tap_frame_t tap_frame;

                if (ntap_decode_tap_frame(&tap_frame, payload, payload_len) != 0 ||
                    tap_frame.network_id != auth_ok->network_id) {
                    (void)snprintf(err, err_len, "invalid relayed TAP_FRAME");
                    ntap_tap_close(&tap);
                    return -1;
                }
                if (ntap_tap_write(&tap, tap_frame.frame, tap_frame.frame_len,
                                   err, err_len) != 0) {
                    ntap_tap_close(&tap);
                    return -1;
                }
                (void)printf("ntap-c: TAP_FRAME received len=%u\n", tap_frame.frame_len);
                (void)fflush(stdout);
                continue;
            }
            (void)snprintf(err, err_len, "unexpected message in tap mode: %s",
                           ntap_msg_type_name(hdr.type));
            ntap_tap_close(&tap);
            return -1;
        }
    }
}

int ntap_c_tap_run(const ntap_c_config_t *cfg, char *err, size_t err_len)
{
    ntap_socket_t fd = NTAP_INVALID_SOCKET;
    uint8_t payload[NTAP_PAYLOAD_MAX_CONTROL];
    uint8_t out_payload[NTAP_PAYLOAD_MAX_CONTROL];
    size_t payload_len = 0;
    size_t out_len = 0;
    uint8_t client_nonce[NTAP_NONCE_SIZE];
    ntap_hdr_t hdr;
    ntap_hello_t server_hello;
    ntap_auth_ok_t auth_ok;
    ntap_config_push_t runtime_config;
    const char *tap_name = NULL;
    uint16_t tap_mtu = NTAP_DEFAULT_MTU;
    int rc = 1;

    if (cfg == NULL) {
        return 1;
    }
    if (ntap_net_init(err, err_len) != 0 ||
        ntap_tcp_connect(cfg->server_addr, &fd, err, err_len) != 0) {
        ntap_net_cleanup();
        return 1;
    }
    if (ntap_random_nonce(client_nonce) != 0 ||
        ntap_encode_hello(out_payload, sizeof(out_payload), &out_len,
                          NTAP_ROLE_TAP_CLIENT, "ntap-c/0.1", 0,
                          client_nonce) != 0 ||
        ntap_send_msg(fd, NTAP_MSG_HELLO, 0, out_payload, (uint32_t)out_len,
                      err, err_len) != 0) {
        goto done;
    }
    if (recv_expected(fd, NTAP_MSG_HELLO, &hdr, payload, sizeof(payload),
                      &payload_len, err, err_len) != 0 ||
        ntap_decode_hello(&server_hello, payload, payload_len) != 0) {
        goto done;
    }
    if (server_hello.role != NTAP_ROLE_SERVER) {
        (void)snprintf(err, err_len, "server HELLO role mismatch");
        goto done;
    }
    if (ntap_encode_auth_tap(out_payload, sizeof(out_payload), &out_len,
                             cfg->username, cfg->password, cfg->network_id,
                             client_nonce) != 0 ||
        ntap_send_msg(fd, NTAP_MSG_AUTH_TAP, 0, out_payload, (uint32_t)out_len,
                      err, err_len) != 0) {
        goto done;
    }
    if (ntap_recv_msg(fd, &hdr, payload, sizeof(payload), &payload_len,
                      err, err_len) != 0) {
        goto done;
    }
    if (hdr.type == NTAP_MSG_AUTH_FAIL) {
        (void)snprintf(err, err_len, "auth failed");
        rc = 3;
        goto done;
    }
    if (hdr.type != NTAP_MSG_AUTH_OK ||
        ntap_decode_auth_ok(&auth_ok, payload, payload_len) != 0) {
        (void)snprintf(err, err_len, "invalid auth response");
        goto done;
    }
    if (recv_expected(fd, NTAP_MSG_CONFIG_PUSH, &hdr, payload, sizeof(payload),
                      &payload_len, err, err_len) != 0 ||
        ntap_decode_config_push(&runtime_config, payload, payload_len) != 0 ||
        runtime_config.network_id != auth_ok.network_id ||
        !runtime_config.tap_enabled) {
        (void)snprintf(err, err_len, "invalid CONFIG_PUSH");
        goto done;
    }
    tap_name = runtime_config.tap_name[0] == '\0' ? cfg->tap_name : runtime_config.tap_name;
    tap_mtu = runtime_config.mtu;
    (void)printf("ntap-c: auth ok session_id=%u network_id=%u\n",
                 auth_ok.session_id, auth_ok.network_id);
    (void)fflush(stdout);
    rc = run_tap_loop(fd, tap_name, tap_mtu, &auth_ok, err, err_len);

done:
    ntap_socket_close(fd);
    ntap_net_cleanup();
    return rc;
}
#endif
