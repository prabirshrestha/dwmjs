#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#if _MSC_VER
#pragma comment(lib,"user32.lib")
#endif

#include <windows.h>
#include "duktape.h"

duk_context*
vduk_get_context() {
    static duk_context *ctx = NULL;
    if(ctx) return ctx;
    ctx = duk_create_heap_default();

    return ctx;
}
int
WINAPI WinMain(HINSTANCE hInstance,
               HINSTANCE hPrevInstance,
               LPSTR lpCmdLine,
               int nShowCmd) {
    duk_context *ctx = vduk_get_context();
    MessageBox(NULL, "Hello world", "Greeting", MB_OK);
    if (ctx) {
        duk_destroy_heap(ctx);
    }
    return 0;
}
