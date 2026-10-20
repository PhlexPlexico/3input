#pragma once
#include <3ds.h>

/*
    ZL/ZR and C-Stick, read out of the (patched, see code.ips) IR module.

    The patched IR module copies the latest ZL/ZR/C-Stick key word into its own
    memory every time its ir:rst sampling thread runs, and exposes the physical
    address of that word through ir:u (the same hook rehid uses). We resolve
    the address once and from then on a key read is a single memory load.

    3Input is a passive reader: it never initialises, shuts down or holds an
    ir:rst session. The sampling thread is driven by whoever really needs
    ir:rst (a game, or the system HID module), so a game can always request it
    and keep it, and we get its keys for free while it runs. Taking ir:rst over
    ourselves is what used to keep games from getting the extra buttons.
    The consequence is that the word reads 0 whenever nothing is sampling.

    Build with -DIRKEYS_HOLD_SESSION to get the old behaviour back: we hold one
    ir:rst session ourselves so the keys stay live even when no game uses
    ir:rst, stepping aside while a game owns ir:USER (the two are mutually
    exclusive, see https://www.3dbrew.org/wiki/IR_Services).
*/

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
