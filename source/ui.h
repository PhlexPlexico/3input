#ifndef CONSOLE_H
#define CONSOLE_H

#include <3ds.h>

#include <stdarg.h>

/**
 * @brief Initializes a console and a LightLock for it.
*/
void console_init();

/**
 * @brief Exits a console, unlocks the LightLock.
*/
void console_exit();

/**
 * @brief Prints on the top screen.
 * @param fmt a string to print.
*/
__attribute__((format(printf,1,2)))
int printf_top(const char*, ...);

/**
 * @brief Prints on the bottom screen.
 * @param fmt a string to print.
*/
__attribute__((format(printf,1,2)))
int printf_bottom(const char*, ...);

/**
 * @brief Prints on a given console.
 * @param window A pointer to a PrintConsole window..
 * @param fmt a string to print.
*/
__attribute__((format(printf,2,3)))
int printf_window(PrintConsole*, const char*, ...);

/**
 * Puts a lock on the window and prints the message. Returns success of vprintf.
 * @param window a PrintConsole pointer.
 * @param fmt String to print on console.
 * @param args a list of args to send to vprintf.
*/
int vprintf_window(PrintConsole*, const char*, va_list);

/**
 * @brief Clears the top window.
*/
void clear_top();

/**
 * @brief Clears the bottom window.
*/
void clear_bottom();

/**
 * @brief Clears the given window.
 * @param window a PrintConsole pointer to a window.
*/
void clear_window(PrintConsole*);


#endif