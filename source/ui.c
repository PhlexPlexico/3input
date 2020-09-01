#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

#define NO_UI

static PrintConsole top, bottom;
static PrintConsole* active_console = NULL;

LightLock console_mutex;

void console_init() {

#ifdef NO_UI
    gfxInitDefault();

	atexit(console_exit);

    consoleInit(GFX_BOTTOM, &bottom);
    consoleInit(GFX_TOP, &top);


    active_console = &top;

    LightLock_Init(&console_mutex);
#endif
}

void console_exit() {
#ifdef NO_UI
    LightLock_Unlock(&console_mutex);
    gfxExit();
#endif
}


void console_lock() {
#ifdef NO_UI
    LightLock_Lock(&console_mutex);
#endif
}
void console_unlock() {
#ifdef NO_UI
    LightLock_Unlock(&console_mutex);
#endif
}

void window_select(PrintConsole* console) {
#ifdef NO_UI
    if(console != active_console) {
        gfxFlushBuffers();
        consoleSelect(console);
        active_console = console;
    }
#endif
}

int printf_top(const char* fmt, ...) {
#ifdef NO_UI
    va_list ap;
    int ret;

    va_start(ap, fmt);
	ret = vprintf_window(&top, fmt, ap);
	va_end(ap);

    return ret;
#else
    return 0;
#endif
}

int printf_bottom(const char* fmt, ...) {
#ifdef NO_UI
    va_list ap;
    int ret;

    va_start(ap, fmt);
	ret = vprintf_window(&bottom, fmt, ap);
	va_end(ap);

    return ret;
#else
    return 0;
#endif
}
int printf_window(PrintConsole* console, const char* fmt, ...) {
#ifdef NO_UI
    va_list ap;
    int ret;

    va_start(ap, fmt);
	ret = vprintf_window(console, fmt, ap);
	va_end(ap);

    return ret;
#else
    return 0
#endif
}
int vprintf_window(PrintConsole* console, const char* fmt, va_list args) {
#ifdef NO_UI
    int ret;
    
    console_lock();

    window_select(console);
    ret = vprintf(fmt, args);

    console_unlock();

    return ret;
#else
    return 0;
#endif
}

void clear_top() {
    clear_window(&top);
}
void clear_bottom() {
    clear_window(&bottom);
}
void clear_window(PrintConsole* window) {
#ifdef NO_UI
    console_lock();
    
    window_select(window);
    consoleClear();

    console_unlock();
#endif
}
