#ifndef CONSOLE_H
#define CONSOLE_H

#include <3ds.h>

#include <stdarg.h>

void console_init();
void console_exit();

__attribute__((format(printf,1,2)))
int printf_top(const char*, ...);

__attribute__((format(printf,1,2)))
int printf_bottom(const char*, ...);


__attribute__((format(printf,2,3)))
int printf_window(PrintConsole*, const char*, ...);

int vprintf_window(PrintConsole*, const char*, va_list);

void clear_top();
void clear_bottom();
void clear_window(PrintConsole*);


#endif