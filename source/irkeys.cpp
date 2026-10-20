#include "irkeys.hpp"

#include "protocol.h"

namespace {

// ir:u commands. 7 and 8 do not exist on a stock IR module, they are added by
// code.ips and return a physical address in the first result word.
const u32 IRU_INITIALIZE   = IPC_MakeHeader(0x1, 0, 0);
const u32 IRU_SHUTDOWN     = IPC_MakeHeader(0x2, 0, 0);
const u32 IRU_GET_KEYS_PA  = IPC_MakeHeader(0x7, 0, 0);
const u32 IRRST_GET_HANDLES = IPC_MakeHeader(0x1, 0, 0);
#ifdef IRKEYS_HOLD_SESSION
const u32 IRU_GET_STATE_PA = IPC_MakeHeader(0x8, 0, 0);

// ir:rst commands, https://www.3dbrew.org/wiki/IR_Services
const u32 IRRST_INITIALIZE  = IPC_MakeHeader(0x2, 2, 0);
const u32 IRRST_SHUTDOWN    = IPC_MakeHeader(0x3, 0, 0);
#endif

constexpr u32 ZL_ALT = 0x10000;
constexpr u32 ZR_ALT = 0x20000;

constexpr u64 RETRY_TICKS = SYSCLOCK_ARM11; // ~1 second

// ir:rst shared memory, https://www.3dbrew.org/wiki/IRRST_Shared_Memory
struct IrrstEntry {
    s32 pad, pressed, released;
    s16 x, y; // C-Stick
};
struct IrrstShared {
    s64 tick, tickPrev;
    s32 index;
    u32 unk14;
    IrrstEntry entries[8];
};
static_assert(sizeof(IrrstShared) == 0x98, "IrrstShared layout");

#ifndef IRKEYS_HOLD_SESSION
constexpr u64 CSTICK_RETRY_TICKS = SYSCLOCK_ARM11 * 5;
#endif

enum class Resolve { Pending, Done, Unsupported };

Resolve resolved = Resolve::Pending;
u64 retryAt = 0;
u8  status = IR_PENDING;
u32 lastError = 0;

u8  cstickStatus = CS_PENDING;
u32 cstickError = 0;
volatile const IrrstShared* cstickMem = nullptr;
Handle cstickHandle = 0;
#ifndef IRKEYS_HOLD_SESSION
u64 cstickRetryAt = 0;
#endif

// Points into the IR module, via the physical-address window (bit 31).
volatile const u32* keysWord = nullptr;

/// Sends a request that carries no parameters and returns its result code.
Result simpleRequest(Handle session, u32 header) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = header;
    Result res = svcSendSyncRequest(session);
    return R_FAILED(res) ? res : static_cast<Result>(cmd[1]);
}

/// The patched ir:u commands answer with the address in place of a result code.
Result addressRequest(Handle session, u32 header, u32* pa) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = header;
    Result res = svcSendSyncRequest(session);
    if (R_FAILED(res))
        return res;
    *pa = cmd[1];
    return 0;
}

/// A physical address handed back by an unpatched module is either 0 or an
/// error code (top bit set). Never dereference those.
bool plausiblePA(u32 pa) {
    return pa != 0 && !(pa & 0x80000000u);
}

template <typename T>
volatile const T* mapPA(u32 pa) {
    return reinterpret_cast<volatile const T*>(pa | 0x80000000u);
}

#ifdef IRKEYS_HOLD_SESSION
volatile const u8* stateByte = nullptr;
Handle rstSession = 0;
#endif

/// Maps an ir:rst shared memory block (takes ownership of `mem`).
bool mapBlock(Handle mem) {
    void* addr = mappableAlloc(sizeof(IrrstShared));
    Result res = addr ? svcMapMemoryBlock(mem, reinterpret_cast<u32>(addr), MEMPERM_READ, MEMPERM_DONTCARE) : -1;
    if (R_FAILED(res)) {
        if (addr)
            mappableFree(addr);
        svcCloseHandle(mem);
        cstickStatus = CS_MAP_FAILED;
        cstickError = res;
        return false;
    }
    cstickMem = static_cast<volatile const IrrstShared*>(addr);
    cstickHandle = mem;
    cstickStatus = CS_OK;
    return true;
}

void unmapBlock() {
    if (!cstickMem)
        return;
    svcUnmapMemoryBlock(cstickHandle, reinterpret_cast<u32>(cstickMem));
    mappableFree(const_cast<IrrstShared*>(cstickMem));
    svcCloseHandle(cstickHandle);
    cstickMem = nullptr;
    cstickHandle = 0;
}

#ifndef IRKEYS_HOLD_SESSION
/// Asks a throwaway ir:rst session for its shared memory block and maps it,
/// leaving no session open. The block is the module's single global ring, so
/// it stays valid after the session is closed. The patched hook (code.ips)
/// keeps the newest entry's C-Stick X/Y current.
bool mapCstickMemory() {
    Handle session;
    Result res = srvGetServiceHandle(&session, "ir:rst");
    if (R_FAILED(res)) {
        cstickStatus = CS_SERVICE_FAILED;
        cstickError = res;
        return false;
    }

    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IRRST_GET_HANDLES;
    res = svcSendSyncRequest(session);
    if (R_SUCCEEDED(res))
        res = cmd[1];
    Handle mem = 0, event = 0;
    if (R_SUCCEEDED(res)) {
        mem = cmd[3];
        event = cmd[4];
    }
    svcCloseHandle(session);
    if (event)
        svcCloseHandle(event);
    if (R_FAILED(res)) {
        cstickStatus = CS_HANDLES_FAILED;
        cstickError = res;
        return false;
    }
    if (!mem) {
        cstickStatus = CS_NO_MEMORY;
        cstickError = 0;
        return false;
    }
    return mapBlock(mem);
}
#endif

/// Opens ir:u just long enough to ask for the address, so a game that wants
/// ir:u is never kept waiting on us.
void resolveAddresses() {
    bool registered = false;
    if (R_FAILED(srvIsServiceRegistered(&registered, "ir:u")) || !registered)
        return;

    Handle iru;
    Result res = srvGetServiceHandle(&iru, "ir:u");
    if (R_FAILED(res)) {
        lastError = res;
        return;
    }

    res = simpleRequest(iru, IRU_INITIALIZE);
    if (R_FAILED(res)) {
        lastError = res;
    } else {
        u32 keysPA = 0;
        Result a = addressRequest(iru, IRU_GET_KEYS_PA, &keysPA);
        bool ok = R_SUCCEEDED(a) && plausiblePA(keysPA);
#ifdef IRKEYS_HOLD_SESSION
        u32 statePA = 0;
        Result b = addressRequest(iru, IRU_GET_STATE_PA, &statePA);
        ok = ok && R_SUCCEEDED(b) && plausiblePA(statePA);
        if (ok)
            stateByte = mapPA<u8>(statePA);
#endif
        simpleRequest(iru, IRU_SHUTDOWN);

        if (R_FAILED(a)) {
            lastError = a;
        } else {
            if (ok) {
                keysWord = mapPA<u32>(keysPA);
                resolved = Resolve::Done;
                status = IR_PASSIVE;
            } else {
                // Talking to a stock IR module. Retrying will not change that
                // and would only keep poking ir:u.
                resolved = Resolve::Unsupported;
                status = IR_UNPATCHED;
                lastError = keysPA;
            }
        }
    }
    svcCloseHandle(iru);
}

#ifdef IRKEYS_HOLD_SESSION
bool acquireRst() {
    Handle session;
    Result res = srvGetServiceHandle(&session, "ir:rst");
    if (R_FAILED(res)) {
        lastError = res;
        return false;
    }

    // GetHandles returns this session's shared memory block, which the module
    // only keeps updated once the session is initialised (below). Keep it and
    // map it for the analog C-Stick.
    Handle mem = 0;
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IRRST_GET_HANDLES;
    res = svcSendSyncRequest(session);
    if (R_SUCCEEDED(res))
        res = cmd[1];
    if (R_SUCCEEDED(res)) {
        mem = cmd[3];
        if (cmd[4]) svcCloseHandle(cmd[4]);

        cmd = getThreadCommandBuffer();
        cmd[0] = IRRST_INITIALIZE;
        cmd[1] = 10;
        cmd[2] = 0;
        res = svcSendSyncRequest(session);
        if (R_SUCCEEDED(res))
            res = cmd[1];
    }

    if (R_FAILED(res)) {
        lastError = res;
        cstickStatus = CS_HANDLES_FAILED;
        cstickError = res;
        if (mem)
            svcCloseHandle(mem);
        svcCloseHandle(session);
        return false;
    }
    rstSession = session;
    if (mem)
        mapBlock(mem);
    else
        cstickStatus = CS_NO_MEMORY;
    return true;
}

void releaseRst() {
    if (!rstSession)
        return;
    simpleRequest(rstSession, IRRST_SHUTDOWN);
    svcCloseHandle(rstSession);
    rstSession = 0;
    unmapBlock();
}
#endif

} // namespace

void irkeysPoll(void) {
#ifndef IRKEYS_HOLD_SESSION
    if (!cstickMem) {
        u64 now = svcGetSystemTick();
        if (now >= cstickRetryAt && !mapCstickMemory())
            cstickRetryAt = now + CSTICK_RETRY_TICKS;
    }
#endif

    if (resolved == Resolve::Unsupported)
        return;

    if (resolved == Resolve::Pending) {
        u64 now = svcGetSystemTick();
        if (now < retryAt)
            return;
        // Blocking here would hang the whole input loop while a game holds ir:u.
        srvSetBlockingPolicy(false);
        resolveAddresses();
        if (resolved != Resolve::Done) {
            retryAt = now + RETRY_TICKS;
            return;
        }
    }

#ifdef IRKEYS_HOLD_SESSION
    if (*stateByte) {
        // A game owns ir:USER: step aside.
        releaseRst();
        status = IR_YIELDED;
    } else if (!rstSession) {
        u64 now = svcGetSystemTick();
        if (now >= retryAt) {
            if (acquireRst()) {
                status = IR_HOLDING;
            } else {
                status = IR_HOLD_FAILED;
                retryAt = now + RETRY_TICKS;
            }
        }
    }
#endif
}

// Same as libctru's hidScanInput: an out-of-range index means the last entry.
static u32 cstickIndex() {
    u32 i = static_cast<u32>(cstickMem->index);
    return i > 7 ? 7 : i;
}

void irkeysCstick(s16* x, s16* y) {
    *x = *y = 0;
    if (!cstickMem)
        return;
    u32 i = cstickIndex();
    *x = cstickMem->entries[i].x;
    *y = cstickMem->entries[i].y;
}

void irkeysCstickInfo(u8* status, u32* detail) {
    *status = cstickStatus;
    *detail = cstickMem ? static_cast<u32>(cstickMem->entries[cstickIndex()].pad) : cstickError;
}

u8 irkeysStatus(void) {
    return status;
}

u32 irkeysError(void) {
    return lastError;
}

u32 irkeysRead(void) {
    if (!keysWord)
        return 0;
    u32 keys = *keysWord;

    // rehid's version of the patch reports ZL/ZR as bits 16/17 of this word so
    // they stay out of what apps read from ir:rst. Fold them back to KEY_ZL and
    // KEY_ZR (rehid's irSampling does the same). A no-op for the older patch,
    // which sets them directly.
    if (keys & ZL_ALT) keys = (keys & ~ZL_ALT) | KEY_ZL;
    if (keys & ZR_ALT) keys = (keys & ~ZR_ALT) | KEY_ZR;
    return keys;
}

void irkeysExit(void) {
#ifdef IRKEYS_HOLD_SESSION
    releaseRst();
#endif
    unmapBlock();
}
