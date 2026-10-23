#pragma once
#include <3ds.h>

/// Cheap to call every frame. Resolves the address of the key word the first
/// time it can (ir:u may be busy or not registered yet).
void irkeysPoll(void);

/// Latest ZL/ZR/C-Stick bits (KEY_ZL, KEY_ZR, KEY_CSTICK_*), 0 until ready.
/// Works with rehid's code.ips (the one 3Input ships) and the older variant.
u32 irkeysRead(void);

/// Analog C-Stick position (about +-156, 0 when nothing is sampling ir:rst).
/// Read from ir:rst's shared memory, which is mapped read-only and needs no
/// ir:rst session or Initialize call, so games are not affected.
void irkeysCstick(s16* x, s16* y);

/// CsStatus (protocol.h) and its detail word, for diagnostics.
void irkeysCstickInfo(u8* status, u32* detail);

/// IrStatus (protocol.h) and its detail word, for diagnostics.
u8  irkeysStatus(void);
u32 irkeysError(void);

/// Releases anything irkeysPoll acquired.
void irkeysExit(void);
