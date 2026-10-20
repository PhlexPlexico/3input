#include "ir_patch.h"

#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATCH_MAX_SIZE 0x4000

/// Reads a whole (small) file into a malloc'd buffer.
static u8 *readFile(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    u8 *buf = NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len > 0 && len <= PATCH_MAX_SIZE && (buf = malloc(len)) != NULL) {
        if (fread(buf, 1, len, f) == (size_t)len) {
            *size = len;
        } else {
            free(buf);
            buf = NULL;
        }
    }
    fclose(f);
    return buf;
}

PatchState irPatchEnsureInstalled(int *err) {
    PatchState state = PATCH_FAILED;
    u8 *bundled = NULL, *onSd = NULL;
    size_t bundledSize = 0, onSdSize = 0;
    *err = 0;

    if (R_FAILED(romfsInit())) {
        *err = ENOENT;
        return PATCH_FAILED;
    }
    bundled = readFile("romfs:/code.ips", &bundledSize);
    if (!bundled) {
        *err = errno ? errno : ENOENT;
        goto done;
    }

    onSd = readFile(IR_PATCH_PATH, &onSdSize);
    if (onSd) {
        state = (onSdSize == bundledSize && !memcmp(onSd, bundled, bundledSize))
                    ? PATCH_PRESENT : PATCH_PRESENT_DIFFERENT;
        goto done;
    }

    // Missing: create the folders (they may already exist) and write it.
    int mkErr = 0;
    if (mkdir("sdmc:/luma", 0777) && errno != EEXIST)
        mkErr = errno;
    if (mkdir("sdmc:/luma/sysmodules", 0777) && errno != EEXIST)
        mkErr = errno;

    FILE *out = fopen(IR_PATCH_PATH, "wb");
    if (!out) {
        *err = mkErr ? mkErr : errno;
        goto done;
    }
    size_t written = fwrite(bundled, 1, bundledSize, out);
    int closeRes = fclose(out);
    if (written != bundledSize || closeRes != 0) {
        *err = errno ? errno : EIO;
        remove(IR_PATCH_PATH);
        goto done;
    }
    state = PATCH_INSTALLED;

done:
    free(bundled);
    free(onSd);
    romfsExit();
    return state;
}
