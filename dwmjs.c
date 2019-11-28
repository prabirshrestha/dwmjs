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
#define BARNAME                 "dwmjsbar"  /* Used for window name/class */
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
static void setupbar(HINSTANCE hInstance);
static BOOL CALLBACK scan(HWND hwnd, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct BarState {
    HWND barhwnd;
    duk_int32_t x;
    duk_int32_t y;
    duk_int32_t width;
    duk_int32_t height;
} BarState;

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

LRESULT CALLBACK barhandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BarState *barState = NULL;
    switch (msg) {
        case WM_CREATE:
            CREATESTRUCT *CrtStrPtr = (CREATESTRUCT *)lParam;
            barState = (BarState*) CrtStrPtr->lpCreateParams;
            SetWindowPos(hwnd, HWND_TOPMOST, barState->x, barState->y, barState->width, barState->height, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            break;
        case WM_PAINT: {
            CREATESTRUCT *CrtStrPtr = (CREATESTRUCT *)lParam;
            barState = (BarState*) CrtStrPtr->lpCreateParams;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            rc.top = barState->y;
            rc.left = barState->x;
            rc.bottom = barState->height;
            rc.right = barState->width;
            HBRUSH greenBrush=CreateSolidBrush(RGB(0,255,0));
            FillRect(hdc, &rc, greenBrush);
            DeleteObject(greenBrush);
            EndPaint(hwnd, &ps);
            free(CrtStrPtr);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            /* buttonpress(msg, &MAKEPOINTS(lParam)); */
            break;
        case WM_TIMER:
            /* drawbar(); */
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

static int setupbarcomplete = 0;
static void
setupbar(HINSTANCE hInstance) {
    if (setupbarcomplete) { return; }
    WNDCLASS winClass;
    memset(&winClass, 0, sizeof winClass);

    winClass.style = 0;
    winClass.lpfnWndProc = barhandler;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = hInstance;
    winClass.hIcon = NULL;
    winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    winClass.hbrBackground = NULL;
    winClass.lpszMenuName = NULL;
    winClass.lpszClassName = BARNAME;

    if (!RegisterClass(&winClass))
        die("Error registering dwmjs-bar window class");

    setupbarcomplete = 1;
}

duk_ret_t
jduk_alert(duk_context *ctx) {
    MessageBox(NULL, (char*)duk_to_string(ctx, -1), "dwmjs: Info", MB_OK);
    duk_pop(ctx);
    return 0;
}

duk_ret_t
jduk_get_all_monitors(duk_context *ctx) {
    duk_idx_t monitors_array_idx = duk_push_array(ctx);
    duk_idx_t monitor_count = 0;

    DISPLAY_DEVICE display_device;
    display_device.cb = sizeof(DISPLAY_DEVICE);

    DWORD deviceNum = 0;
    while (EnumDisplayDevices(NULL, deviceNum, &display_device, 0)) {
        DISPLAY_DEVICE new_display_device = {0};
        new_display_device.cb = sizeof(DISPLAY_DEVICE);
        DWORD monitorNum = 0;
        while (EnumDisplayDevices(display_device.DeviceName, monitorNum, &new_display_device, 0))
        {
            // Get the display mode settings of this device.
            DEVMODE devMode;
            if (!EnumDisplaySettings(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &devMode))
                continue;

            duk_push_string(ctx, new_display_device.DeviceID);
            duk_put_prop_index(ctx, monitors_array_idx, monitor_count);

            monitor_count++;

            monitorNum++;
        }
        deviceNum++;
    }

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
    const char *monitor_id = duk_to_string(ctx, -1);

    duk_idx_t monitor_count = 0;
    DISPLAY_DEVICE display_device;
    display_device.cb = sizeof(DISPLAY_DEVICE);

    DWORD deviceNum = 0;
    while (EnumDisplayDevices(NULL, deviceNum, &display_device, 0)) {
        DISPLAY_DEVICE new_display_device = {0};
        new_display_device.cb = sizeof(DISPLAY_DEVICE);
        DWORD monitorNum = 0;
        while (EnumDisplayDevices(display_device.DeviceName, monitorNum, &new_display_device, 0))
        {
            // Get the display mode settings of this device.
            DEVMODE devMode;
            if (!EnumDisplaySettings(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &devMode))
                continue;

            if (strcmp(new_display_device.DeviceID, monitor_id) == 0) {
                duk_idx_t monitor_obj_idx = duk_push_object(ctx);

                duk_push_string(ctx, "id");
                duk_push_string(ctx, monitor_id);
                duk_put_prop(ctx, monitor_obj_idx);

                duk_push_string(ctx, "width");
                duk_push_number(ctx, devMode.dmPelsWidth);
                duk_put_prop(ctx, monitor_obj_idx);

                duk_push_string(ctx, "height");
                duk_push_number(ctx, devMode.dmPelsHeight);
                duk_put_prop(ctx, monitor_obj_idx);

                return 1;
            }

            monitor_count++;

            monitorNum++;
        }
        deviceNum++;
    }

    return 0;
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
    RECT rect;
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
jduk_get_task_bar(duk_context *ctx) {
    duk_idx_t task_bar_obj_idx = duk_push_object(ctx);

    HWND hwnd = FindWindow("Shell_TrayWnd", NULL);
    TCHAR buf[500];
    RECT rect;

    duk_push_string(ctx, "id");
    duk_push_number(ctx, (long)hwnd);
    duk_put_prop(ctx, task_bar_obj_idx);

    duk_push_string(ctx, "visibility");
    duk_push_string(ctx, IsWindowVisible(hwnd) > 0 ? "visible" : "hidden");
    duk_put_prop(ctx, task_bar_obj_idx);

    ZeroMemory(buf, 500);
    GetClassName(hwnd, buf, sizeof buf);
    duk_push_string(ctx, "className");
    duk_push_string(ctx, buf);
    duk_put_prop(ctx, task_bar_obj_idx);

    ZeroMemory(buf, 500);
    GetWindowText(hwnd, buf, sizeof buf);
    duk_push_string(ctx, "title");
    duk_push_string(ctx, buf);
    duk_put_prop(ctx, task_bar_obj_idx);

    if(GetWindowRect(hwnd, &rect))
    {
        duk_push_string(ctx, "x");
        duk_push_number(ctx, rect.left);
        duk_put_prop(ctx, task_bar_obj_idx);

        duk_push_string(ctx, "y");
        duk_push_number(ctx, rect.top);
        duk_put_prop(ctx, task_bar_obj_idx);

        int width = rect.right - rect.left;
        duk_push_string(ctx, "width");
        duk_push_number(ctx, width);
        duk_put_prop(ctx, task_bar_obj_idx);

        int height = rect.bottom - rect.top;
        duk_push_string(ctx, "height");
        duk_push_number(ctx, height);
        duk_put_prop(ctx, task_bar_obj_idx);
    }

    return 1;
}

void
jduk_set_task_bar_attributes_visibility(duk_context *ctx, duk_idx_t attributes_idx, HWND hwnd) {
    duk_get_prop_string(ctx, attributes_idx, "visibility");
    const char *visibility = duk_to_string(ctx, -1);
    duk_bool_t hidden = strcmp(visibility, "hidden") == 0;
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, (hidden ? SWP_HIDEWINDOW : SWP_SHOWWINDOW ) | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    duk_pop(ctx);
}

duk_ret_t
jduk_set_task_bar_attributes(duk_context *ctx) {
    duk_idx_t window_id_idx = 0;
    duk_int32_t window_id = duk_to_number(ctx, window_id_idx);
    HWND hwnd = (HWND)(LONG_PTR)window_id;

    duk_idx_t attributes_idx = -1;
    if (duk_is_null_or_undefined(ctx, attributes_idx) || !duk_is_object(ctx, attributes_idx)) {
        duk_pop(ctx);
        return 0;
    }

    if (duk_has_prop_string(ctx, attributes_idx, "visibility")) {
        jduk_set_task_bar_attributes_visibility(ctx, attributes_idx, hwnd);
    }

    duk_pop(ctx);
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

duk_ret_t
jduk_bar_finalizer(duk_context *ctx) {
    duk_get_prop_string(ctx, -1, "barState");
    BarState *barState = (BarState*)duk_to_pointer(ctx, -1);

    if (barState) {
        free(barState);
        barState = NULL;
    }

    return 0;
}

duk_ret_t
jduk_bar_get_attributes(duk_context *ctx) {
    duk_push_this(ctx);

    duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL("id"));
    duk_int32_t id = duk_to_int(ctx, -1);
    duk_idx_t result_obj_idx = duk_push_object(ctx);

    duk_push_literal(ctx, "id");
    duk_push_int(ctx, id);
    duk_put_prop(ctx, result_obj_idx);

    duk_get_prop_string(ctx, -1, "barState");
    BarState *barState = (BarState*)duk_to_pointer(ctx, -1);

    HWND hwnd = barState->barhwnd;
    RECT rect;
    WINDOWINFO window_info = { .cbSize = sizeof window_info };

    duk_idx_t window_obj_idx = duk_push_object(ctx);

    int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    duk_push_string(ctx, "visibility");
    duk_push_string(ctx, IsWindowVisible(hwnd) > 0 ? "visible" : "hidden");
    duk_put_prop(ctx, window_obj_idx);

    if (GetWindowInfo(hwnd, &window_info)) {
        duk_push_string(ctx, "isActive");
        duk_push_boolean(ctx, window_info.dwWindowStatus > 0);
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

duk_ret_t
jduk_bar_ctor(duk_context *ctx) {
    if (!duk_is_constructor_call(ctx)) {
        return DUK_RET_TYPE_ERROR;
    }

    duk_idx_t attributes_idx = -1;
    if (duk_is_null_or_undefined(ctx, attributes_idx) || !duk_is_object(ctx, attributes_idx)) {
        MessageBox(NULL, "bar.ctor null", "dwmjs: Info", MB_OK);
        duk_pop(ctx);
        return 0;
    }

    BarState *barState = (BarState*)malloc(sizeof(BarState));
    if (!barState) {
        die("Failed to allocate memory to create barState");
    }

    const char *font = "Consolas";
    const char *underlineColor = "#ffffff";
    duk_int32_t underlineWidth = 1;
    const char *backgroundColor = "#ffffff";
    const char *foregroundColor = "#ffffff";
    const char *visibility = "visible";

    if (duk_has_prop_string(ctx, attributes_idx, "x")) {
        duk_get_prop_string(ctx, attributes_idx, "x");
        barState->x = duk_to_int32(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "y")) {
        duk_get_prop_string(ctx, attributes_idx, "y");
        barState->y = duk_to_int32(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "width")) {
        duk_get_prop_string(ctx, attributes_idx, "width");
        barState->width = duk_to_int32(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "height")) {
        duk_get_prop_string(ctx, attributes_idx, "height");
        barState->height = duk_to_int32(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "font")) {
        duk_get_prop_string(ctx, attributes_idx, "font");
        font = duk_to_string(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "backgroundColor")) {
        duk_get_prop_string(ctx, attributes_idx, "backgroundColor");
        backgroundColor = duk_to_string(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "foregroundColor")) {
        duk_get_prop_string(ctx, attributes_idx, "foregroundColor");
        foregroundColor = duk_to_string(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "underlineColor")) {
        duk_get_prop_string(ctx, attributes_idx, "underlineColor");
        underlineColor = duk_to_string(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "underlineWidth")) {
        duk_get_prop_string(ctx, attributes_idx, "underlineWidth");
        underlineWidth = duk_to_int32(ctx, -1);
        duk_pop(ctx);
    }

    if (duk_has_prop_string(ctx, attributes_idx, "visibility")) {
        duk_get_prop_string(ctx, attributes_idx, "visibility");
        visibility = duk_to_string(ctx, -1);
        duk_pop(ctx);
    }

    setupbar(hinstance);

    HWND barhwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        BARNAME,
        NULL, /* window title */
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        NULL, /* parent window */
        NULL, /* menu */
        hinstance,
        barState
    );

    if (!barhwnd) {
        free(barState);
        die("Failed to create bar");
    }

    duk_bool_t hidden = strcmp(visibility, "hidden") == 0;
    SetWindowPos(barhwnd, 0, 0, 0, 0, 0, (hidden ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    barState->barhwnd = barhwnd;

    duk_push_this(ctx);

    duk_idx_t finalizer_obj_id = duk_push_object(ctx);
    duk_push_pointer(ctx, barState);
    duk_put_prop_string(ctx, finalizer_obj_id, "barState");

    duk_push_c_function(ctx, jduk_bar_finalizer, 1);
    duk_set_finalizer(ctx, -2);

    duk_push_c_function(ctx, jduk_bar_get_attributes, 0);
    duk_set_finalizer(ctx, -2);

    duk_push_int(ctx, (long)barhwnd);
    duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("id"));

    CREATESTRUCT *CrtStrPtr = malloc(sizeof(CREATESTRUCT));
    CrtStrPtr->lpCreateParams = barState;
    PostMessage(barhwnd, WM_PAINT, 0, CrtStrPtr);

    return 1;
}

duk_ret_t
jduk_bar_set_attributes(duk_context *ctx) {
    duk_push_this(ctx);

    duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL("id"));
    duk_int32_t id = duk_to_int(ctx, -1);

    // TODO

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

    duk_push_c_lightfunc(ctx, jduk_get_task_bar, 0, 0, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "getTaskBar");

    duk_push_c_lightfunc(ctx, jduk_set_task_bar_attributes, 2, 2, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "setTaskBarAttributes");

    duk_push_c_function(ctx, jduk_bar_ctor, 1); // var bar = new dwmjs.Bar();
    duk_push_object(ctx); // [Bar proto]

    duk_push_c_lightfunc(ctx, jduk_bar_get_attributes, 0, 0, 0);
    duk_put_prop_string(ctx, -2, "getAttributes");

    duk_push_c_function(ctx, jduk_bar_set_attributes, 1);
    duk_put_prop_string(ctx, -2, "setAttributes");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_prop_string(ctx, dwmjs_obj_id, "Bar");

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
