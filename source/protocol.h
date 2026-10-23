#pragma once
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

/// Wi-Fi keep-alive (wifi.cpp).
enum WifiStatus {
    WIFI_OFF          = 0, ///< Not reported (modules before the keep-alive existed always send 0).
    WIFI_PENDING      = 1, ///< Not checked yet.
    WIFI_HOLDING      = 2, ///< Link is up and we hold a connection request of our own.
    WIFI_RECONNECTING = 3, ///< Link is down, turning the radio on and reconnecting.
    WIFI_GAVE_UP      = 4, ///< Link stayed down after repeated attempts, waiting for it to return on its own.
    WIFI_AC_FAILED    = 5, ///< Could not open the ac service.
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
    uint8_t  wifi_status;     ///< WifiStatus
    uint8_t  wifi_recoveries; ///< Times the link came back after a drop (saturates at 255).
    uint32_t ir_error;    ///< Detail for ir_status, see IrStatus.
    uint32_t cs_error;    ///< Detail for cs_status, see CsStatus.
};

#define INPUT_PACKET_SIZE 32
static_assert(sizeof(InputPacket) == INPUT_PACKET_SIZE, "InputPacket must be 32 bytes");
