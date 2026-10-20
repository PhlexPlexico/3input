#pragma once
/*
    Wire format sent to connected clients (TCP, port INPUT_PORT).

    The stream is a plain sequence of fixed-size, little-endian InputPacket
    records with no delimiters or padding. A client reads INPUT_PACKET_SIZE
    bytes at a time; if it ever loses track it can resync by scanning for the
    two magic bytes.

    `buttons` uses the libctru KEY_* bit layout, with ZL/ZR and the C-stick
    already merged in:
        bit  0 A        bit  8 R        bit 14 ZL        bit 24 C-Stick Right
        bit  1 B        bit  9 L        bit 15 ZR        bit 25 C-Stick Left
        bit  2 Select   bit 10 X        bit 28 CPad Right bit 26 C-Stick Up
        bit  3 Start    bit 11 Y        bit 29 CPad Left  bit 27 C-Stick Down
        bit  4 Right    bit 12/13 GPIO  bit 30 CPad Up
        bit  5 Left                     bit 31 CPad Down
        bit  6 Up
        bit  7 Down
        bit 16 HOME (pulse, ~100 ms per press)
    `ir_status`/`ir_error` and `cs_status`/`cs_error` report how the ZL/ZR/C-Stick
    keys (IrStatus) and the analog C-Stick (CsStatus) paths are doing.
    `cp_*` is the circle pad and `cs_*` the C-Stick (both about +-156), `tp_*` the touch screen in pixels
    (0,0 when the screen is not being touched).
*/
#include <stdint.h>

#define INPUT_PORT         65534
#define INPUT_MAGIC0       'I'
#define INPUT_MAGIC1       '3'
#define INPUT_VERSION      4

/// HOME button, reported in `buttons` (not part of the 3DS pad state). The system
/// only announces presses, so this is a short pulse (~100 ms) per press.
#define KEY_HOME_BIT       (1u << 16)

/// Where the ZL/ZR/C-Stick path (irkeys.cpp) currently stands, for debugging.
enum IrStatus {
    IR_PENDING     = 0, ///< Patched ir:u not reached yet (not registered, busy, call failed). ir_error has the last Result.
    IR_UNPATCHED   = 1, ///< ir:u answered but the IR module is not patched (code.ips missing/not applied). ir_error is the bogus address it returned.
    IR_PASSIVE     = 2, ///< Resolved; reading the key word without owning ir:rst. Keys are live only while something else runs ir:rst.
    IR_HOLDING     = 3, ///< Resolved; we hold an ir:rst session (IRKEYS_HOLD_SESSION build).
    IR_YIELDED     = 4, ///< Hold build: the IR module's state byte (0x1132C9, "initalized" in patch.s) was set, so we released ir:rst. Its exact meaning (ir:USER vs ir:rst in use) is unconfirmed.
    IR_HOLD_FAILED = 5, ///< Hold build: could not get/initialise an ir:rst session. ir_error has the Result.
};

/// State of the analog C-Stick path (irkeys.cpp maps ir:rst's shared memory).
enum CsStatus {
    CS_PENDING         = 0, ///< Not tried yet.
    CS_OK              = 1, ///< Mapped. cs_error holds the pad word of the newest ring entry (raw; only updated by the sampling thread, so it can be stale).
    CS_SERVICE_FAILED  = 2, ///< Could not open ir:rst. cs_error is the Result (session limit reached?).
    CS_HANDLES_FAILED  = 3, ///< ir:rst GetHandles failed. cs_error is the Result.
    CS_NO_MEMORY       = 4, ///< GetHandles succeeded but returned no shared memory handle.
    CS_MAP_FAILED      = 5, ///< Mapping the block failed. cs_error is the Result.
};

struct __attribute__((packed)) InputPacket {
    uint8_t  magic[2]; ///< INPUT_MAGIC0, INPUT_MAGIC1
    uint8_t  version;  ///< INPUT_VERSION
    uint8_t  seq;      ///< Wrapping frame counter, lets clients spot dropped frames.
    uint32_t buttons;
    int16_t  cp_x;
    int16_t  cp_y;
    int16_t  tp_x;
    int16_t  tp_y;
    int16_t  cs_x;        ///< C-Stick (New 3DS), same range and orientation as cp_*.
    int16_t  cs_y;
    uint8_t  ir_status;   ///< IrStatus
    uint8_t  cs_status;   ///< CsStatus
    uint8_t  reserved[2]; ///< Zero.
    uint32_t ir_error;    ///< Detail for ir_status, see IrStatus.
    uint32_t cs_error;    ///< Detail for cs_status, see CsStatus.
};

#define INPUT_PACKET_SIZE 32
static_assert(sizeof(InputPacket) == INPUT_PACKET_SIZE, "InputPacket must be 32 bytes");
