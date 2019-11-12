#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#if _MSC_VER
#pragma comment(lib,"user32.lib")
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "duktape.h"

#define NAME					"dwmjs" 	/* Used for window name/class */
#define PATH_SEPERATOR          "\\"
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
            /* dwmjs.__onmessage('load') */
            duk_get_global_string(duk_ctx, "dwmjs");
            duk_push_string(duk_ctx, "__onmessage");
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
jduk_exit(duk_context *ctx) {
    duk_int32_t code = duk_to_int32(ctx, -1);
    cleanup();
    exit(code);
    return 0;
}

duk_ret_t
jduk_call(duk_context *ctx) {
    duk_idx_t argc = duk_get_top(ctx);
    if (argc < 1) {
        MessageBox(NULL, "Invalid number of args provided for dwmjs.call", "dwmjs: call", MB_OK | MB_ICONERROR);
        // TODO: throw error instead
        duk_pop(ctx);
        return -1;
    }

    if(!duk_is_string(ctx, 0)) {
        MessageBox(NULL, "Event name should be string", "dwmjs: call", MB_OK | MB_ICONERROR);
        // TODO: throw error instead
        duk_pop(ctx);
        return -1;
    }

    const char *method = duk_to_string(ctx, 0);
    if (strcmp(method, "exit") == 0 ) {
        cleanup();
        exit(duk_to_int32(ctx, -1));
    } else if (strcmp(method, "greet") == 0) {
        MessageBox(NULL, (char*)duk_to_string(ctx, -1), "dwmjs: greet", MB_OK);
    } else {
        // TODO: unknown method. throw error
        MessageBox(NULL, "Unknown", "dwmjs: call()", MB_OK | ERROR);
    }

    duk_pop(ctx);
    return 0;
}

#define EVENT_LISTENER_JS_CODE "(function(dwmjs) {"\
    "var listeners = {};"\
    "dwmjs.__onmessage = function (type, data) {"\
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

    duk_push_c_lightfunc(ctx, jduk_call, 2, 2, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "call");

    duk_push_c_lightfunc(ctx, jduk_exit, 1, 1, 0);
    duk_put_prop_string(ctx, dwmjs_obj_id, "exit");

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

    setup(hInstance);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    cleanup();
    return EXIT_SUCCESS;
}
