#include "duk_config.h"
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600

#if _MSC_VER
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "duktape.h"

#define NAME                    "dwmjs"     /* Used for window name/class */
#define PATH_SEPERATOR          "\\"
static HWND dwmhwnd;
static HINSTANCE hinstance;
static duk_context *duk_ctx;

static UINT shellhookid;
typedef BOOL (*RegisterShellHookWindowProc) (HWND);
RegisterShellHookWindowProc _RegisterShellHookWindow;

static void cleanup();
static void die(const char *fmt, ...);
static void setup(HINSTANCE hInstance);
static BOOL CALLBACK scan(HWND hwnd, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void
die(const char *fmt, ...) {
    char buf[5000];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, 5000, fmt, args);
    va_end(args);

    MessageBox(NULL, buf, "Fatal error", MB_OK | MB_ICONERROR);

    cleanup();
    exit(EXIT_FAILURE);
}

static void
logd(const char *fmt, ...) {
    char buf[5000];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, 5000, fmt, args);
    va_end(args);

    MessageBox(NULL, buf, "Log", MB_OK);
}

static void
cleanup() {
    if (duk_ctx) {
        duk_destroy_heap(duk_ctx);
        duk_ctx = NULL;
    }
}

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            /* dwmjs.dispatchEvent('load') */
            duk_get_global_string(duk_ctx, "dwmjs");
            duk_push_string(duk_ctx, "dispatchEvent");
            duk_push_string(duk_ctx, "load");
            duk_pcall_prop(duk_ctx, -3, 1);
            duk_pop_2(duk_ctx);
            break;
        case WM_CLOSE:
            cleanup();
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            if (msg == shellhookid) { /* Handle the shell hook message */
                switch (wParam & 0x7fff) {
                    case HSHELL_WINDOWCREATED:
                        duk_idx_t window_create_event_top = duk_get_top(duk_ctx);
                        duk_get_global_string(duk_ctx, "dwmjs");
                        duk_push_string(duk_ctx, "dispatchEvent");
                        duk_push_string(duk_ctx, "windowcreate");
                        duk_idx_t window_create_event_index = duk_push_object(duk_ctx);
                        duk_push_string(duk_ctx, "windowId");
                        duk_push_number(duk_ctx, (long)(HWND)lParam);
                        duk_put_prop(duk_ctx, window_create_event_index);

                        duk_pcall_prop(duk_ctx, window_create_event_top, 2);
                        duk_pop(duk_ctx); /* result */
                        break;
                    case HSHELL_WINDOWDESTROYED:
                        duk_idx_t window_close_event_top = duk_get_top(duk_ctx);
                        duk_get_global_string(duk_ctx, "dwmjs");
                        duk_push_string(duk_ctx, "dispatchEvent");
                        duk_push_string(duk_ctx, "windowclose");
                        duk_idx_t window_close_event_index = duk_push_object(duk_ctx);
                        duk_push_string(duk_ctx, "windowId");
                        duk_push_number(duk_ctx, (long)(HWND)lParam);
                        duk_put_prop(duk_ctx, window_close_event_index);

                        duk_pcall_prop(duk_ctx, window_close_event_top, 2);
                        duk_pop(duk_ctx); /* result */
                        break;
                    case HSHELL_WINDOWACTIVATED:
                        duk_idx_t window_activate_event_top = duk_get_top(duk_ctx);
                        duk_get_global_string(duk_ctx, "dwmjs");
                        duk_push_string(duk_ctx, "dispatchEvent");
                        duk_push_string(duk_ctx, "windowfocusin");
                        duk_idx_t window_activate_event_index = duk_push_object(duk_ctx);
                        duk_push_string(duk_ctx, "windowId");
                        duk_push_number(duk_ctx, (long)(HWND)lParam);
                        duk_put_prop(duk_ctx, window_activate_event_index);

                        duk_pcall_prop(duk_ctx, window_activate_event_top, 2);

                        duk_pop(duk_ctx); /* result */
                        break;
                }
            } else {
                return DefWindowProc(hwnd, msg, wParam, lParam);
            }
    }

    return 0;
}

static void
setup(HINSTANCE hInstance) {
    WNDCLASSEX winClass;

    winClass.cbSize = sizeof(WNDCLASSEX);
    winClass.style = 0;
    winClass.lpfnWndProc = (WNDPROC)WndProc;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = hInstance;
    winClass.hIcon = NULL;
    winClass.hIconSm = NULL;
    winClass.hCursor = NULL;
    winClass.hbrBackground = NULL;
    winClass.lpszMenuName = NULL;
    winClass.lpszClassName = NAME;

    if (!RegisterClassEx(&winClass))
        die("Error registering dwmjs window class");

    dwmhwnd = CreateWindowEx(0, NAME, NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!dwmhwnd)
        die("Error creating dwmjs window");

    /* Get function pointer for RegisterShellHookWindow */
    _RegisterShellHookWindow = (RegisterShellHookWindowProc)GetProcAddress(GetModuleHandle("USER32.DLL"), "RegisterShellHookWindow");
    if (!_RegisterShellHookWindow)
        die("Could not find RegisterShellHookWindow");
    _RegisterShellHookWindow(dwmhwnd);
    /* Grab a dynamic id for the SHELLHOOK message to be used later */
    shellhookid = RegisterWindowMessage("SHELLHOOK");
}


duk_ret_t
jduk_alert(duk_context *ctx) {
    MessageBox(NULL, (char*)duk_to_string(ctx, -1), "dwmjs: Info", MB_OK);
    duk_pop(ctx);
    return 0;
}

typedef struct MonitorIdListEnumProcState {
    duk_context *ctx;
    uint32_t count;
    duk_idx_t monitors_array_idx;
} MonitorIdListEnumProcState;

BOOL CALLBACK MonitorIdListEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MonitorIdListEnumProcState *state;
    state = (MonitorIdListEnumProcState*)dwData;

    duk_push_number(state->ctx, (uint64_t)hMonitor);
    duk_put_prop_index(state->ctx, state->monitors_array_idx, state->count);

    state->count = state->count + 1;

    return TRUE;
}

duk_ret_t
jduk_get_all_monitors(duk_context *ctx) {
    MonitorIdListEnumProcState state;
    state.ctx = ctx;
    state.count = 0;
    state.monitors_array_idx = duk_push_array(ctx);

    EnumDisplayMonitors(NULL, NULL, &MonitorIdListEnumProc, (LPARAM)&state);

    return 1;
}

typedef struct EnumWindowState {
    duk_context *duk_ctx;
    duk_int32_t count;
    duk_idx_t windows_array_idx;
} EnumWindowState;

BOOL CALLBACK
scan(HWND hwnd, LPARAM lParam) {
    EnumWindowState *state;
    state = (EnumWindowState*)lParam;

    duk_push_number(state->duk_ctx, (long)hwnd);
    duk_put_prop_index(state->duk_ctx, state->windows_array_idx, state->count);

    state->count = state->count + 1;
    return TRUE;
}

duk_ret_t
jduk_get_monitor_by_id(duk_context *ctx) {
    size_t monitor_id = duk_to_number(ctx, -1);
    HMONITOR hMonitor = (HMONITOR)monitor_id;

    MONITORINFO iMonitor;
    iMonitor.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &iMonitor);

    duk_idx_t monitor_obj_idx = duk_push_object(ctx);

    duk_push_string(ctx, "id");
    duk_push_number(ctx, monitor_id);
    duk_put_prop(ctx, monitor_obj_idx);

    /* duk_push_string(ctx, "name"); */
    /* duk_push_string(ctx, iMonitor.szDevice); */
    /* duk_put_prop(ctx, monitor_obj_idx); */

    duk_push_string(ctx, "x");
    duk_push_number(ctx, iMonitor.rcMonitor.left);
    duk_put_prop(ctx, monitor_obj_idx);

    duk_push_string(ctx, "y");
    duk_push_number(ctx, iMonitor.rcMonitor.top);
    duk_put_prop(ctx, monitor_obj_idx);

    duk_push_string(ctx, "width");
    duk_push_number(ctx, iMonitor.rcMonitor.right - iMonitor.rcMonitor.left);
    duk_put_prop(ctx, monitor_obj_idx);

    duk_push_string(ctx, "height");
    duk_push_number(ctx, iMonitor.rcMonitor.bottom - iMonitor.rcMonitor.top);
    duk_put_prop(ctx, monitor_obj_idx);

    return 1;
}

duk_ret_t
jduk_get_all_windows(duk_context *ctx) {
    EnumWindowState state;
    state.duk_ctx = ctx;
    state.count = 0;
    state.windows_array_idx = duk_push_array(ctx);

    EnumWindows(scan, (LPARAM)&state);

    return 1;
}

duk_ret_t
jduk_get_window_by_id(duk_context *ctx) {
    duk_int32_t window_id = duk_to_number(ctx, -1);
    HWND hwnd = (HWND)(LONG_PTR)window_id;
    WINDOWINFO window_info = { .cbSize = sizeof window_info };
    DWORD buf_size = MAX_PATH;
    TCHAR buf[MAX_PATH];

    duk_idx_t window_obj_idx = duk_push_object(ctx);

    duk_push_string(ctx, "id");
    duk_push_number(ctx, window_id);
    duk_put_prop(ctx, window_obj_idx);

    HWND parent = GetParent(hwnd);
    duk_push_string(ctx, "parentId");
    duk_push_number(ctx, (long)parent);
    duk_put_prop(ctx, window_obj_idx);

    int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    duk_push_string(ctx, "isTopMost");
    duk_push_boolean(ctx, (exstyle & WS_EX_TOPMOST) > 0);
    duk_put_prop(ctx, window_obj_idx);

    duk_push_string(ctx, "isToolWindow");
    duk_push_boolean(ctx, (exstyle & WS_EX_TOOLWINDOW) > 0);
    duk_put_prop(ctx, window_obj_idx);

    duk_push_string(ctx, "isAppWindow");
    duk_push_boolean(ctx, (exstyle & WS_EX_APPWINDOW) > 0);
    duk_put_prop(ctx, window_obj_idx);

    duk_push_string(ctx, "isClientEdge");
    duk_push_boolean(ctx, (exstyle & WS_EX_CLIENTEDGE) > 0);
    duk_put_prop(ctx, window_obj_idx);

    duk_push_string(ctx, "processId");
    DWORD dwProcessID = 0;
    GetWindowThreadProcessId(hwnd, &dwProcessID);
    duk_push_number(ctx, dwProcessID);
    duk_put_prop(ctx, window_obj_idx);

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessID);    
    if (hProc) {
        if (QueryFullProcessImageName(hProc, 0, buf, &buf_size)) {
            duk_push_string(ctx, "path");
            duk_push_string(ctx, buf);
            duk_put_prop(ctx, window_obj_idx);
        }
    }

    duk_push_string(ctx, "visibility");
    duk_push_string(ctx, IsWindowVisible(hwnd) > 0 ? "visible" : "hidden");
    duk_put_prop(ctx, window_obj_idx);

    ZeroMemory(buf, MAX_PATH);
    if (GetClassName(hwnd, buf, sizeof buf)) {
        duk_push_string(ctx, "className");
        duk_push_string(ctx, buf);
        duk_put_prop(ctx, window_obj_idx);
    }

    ZeroMemory(buf, MAX_PATH);
    GetWindowText(hwnd, buf, sizeof buf);
    duk_push_string(ctx, "title");
    duk_push_string(ctx, buf);
    duk_put_prop(ctx, window_obj_idx);

    if (GetWindowInfo(hwnd, &window_info)) {
        duk_push_string(ctx, "canMaximize");
        duk_push_number(ctx, (window_info.dwStyle & WS_MAXIMIZEBOX) > 0);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "canMinimize");
        duk_push_number(ctx, (window_info.dwStyle & WS_MINIMIZEBOX) > 0);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "isActive");
        duk_push_boolean(ctx, window_info.dwWindowStatus > 0);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "isPopup");
        duk_push_boolean(ctx, (window_info.dwStyle & WS_POPUP) > 0);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "borderWidth");
        duk_push_number(ctx, window_info.cxWindowBorders);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "borderHeight");
        duk_push_number(ctx, window_info.cyWindowBorders);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "x");
        duk_push_number(ctx, window_info.rcWindow.left);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "y");
        duk_push_number(ctx, window_info.rcWindow.top);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "width");
        duk_push_number(ctx, window_info.rcWindow.right - window_info.rcWindow.left);
        duk_put_prop(ctx, window_obj_idx);

        duk_push_string(ctx, "height");
        duk_push_number(ctx, window_info.rcWindow.bottom - window_info.rcWindow.top);
        duk_put_prop(ctx, window_obj_idx);
    }

    return 1;
}

void
jduk_set_window_attributes_border_bar_visibility(duk_context *ctx, duk_idx_t attributes_idx, HWND hwnd) {
    duk_get_prop_string(ctx, attributes_idx, "borderBarVisibility");
    const char *borderBarVisibility = duk_to_string(ctx, -1);
    duk_bool_t hidden = strcmp(borderBarVisibility, "hidden") == 0;
    if (hidden) {
        SetWindowLong(hwnd, GWL_STYLE, (GetWindowLong(hwnd, GWL_STYLE) & ~(WS_CAPTION | WS_SIZEBOX)) | WS_BORDER | WS_THICKFRAME);
        SetWindowLong(hwnd, GWL_EXSTYLE, (GetWindowLong(hwnd, GWL_EXSTYLE) & ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE)));
    } else {
        SetWindowLong(hwnd, GWL_STYLE, (GetWindowLong(hwnd, GWL_STYLE) | (WS_CAPTION | WS_SIZEBOX)));
    }
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    duk_pop(ctx);
}

void
jduk_set_window_attributes_visibility(duk_context *ctx, duk_idx_t attributes_idx, HWND hwnd) {
    duk_get_prop_string(ctx, attributes_idx, "visibility");
    const char *visibility = duk_to_string(ctx, -1);
    duk_bool_t hidden = strcmp(visibility, "hidden") == 0;
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, (hidden ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    duk_pop(ctx);
}

duk_ret_t
jduk_set_window_attributes(duk_context *ctx) {
    duk_idx_t window_id_idx = 0;
    duk_int32_t window_id = duk_to_number(ctx, window_id_idx);
    HWND hwnd = (HWND)(LONG_PTR)window_id;

    duk_idx_t attributes_idx = -1;
    if (duk_is_null_or_undefined(ctx, attributes_idx) || !duk_is_object(ctx, attributes_idx)) {
        duk_pop(ctx);
        return 0;
    }

    if (duk_has_prop_string(ctx, attributes_idx, "borderBarVisibility")) {
        jduk_set_window_attributes_border_bar_visibility(ctx, attributes_idx, hwnd);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "visibility")) {
        jduk_set_window_attributes_visibility(ctx, attributes_idx, hwnd);
    }

    duk_pop(ctx);
    duk_pop(ctx);
    return 0;
}

duk_ret_t
jduk_close_window(duk_context *ctx) {
    duk_idx_t window_id_idx = 0;
    duk_int32_t window_id = duk_to_number(ctx, window_id_idx);
    HWND hwnd = (HWND)(LONG_PTR)window_id;

    PostMessage(hwnd, WM_CLOSE, 0, 0);

    duk_pop(ctx);
    return 0;
}

duk_ret_t
jduk_exit(duk_context *ctx) {
    duk_int32_t code = duk_to_int32(ctx, -1);
    cleanup();
    exit(code);
    return 0;
}

#define EVENT_LISTENER_JS_CODE "(function(dwmjs) {"\
    "var listeners = {};"\
    "dwmjs.dispatchEvent = function (type, data) {"\
    "   var callbacks = listeners[type];"\
    "   if (callbacks) {"\
    "       for (var i = 0; i < callbacks.length; i++) {"\
    "           callbacks[i](data);"\
    "       }"\
    "   }"\
    "};"\
    "dwmjs.addEventListener = function (type, callback) {"\
    "   var callbacks = listeners[type];"\
    "    if (!callbacks) { callbacks = listeners[type] = []; }"\
    "    callbacks.push(callback);"\
    "};"\
    "dwmjs.removeEventListener = function (type, callback) {"\
    "    var callbacks = listeners[type];"\
    "    if (callbacks) {"\
    "        callbacks[type].splice(callbacks[type].indexOf(callback) >>> 0, 1);"\
    "    }"\
    "};"\
    "})(dwmjs);"

static duk_ret_t
jduk_init_context(duk_context *ctx, void *udata) {
    duk_push_global_object(ctx);

    duk_push_c_lightfunc(ctx, jduk_alert, 1, 1, 0);
    duk_put_prop_string(ctx, -2, "alert");

    duk_idx_t dwmjs_obj_id = duk_push_object(ctx);

    duk_push_c_lightfunc(ctx, jduk_exit, 1, 1, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "exit");

    duk_push_c_lightfunc(ctx, jduk_get_all_monitors, 0, 0, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "getMonitors");

    duk_push_c_lightfunc(ctx, jduk_get_monitor_by_id, 1, 1, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "getMonitorById");

    duk_push_c_lightfunc(ctx, jduk_get_all_windows, 0, 0, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "getWindows");

    duk_push_c_lightfunc(ctx, jduk_get_window_by_id, 1, 1, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "getWindowById");

    duk_push_c_lightfunc(ctx, jduk_set_window_attributes, 2, 2, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "setWindowAttributes");

    duk_push_c_lightfunc(ctx, jduk_close_window, 1, 1, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "closeWindow");

    duk_put_global_string(ctx, "dwmjs");

    duk_push_string(ctx, EVENT_LISTENER_JS_CODE);
    duk_eval(ctx);

    // init user script from ~/.dwm.js if it exists
    char *homedir;
    size_t len;
    errno_t err;
    err = _dupenv_s(&homedir, &len, "userprofile" );
    if (err)
        die("Failed to get home directory");

    char configfilepath[256];
    snprintf(configfilepath, sizeof configfilepath, "%s%s%s", homedir, PATH_SEPERATOR, ".dwm.js");
    free(homedir);

    FILE *configfilestream;
    err = fopen_s(&configfilestream, configfilepath, "r");
    if (configfilestream) {
        fseek(configfilestream, 0, SEEK_END);
        size_t length = ftell(configfilestream);
        fseek(configfilestream, 0, SEEK_SET);
        char *dwmjscontents = (char *) malloc(length + 1);
        dwmjscontents[length] = '\0';
        fread(dwmjscontents, 1, length, configfilestream);
        fclose(configfilestream);
        if (duk_peval_string(ctx, dwmjscontents) != 0) {
            die("Failed to execute ~/.dwm.js");
        }
    } else {
        die("Error reading %s config file.", configfilepath);
    }

    duk_pop(ctx);
    return 0;
}

int
WINAPI WinMain(HINSTANCE hInstance,
               HINSTANCE hPrevInstance,
               LPSTR lpCmdLine,
               int nShowCmd) {
    MSG msg;

    SetProcessDPIAware();

    duk_ctx = duk_create_heap_default();
    if (!duk_ctx) {
        die("Failed to create duktape context");
        return EXIT_FAILURE;
    }

    if (duk_safe_call(duk_ctx, jduk_init_context, NULL, 0, 1) != 0) {
        die("Failed to initialize duktape script");
        return EXIT_FAILURE;
    }

    hinstance = hInstance;
    setup(hInstance);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    cleanup();
    return EXIT_SUCCESS;
}
