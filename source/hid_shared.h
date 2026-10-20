#pragma once
/*
    Layout of the HID shared memory block, see
    https://www.3dbrew.org/wiki/HID_Shared_Memory
*/
#include <stddef.h>
#include <stdint.h>

#define HID_RING_SIZE 8

struct HidPadEntry {
    uint32_t curr;     ///< Buttons currently held (KEY_* bits).
    uint32_t pressed;  ///< Newly pressed since the previous entry.
    uint32_t released; ///< Newly released since the previous entry.
    int16_t  cp_x;     ///< Circle pad.
    int16_t  cp_y;
};

struct HidPad {
    uint64_t tick;      // 0x00
    uint64_t tick_prev; // 0x08
    uint32_t index;     // 0x10 - index of the most recently written entry
    uint32_t unk14;     // 0x14
    uint32_t slider;    // 0x18 - 3D slider
    uint32_t raw_pad;   // 0x1C
    uint32_t raw_cp;    // 0x20
    uint32_t unk24;     // 0x24
    HidPadEntry entries[HID_RING_SIZE]; // 0x28
};

struct HidTouchEntry {
    int16_t  x;
    int16_t  y;
    uint32_t valid;
};

struct HidTouch {
    uint64_t tick;      // 0x00
    uint64_t tick_prev; // 0x08
    uint32_t index;     // 0x10
    uint32_t unk14;     // 0x14
    HidTouchEntry raw;  // 0x18
    HidTouchEntry entries[HID_RING_SIZE]; // 0x20
};

struct HidShared {
    HidPad   pad;   // 0x00
    HidTouch touch; // 0xA8
};

static_assert(sizeof(HidPadEntry) == 0x10, "HidPadEntry layout");
static_assert(offsetof(HidPad, entries) == 0x28, "HidPad layout");
static_assert(sizeof(HidPad) == 0xA8, "HidPad layout");
static_assert(offsetof(HidTouch, entries) == 0x20, "HidTouch layout");
static_assert(sizeof(HidTouch) == 0x60, "HidTouch layout");
static_assert(offsetof(HidShared, touch) == 0xA8, "HidShared layout");
