#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#if _MSC_VER
#pragma comment(lib,"user32.lib")
#endif

#include <windows.h>
#include <stdlib.h>
#include "duktape.h"

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
        MessageBox(NULL, "Failed to create duktape context", "Error", MB_OK);
        return EXIT_FAILURE;
    }

    if (duk_safe_call(ctx, jduk_init_context, NULL, 0, 1) != 0) {
        MessageBox(NULL, "Failed to initialize duktape script", "Error", MB_OK);
        duk_destroy_heap(ctx);
        ctx = NULL;
        return EXIT_FAILURE;
    }

    MessageBox(NULL, "Hello world", "Greeting", MB_OK);
    duk_destroy_heap(ctx);
    ctx = NULL;
    return EXIT_SUCCESS;
}
