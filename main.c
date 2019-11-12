#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#if _MSC_VER
#pragma comment(lib,"user32.lib")
#endif

#include <windows.h>
#include <stdlib.h>
#include "duktape.h"

static duk_context *duk_ctx;
void cleanup();
void die(const char *fmt, ...);

void
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

void
cleanup() {
    if (duk_ctx) {
        duk_destroy_heap(duk_ctx);
        duk_ctx = NULL;
    }
}

static duk_ret_t
jduk_init_context(duk_context *ctx, void *udata) {
    duk_push_global_object(ctx);
    duk_pop(ctx);
    return 0;
}

int
WINAPI WinMain(HINSTANCE hInstance,
               HINSTANCE hPrevInstance,
               LPSTR lpCmdLine,
               int nShowCmd) {
    duk_context *ctx = duk_create_heap_default();
    if (!ctx) {
        die("Failed to create duktape context");
        return EXIT_FAILURE;
    }

    if (duk_safe_call(ctx, jduk_init_context, NULL, 0, 1) != 0) {
        die("Failed to initialize duktape script");
        return EXIT_FAILURE;
    }

    MessageBox(NULL, "Hello world", "Greeting", MB_OK);
    cleanup();
    return EXIT_SUCCESS;
}
