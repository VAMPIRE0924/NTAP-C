#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#include "c/config.h"

#include <windows.h>
#include <shellapi.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define NTAP_GUI_CLASS L"NTAPCClientWindow"
#define NTAP_GUI_TITLE L"NTAP-C 客户端"
#define NTAP_LOG_MESSAGE (WM_APP + 1)
#define NTAP_STATUS_MESSAGE (WM_APP + 2)
#define NTAP_DONE_MESSAGE (WM_APP + 3)

#define IDC_SERVER 1001
#define IDC_USERNAME 1002
#define IDC_PASSWORD 1003
#define IDC_NETWORK 1004
#define IDC_TAPNAME 1005
#define IDC_CONNECT 1006
#define IDC_DISCONNECT 1007
#define IDC_LOG 1008
#define IDC_STATUS 1009

typedef struct ntap_gui_state {
    HWND hwnd;
    HWND server_edit;
    HWND username_edit;
    HWND password_edit;
    HWND network_edit;
    HWND tap_edit;
    HWND connect_button;
    HWND disconnect_button;
    HWND log_edit;
    HWND status_static;
    HANDLE worker_thread;
    HANDLE child_process;
    CRITICAL_SECTION lock;
    wchar_t exe_dir[MAX_PATH];
    wchar_t package_root[MAX_PATH];
    wchar_t cli_path[MAX_PATH];
    wchar_t config_path[MAX_PATH];
    wchar_t ensure_script[MAX_PATH];
    volatile LONG connected;
} ntap_gui_state_t;

static ntap_gui_state_t g_gui;

static void post_allocated_text(UINT msg, const wchar_t *text)
{
    wchar_t *copy = NULL;
    size_t len = 0;

    if (text == NULL) {
        text = L"";
    }
    len = wcslen(text);
    copy = (wchar_t *)calloc(len + 1u, sizeof(wchar_t));
    if (copy == NULL) {
        return;
    }
    (void)wcscpy(copy, text);
    (void)PostMessageW(g_gui.hwnd, msg, 0, (LPARAM)copy);
}

static void post_log(const wchar_t *text)
{
    post_allocated_text(NTAP_LOG_MESSAGE, text);
}

static void post_status(const wchar_t *text)
{
    post_allocated_text(NTAP_STATUS_MESSAGE, text);
}

static void append_log_text(HWND edit, const wchar_t *text)
{
    int len = 0;

    if (edit == NULL || text == NULL) {
        return;
    }
    len = GetWindowTextLengthW(edit);
    (void)SendMessageW(edit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    (void)SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    (void)SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

static void set_controls_running(BOOL running)
{
    EnableWindow(g_gui.connect_button, !running);
    EnableWindow(g_gui.disconnect_button, running);
}

static void join_path(wchar_t *out, size_t out_len, const wchar_t *left,
                      const wchar_t *right)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    if (left == NULL) {
        left = L"";
    }
    if (right == NULL) {
        right = L"";
    }
    if (left[0] == L'\0') {
        (void)_snwprintf(out, out_len, L"%s", right);
    } else if (left[wcslen(left) - 1u] == L'\\' || left[wcslen(left) - 1u] == L'/') {
        (void)_snwprintf(out, out_len, L"%s%s", left, right);
    } else {
        (void)_snwprintf(out, out_len, L"%s\\%s", left, right);
    }
    out[out_len - 1u] = L'\0';
}

static void parent_dir(wchar_t *path)
{
    wchar_t *slash = NULL;

    if (path == NULL || path[0] == L'\0') {
        return;
    }
    slash = wcsrchr(path, L'\\');
    if (slash == NULL) {
        slash = wcsrchr(path, L'/');
    }
    if (slash != NULL) {
        *slash = L'\0';
    }
}

static bool file_exists(const wchar_t *path)
{
    DWORD attrs = GetFileAttributesW(path);

    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void resolve_full_path(wchar_t *out, size_t out_len, const wchar_t *path)
{
    DWORD len = 0;

    if (out == NULL || out_len == 0) {
        return;
    }
    len = GetFullPathNameW(path, (DWORD)out_len, out, NULL);
    if (len == 0 || len >= out_len) {
        (void)_snwprintf(out, out_len, L"%s", path);
        out[out_len - 1u] = L'\0';
    }
}

static void init_paths(void)
{
    wchar_t exe_path[MAX_PATH];
    wchar_t temp[MAX_PATH];
    wchar_t program_data[MAX_PATH];
    DWORD got = 0;

    exe_path[0] = L'\0';
    (void)GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    (void)_snwprintf(g_gui.exe_dir, MAX_PATH, L"%s", exe_path);
    g_gui.exe_dir[MAX_PATH - 1u] = L'\0';
    parent_dir(g_gui.exe_dir);

    (void)_snwprintf(g_gui.package_root, MAX_PATH, L"%s", g_gui.exe_dir);
    g_gui.package_root[MAX_PATH - 1u] = L'\0';
    parent_dir(g_gui.package_root);

    join_path(g_gui.cli_path, MAX_PATH, g_gui.exe_dir, L"ntap-c-cli.exe");

    got = GetEnvironmentVariableW(L"ProgramData", program_data, MAX_PATH);
    if (got == 0 || got >= MAX_PATH) {
        (void)_snwprintf(program_data, MAX_PATH, L"C:\\ProgramData");
    }
    join_path(temp, MAX_PATH, program_data, L"NTAP");
    (void)CreateDirectoryW(temp, NULL);
    join_path(g_gui.config_path, MAX_PATH, temp, L"ntap-c-gui.conf");

    join_path(temp, MAX_PATH, g_gui.package_root, L"install\\ensure-tap-windows.ps1");
    if (file_exists(temp)) {
        (void)_snwprintf(g_gui.ensure_script, MAX_PATH, L"%s", temp);
        return;
    }
    join_path(temp, MAX_PATH, g_gui.exe_dir, L"ensure-tap-windows.ps1");
    if (file_exists(temp)) {
        (void)_snwprintf(g_gui.ensure_script, MAX_PATH, L"%s", temp);
        return;
    }
    join_path(temp, MAX_PATH, g_gui.exe_dir, L"..\\..\\..\\scripts\\windows\\ensure-tap-windows.ps1");
    resolve_full_path(g_gui.ensure_script, MAX_PATH, temp);
    if (!file_exists(g_gui.ensure_script)) {
        g_gui.ensure_script[0] = L'\0';
    }
}

static bool get_window_text_alloc(HWND hwnd, wchar_t **out)
{
    int len = 0;
    wchar_t *buf = NULL;

    if (out == NULL) {
        return false;
    }
    *out = NULL;
    len = GetWindowTextLengthW(hwnd);
    buf = (wchar_t *)calloc((size_t)len + 1u, sizeof(wchar_t));
    if (buf == NULL) {
        return false;
    }
    (void)GetWindowTextW(hwnd, buf, len + 1);
    *out = buf;
    return true;
}

static bool wide_to_utf8(const wchar_t *input, char *out, size_t out_len)
{
    int needed = 0;

    if (out == NULL || out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (input == NULL) {
        return true;
    }
    needed = WideCharToMultiByte(CP_UTF8, 0, input, -1, out,
                                 (int)out_len, NULL, NULL);
    return needed > 0;
}

static void scrub_config_value(char *value)
{
    size_t i = 0;

    if (value == NULL) {
        return;
    }
    for (i = 0; value[i] != '\0'; i++) {
        if (value[i] == '\r' || value[i] == '\n') {
            value[i] = ' ';
        }
    }
}

static bool write_gui_config(const wchar_t *server, const wchar_t *username,
                             const wchar_t *password, const wchar_t *network_id,
                             const wchar_t *tap_name, wchar_t *err,
                             size_t err_len)
{
    char server_utf8[512];
    char username_utf8[256];
    char password_utf8[256];
    char network_utf8[64];
    char tap_utf8[128];
    char config_path_utf8[MAX_PATH * 3];
    char log_path_utf8[MAX_PATH * 3];
    wchar_t log_path[MAX_PATH];
    FILE *fp = NULL;

    if (!wide_to_utf8(server, server_utf8, sizeof(server_utf8)) ||
        !wide_to_utf8(username, username_utf8, sizeof(username_utf8)) ||
        !wide_to_utf8(password, password_utf8, sizeof(password_utf8)) ||
        !wide_to_utf8(network_id, network_utf8, sizeof(network_utf8)) ||
        !wide_to_utf8(tap_name, tap_utf8, sizeof(tap_utf8)) ||
        !wide_to_utf8(g_gui.config_path, config_path_utf8, sizeof(config_path_utf8))) {
        (void)_snwprintf(err, err_len, L"连接信息编码失败");
        return false;
    }
    scrub_config_value(server_utf8);
    scrub_config_value(username_utf8);
    scrub_config_value(password_utf8);
    scrub_config_value(network_utf8);
    scrub_config_value(tap_utf8);

    (void)_snwprintf(log_path, MAX_PATH, L"%s", g_gui.config_path);
    log_path[MAX_PATH - 1u] = L'\0';
    parent_dir(log_path);
    join_path(log_path, MAX_PATH, log_path, L"ntap-c.log");
    if (!wide_to_utf8(log_path, log_path_utf8, sizeof(log_path_utf8))) {
        (void)snprintf(log_path_utf8, sizeof(log_path_utf8), "./ntap-c.log");
    }

    fp = _wfopen(g_gui.config_path, L"wb");
    if (fp == NULL) {
        (void)_snwprintf(err, err_len, L"无法写入配置文件：%s", g_gui.config_path);
        return false;
    }
    (void)fprintf(fp,
                  "[client]\n"
                  "server_addr=%s\n"
                  "username=%s\n"
                  "password=%s\n"
                  "network_id=%s\n"
                  "tap_name=%s\n"
                  "mtu=1400\n\n"
                  "[log]\n"
                  "level=info\n"
                  "file=%s\n",
                  server_utf8, username_utf8, password_utf8,
                  network_utf8[0] == '\0' ? "1" : network_utf8,
                  tap_utf8[0] == '\0' ? "ntap-c0" : tap_utf8,
                  log_path_utf8);
    (void)fclose(fp);
    return true;
}

static void append_quoted(wchar_t *out, size_t out_len, const wchar_t *value)
{
    size_t pos = wcslen(out);
    size_t i = 0;

    if (pos + 2u >= out_len) {
        return;
    }
    out[pos++] = L'"';
    for (i = 0; value != NULL && value[i] != L'\0' && pos + 2u < out_len; i++) {
        if (value[i] == L'"') {
            out[pos++] = L'\\';
        }
        out[pos++] = value[i];
    }
    out[pos++] = L'"';
    out[pos] = L'\0';
}

static bool run_process_capture(const wchar_t *exe, const wchar_t *args,
                                DWORD *exit_code, char *output,
                                size_t output_len)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmd[4096];
    DWORD total = 0;
    bool ok = false;

    if (output != NULL && output_len > 0) {
        output[0] = '\0';
    }
    if (exit_code != NULL) {
        *exit_code = 1;
    }
    (void)memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return false;
    }
    (void)SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    cmd[0] = L'\0';
    append_quoted(cmd, sizeof(cmd) / sizeof(cmd[0]), exe);
    if (args != NULL && args[0] != L'\0') {
        (void)wcsncat(cmd, L" ", (sizeof(cmd) / sizeof(cmd[0])) - wcslen(cmd) - 1u);
        (void)wcsncat(cmd, args, (sizeof(cmd) / sizeof(cmd[0])) - wcslen(cmd) - 1u);
    }

    (void)memset(&si, 0, sizeof(si));
    (void)memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = NULL;

    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        goto done;
    }
    CloseHandle(write_pipe);
    write_pipe = NULL;

    for (;;) {
        char buf[512];
        DWORD got = 0;

        if (!ReadFile(read_pipe, buf, sizeof(buf), &got, NULL) || got == 0) {
            break;
        }
        if (output != NULL && output_len > 1u && total < output_len - 1u) {
            DWORD copy = got;
            if ((size_t)copy > output_len - 1u - total) {
                copy = (DWORD)(output_len - 1u - total);
            }
            (void)memcpy(output + total, buf, copy);
            total += copy;
            output[total] = '\0';
        }
    }
    (void)WaitForSingleObject(pi.hProcess, INFINITE);
    if (exit_code != NULL) {
        (void)GetExitCodeProcess(pi.hProcess, exit_code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ok = true;

done:
    if (write_pipe != NULL) {
        CloseHandle(write_pipe);
    }
    if (read_pipe != NULL) {
        CloseHandle(read_pipe);
    }
    return ok;
}

static void post_process_output(const char *output)
{
    wchar_t wide[4096];
    int rc = 0;

    if (output == NULL || output[0] == '\0') {
        return;
    }
    rc = MultiByteToWideChar(CP_UTF8, 0, output, -1, wide,
                             (int)(sizeof(wide) / sizeof(wide[0])));
    if (rc <= 0) {
        rc = MultiByteToWideChar(CP_ACP, 0, output, -1, wide,
                                 (int)(sizeof(wide) / sizeof(wide[0])));
    }
    if (rc > 0) {
        post_log(wide);
    }
}

static bool run_elevated_tap_prepare(const wchar_t *tap_name)
{
    SHELLEXECUTEINFOW sei;
    wchar_t params[2048];
    DWORD exit_code = 1;

    if (g_gui.ensure_script[0] == L'\0') {
        post_log(L"未找到 TAP 自动准备脚本。请确认安装包完整。");
        return false;
    }
    params[0] = L'\0';
    (void)_snwprintf(params, sizeof(params) / sizeof(params[0]),
                     L"-NoProfile -ExecutionPolicy Bypass -File ");
    append_quoted(params, sizeof(params) / sizeof(params[0]), g_gui.ensure_script);
    (void)wcsncat(params, L" -TapName ",
                  (sizeof(params) / sizeof(params[0])) - wcslen(params) - 1u);
    append_quoted(params, sizeof(params) / sizeof(params[0]), tap_name);

    post_log(L"正在请求管理员权限准备 TAP 适配器...");
    (void)memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"powershell.exe";
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei) || sei.hProcess == NULL) {
        post_log(L"管理员授权被取消或启动失败，无法自动准备 TAP。");
        return false;
    }
    (void)WaitForSingleObject(sei.hProcess, INFINITE);
    (void)GetExitCodeProcess(sei.hProcess, &exit_code);
    CloseHandle(sei.hProcess);
    if (exit_code != 0) {
        post_log(L"TAP 自动准备没有成功。请安装 TAP-Windows6/OpenVPN TAP 驱动后重试。");
        return false;
    }
    post_log(L"TAP 自动准备完成，正在重新检测...");
    return true;
}

static bool ensure_tap_ready(const wchar_t *tap_name)
{
    wchar_t args[1024];
    char output[8192];
    DWORD exit_code = 1;

    args[0] = L'\0';
    (void)wcsncat(args, L"-c ", (sizeof(args) / sizeof(args[0])) - 1u);
    append_quoted(args, sizeof(args) / sizeof(args[0]), g_gui.config_path);
    (void)wcsncat(args, L" check-env",
                  (sizeof(args) / sizeof(args[0])) - wcslen(args) - 1u);

    post_status(L"正在检测 TAP...");
    post_log(L"正在检测 TAP 适配器...");
    if (!run_process_capture(g_gui.cli_path, args, &exit_code, output, sizeof(output))) {
        post_log(L"无法启动 ntap-c-cli.exe，请确认安装包完整。");
        return false;
    }
    if (exit_code == 0) {
        post_log(L"TAP 检测通过。");
        return true;
    }
    post_process_output(output);
    post_log(L"未检测到可用 TAP，开始自动准备。");
    if (!run_elevated_tap_prepare(tap_name)) {
        return false;
    }
    output[0] = '\0';
    exit_code = 1;
    if (!run_process_capture(g_gui.cli_path, args, &exit_code, output, sizeof(output))) {
        post_log(L"无法重新检测 TAP。");
        return false;
    }
    if (exit_code != 0) {
        post_process_output(output);
        post_log(L"TAP 仍不可用。请安装 TAP-Windows6/OpenVPN TAP 驱动，然后重新连接。");
        return false;
    }
    post_log(L"TAP 检测通过。");
    return true;
}

static void store_child_process(HANDLE process)
{
    EnterCriticalSection(&g_gui.lock);
    g_gui.child_process = process;
    LeaveCriticalSection(&g_gui.lock);
}

static HANDLE load_child_process(void)
{
    HANDLE process = NULL;

    EnterCriticalSection(&g_gui.lock);
    process = g_gui.child_process;
    LeaveCriticalSection(&g_gui.lock);
    return process;
}

static void clear_child_process(void)
{
    EnterCriticalSection(&g_gui.lock);
    g_gui.child_process = NULL;
    LeaveCriticalSection(&g_gui.lock);
}

static void run_client_process(void)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmd[2048];
    DWORD exit_code = 1;

    (void)memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        post_log(L"创建日志管道失败。");
        return;
    }
    (void)SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    cmd[0] = L'\0';
    append_quoted(cmd, sizeof(cmd) / sizeof(cmd[0]), g_gui.cli_path);
    (void)wcsncat(cmd, L" -c ", (sizeof(cmd) / sizeof(cmd[0])) - wcslen(cmd) - 1u);
    append_quoted(cmd, sizeof(cmd) / sizeof(cmd[0]), g_gui.config_path);
    (void)wcsncat(cmd, L" run", (sizeof(cmd) / sizeof(cmd[0])) - wcslen(cmd) - 1u);

    (void)memset(&si, 0, sizeof(si));
    (void)memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;

    post_status(L"正在连接...");
    post_log(L"正在连接 NTAP-A...");
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, g_gui.exe_dir, &si, &pi)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        post_log(L"启动连接进程失败。");
        return;
    }
    CloseHandle(write_pipe);
    write_pipe = NULL;
    CloseHandle(pi.hThread);
    store_child_process(pi.hProcess);
    InterlockedExchange(&g_gui.connected, 1);
    post_status(L"已连接 / 运行中");

    for (;;) {
        char buf[1024];
        DWORD got = 0;

        if (!ReadFile(read_pipe, buf, sizeof(buf) - 1u, &got, NULL) || got == 0) {
            break;
        }
        buf[got] = '\0';
        post_process_output(buf);
    }
    (void)WaitForSingleObject(pi.hProcess, INFINITE);
    (void)GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    clear_child_process();
    InterlockedExchange(&g_gui.connected, 0);
    if (exit_code == 0) {
        post_status(L"已断开");
        post_log(L"连接已结束。");
    } else {
        wchar_t msg[128];

        (void)_snwprintf(msg, sizeof(msg) / sizeof(msg[0]),
                         L"连接已停止，退出码 %lu。", (unsigned long)exit_code);
        post_status(L"连接失败或已停止");
        post_log(msg);
    }
    if (read_pipe != NULL) {
        CloseHandle(read_pipe);
    }
}

static DWORD WINAPI connect_worker(LPVOID opaque)
{
    wchar_t *server = NULL;
    wchar_t *username = NULL;
    wchar_t *password = NULL;
    wchar_t *network = NULL;
    wchar_t *tap_name = NULL;
    const wchar_t *network_value = NULL;
    const wchar_t *tap_value = NULL;
    wchar_t err[512];
    bool ok = false;

    (void)opaque;
    post_status(L"准备连接...");
    if (!get_window_text_alloc(g_gui.server_edit, &server) ||
        !get_window_text_alloc(g_gui.username_edit, &username) ||
        !get_window_text_alloc(g_gui.password_edit, &password) ||
        !get_window_text_alloc(g_gui.network_edit, &network) ||
        !get_window_text_alloc(g_gui.tap_edit, &tap_name)) {
        post_log(L"读取窗口输入失败。");
        goto done;
    }
    if (server[0] == L'\0' || username[0] == L'\0' || password[0] == L'\0') {
        post_status(L"请填写连接信息");
        post_log(L"请填写服务器、账号和密码。");
        goto done;
    }
    network_value = network[0] == L'\0' ? L"1" : network;
    tap_value = tap_name[0] == L'\0' ? L"ntap-c0" : tap_name;
    err[0] = L'\0';
    if (!write_gui_config(server, username, password, network_value, tap_value,
                          err, sizeof(err) / sizeof(err[0]))) {
        post_status(L"配置写入失败");
        post_log(err);
        goto done;
    }
    if (!file_exists(g_gui.cli_path)) {
        post_status(L"安装包不完整");
        post_log(L"缺少 ntap-c-cli.exe，无法连接。");
        goto done;
    }
    ok = ensure_tap_ready(tap_value);
    if (!ok) {
        post_status(L"TAP 未就绪");
        goto done;
    }
    run_client_process();

done:
    free(server);
    free(username);
    free(password);
    free(network);
    free(tap_name);
    InterlockedExchange(&g_gui.connected, 0);
    (void)PostMessageW(g_gui.hwnd, NTAP_DONE_MESSAGE, 0, 0);
    return 0;
}

static void on_connect(void)
{
    DWORD thread_id = 0;

    if (InterlockedCompareExchange(&g_gui.connected, 1, 0) != 0) {
        return;
    }
    set_controls_running(TRUE);
    g_gui.worker_thread = CreateThread(NULL, 0, connect_worker, NULL, 0, &thread_id);
    if (g_gui.worker_thread == NULL) {
        InterlockedExchange(&g_gui.connected, 0);
        set_controls_running(FALSE);
        post_log(L"启动连接线程失败。");
    }
}

static void on_disconnect(void)
{
    HANDLE child = load_child_process();

    if (child != NULL) {
        post_log(L"正在断开连接...");
        (void)TerminateProcess(child, 2);
    }
    set_controls_running(FALSE);
    post_status(L"已断开");
}

static HWND add_label(HWND parent, const wchar_t *text, int x, int y, int w, int h)
{
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, h, parent, NULL, GetModuleHandleW(NULL), NULL);
}

static HWND add_edit(HWND parent, int id, const wchar_t *text, int x, int y,
                     int w, int h, DWORD extra_style)
{
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra_style,
                           x, y, w, h, parent, (HMENU)(intptr_t)id,
                           GetModuleHandleW(NULL), NULL);
}

static void create_controls(HWND hwnd)
{
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HWND controls[16];
    size_t count = 0;

    controls[count++] = add_label(hwnd, L"服务器", 18, 20, 80, 22);
    g_gui.server_edit = add_edit(hwnd, IDC_SERVER, L"127.0.0.1:8024", 110, 18, 260, 24, 0);
    controls[count++] = g_gui.server_edit;

    controls[count++] = add_label(hwnd, L"账号", 18, 54, 80, 22);
    g_gui.username_edit = add_edit(hwnd, IDC_USERNAME, L"", 110, 52, 260, 24, 0);
    controls[count++] = g_gui.username_edit;

    controls[count++] = add_label(hwnd, L"密码", 18, 88, 80, 22);
    g_gui.password_edit = add_edit(hwnd, IDC_PASSWORD, L"", 110, 86, 260, 24, ES_PASSWORD);
    controls[count++] = g_gui.password_edit;

    controls[count++] = add_label(hwnd, L"网络 ID", 18, 122, 80, 22);
    g_gui.network_edit = add_edit(hwnd, IDC_NETWORK, L"1", 110, 120, 95, 24, 0);
    controls[count++] = g_gui.network_edit;

    controls[count++] = add_label(hwnd, L"TAP 名称", 220, 122, 80, 22);
    g_gui.tap_edit = add_edit(hwnd, IDC_TAPNAME, L"ntap-c0", 290, 120, 80, 24, 0);
    controls[count++] = g_gui.tap_edit;

    g_gui.connect_button = CreateWindowExW(0, L"BUTTON", L"连接",
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                           390, 18, 92, 34, hwnd,
                                           (HMENU)(intptr_t)IDC_CONNECT,
                                           GetModuleHandleW(NULL), NULL);
    controls[count++] = g_gui.connect_button;
    g_gui.disconnect_button = CreateWindowExW(0, L"BUTTON", L"断开",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                              390, 60, 92, 34, hwnd,
                                              (HMENU)(intptr_t)IDC_DISCONNECT,
                                              GetModuleHandleW(NULL), NULL);
    controls[count++] = g_gui.disconnect_button;

    g_gui.status_static = CreateWindowExW(0, L"STATIC", L"未连接",
                                          WS_CHILD | WS_VISIBLE,
                                          390, 114, 150, 22, hwnd,
                                          (HMENU)(intptr_t)IDC_STATUS,
                                          GetModuleHandleW(NULL), NULL);
    controls[count++] = g_gui.status_static;

    g_gui.log_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                     ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
                                     ES_READONLY,
                                     18, 158, 520, 260, hwnd,
                                     (HMENU)(intptr_t)IDC_LOG,
                                     GetModuleHandleW(NULL), NULL);
    controls[count++] = g_gui.log_edit;

    for (size_t i = 0; i < count; i++) {
        (void)SendMessageW(controls[i], WM_SETFONT, (WPARAM)font, TRUE);
    }
    set_controls_running(FALSE);
    append_log_text(g_gui.log_edit, L"填写连接信息后点“连接”。程序会自动检测并准备 TAP。");
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE:
        g_gui.hwnd = hwnd;
        create_controls(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_CONNECT) {
            on_connect();
            return 0;
        }
        if (LOWORD(wparam) == IDC_DISCONNECT) {
            on_disconnect();
            return 0;
        }
        return 0;
    case NTAP_LOG_MESSAGE:
        append_log_text(g_gui.log_edit, (const wchar_t *)lparam);
        free((void *)lparam);
        return 0;
    case NTAP_STATUS_MESSAGE:
        SetWindowTextW(g_gui.status_static, (const wchar_t *)lparam);
        free((void *)lparam);
        return 0;
    case NTAP_DONE_MESSAGE:
        set_controls_running(FALSE);
        return 0;
    case WM_CLOSE:
        on_disconnect();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                   LPSTR cmd_line, int show_cmd)
{
    WNDCLASSW wc;
    MSG msg;

    (void)prev_instance;
    (void)cmd_line;
    (void)memset(&g_gui, 0, sizeof(g_gui));
    InitializeCriticalSection(&g_gui.lock);
    init_paths();

    (void)memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = NTAP_GUI_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&wc)) {
        DeleteCriticalSection(&g_gui.lock);
        return 1;
    }

    g_gui.hwnd = CreateWindowExW(0, NTAP_GUI_CLASS, NTAP_GUI_TITLE,
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                 WS_MINIMIZEBOX,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 575, 475,
                                 NULL, NULL, instance, NULL);
    if (g_gui.hwnd == NULL) {
        DeleteCriticalSection(&g_gui.lock);
        return 1;
    }
    ShowWindow(g_gui.hwnd, show_cmd);
    UpdateWindow(g_gui.hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteCriticalSection(&g_gui.lock);
    return (int)msg.wParam;
}
#endif
