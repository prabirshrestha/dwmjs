#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#if _MSC_VER
#pragma comment(lib,"user32.lib")
#endif

#include <windows.h>
#include <stdlib.h>
#include "duktape.h"

#define NAME					"dwmjs" 	/* Used for window name/class */
static HWND dwmhwnd;
static duk_context *duk_ctx;
static void cleanup();
static void die(const char *fmt, ...);
static void setup(HINSTANCE hInstance);
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
cleanup() {
    if (duk_ctx) {
        duk_destroy_heap(duk_ctx);
        duk_ctx = NULL;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE:
			break;
        case WM_CLOSE:
			cleanup();
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
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
}

duk_ret_t
jduk_alert(duk_context *ctx) {
    MessageBox(NULL, (char*)duk_to_string(ctx, -1), "dwmjs: Info", MB_OK);
    duk_pop(ctx);
    return 0;
}

duk_ret_t
jduk_post_message(duk_context *ctx) {
    MessageBox(NULL, (char*)duk_to_string(ctx, -1), "dwmjs: post message", MB_OK);
    duk_pop(ctx);
    return 0;
}

#define EVENT_LISTENER_JS_CODE "(function(dwmjs) {"\
    "var listeners = {};"\
    "dwmjs.__onmessage = function (type, data) {"\
    "   var callbacks = listeners[type];"\
    "   if (callbacks) {"\
    "       for (var i = 0; i < callbacks.lenght; i++) {"\
    "           callbacks[i](data);"\
    "       }"\
    "   }"\
    "};"\
    "dwmjs.addEventListener = function (type, callback) {"\
    "   var callbacks = listeners[event];"\
    "    if (!callbacks) { callbacks = listeners[event] = []; }"\
    "    callbacks.push(callback);"\
    "};"\
    "dwmjs.removeEventListener = function (type, callback) {"\
    "    var callbacks = listeners[event];"\
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

    duk_push_c_lightfunc(ctx, jduk_post_message, 1, 1, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "postMessage");

    duk_put_global_string(ctx, "dwmjs");

    duk_push_string(ctx, EVENT_LISTENER_JS_CODE);
    duk_eval(ctx);

    char *evalstr = "alert(JSON.stringify(!!dwmjs.addEventListener))";
    if (duk_peval_string(ctx, evalstr) != 0) {
        die("failed");
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

    duk_context *ctx = duk_create_heap_default();
    if (!ctx) {
        die("Failed to create duktape context");
        return EXIT_FAILURE;
    }

    if (duk_safe_call(ctx, jduk_init_context, NULL, 0, 1) != 0) {
        die("Failed to initialize duktape script");
        return EXIT_FAILURE;
    }

    setup(hInstance);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    cleanup();
    return EXIT_SUCCESS;
}
