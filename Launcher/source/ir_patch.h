#ifndef IR_PATCH_H
#define IR_PATCH_H

#include <3ds.h>
#include <stdbool.h>

/// Where Luma looks for a code patch for the IR module (title 0004013000003302).
#define IR_PATCH_PATH "sdmc:/luma/sysmodules/0004013000003302.ips"

typedef enum {
    PATCH_PRESENT,           ///< Already on the SD card, same as the bundled copy.
    PATCH_PRESENT_DIFFERENT, ///< On the SD card but not identical to the bundled copy (left alone).
    PATCH_INSTALLED,         ///< Was missing, written just now. A reboot is needed for it to load.
    PATCH_FAILED,            ///< Could not install, see the errno value.
} PatchState;

/**
 * @brief Makes sure the bundled code.ips (romfs:/code.ips) is on the SD card,
 * creating /luma/sysmodules if needed. An existing file is never overwritten.
 * @param err Receives errno when the result is PATCH_FAILED.
 */
PatchState irPatchEnsureInstalled(int *err);

typedef enum {
    CPP_ABSENT,
    CPP_PRESENT,
    CPP_UNKNOWN, ///< Could not talk to ir:USER, no answer either way.
} CppState;

/**
 * @brief Checks whether a Circle Pad Pro is attached by opening a connection to
 * it through ir:USER. Blocks for up to about two seconds when nothing is attached.
 */
CppState cppDetect(void);

#endif
