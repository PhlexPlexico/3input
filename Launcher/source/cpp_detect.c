#include "ir_patch.h"

#include <malloc.h>
#include <string.h>

/*
    Circle Pad Pro detection over ir:USER, see
    https://www.3dbrew.org/wiki/IR_Services and
    https://www.3dbrew.org/wiki/IRUSER_Shared_Memory

    Not verified on hardware: the 3dbrew pages do not list the status values,
    the device id and the "connected" value below come from how the Circle Pad
    Pro handshake is commonly implemented (e.g. in Citra's ir:USER).
*/

#define IRUSER_INITIALIZE_SHARED   IPC_MakeHeader(0x18, 6, 2)
#define IRUSER_FINALIZE            IPC_MakeHeader(0x02, 0, 0)
#define IRUSER_REQUIRE_CONNECTION  IPC_MakeHeader(0x06, 1, 0)
#define IRUSER_DISCONNECT          IPC_MakeHeader(0x09, 0, 0)
#define IRUSER_GET_CONN_EVENT      IPC_MakeHeader(0x0C, 0, 0)

#define DEVICE_CIRCLE_PAD_PRO      1
#define BITRATE_CPP                4
#define STATUS_CONNECTION_OFFSET   0x8 // StatusInfo.connection status
#define STATUS_CONNECTED           2

#define SHM_SIZE      0x1000
#define RECV_SIZE     0x400
#define RECV_PACKETS  4
#define SEND_SIZE     0x100
#define SEND_PACKETS  2

#define CONNECT_TIMEOUT_NS 2000000000LL

static Result simple(Handle h, u32 header) {
    u32 *cmd = getThreadCommandBuffer();
    cmd[0] = header;
    Result res = svcSendSyncRequest(h);
    return R_FAILED(res) ? res : (Result)cmd[1];
}

CppState cppDetect(void) {
    CppState result = CPP_UNKNOWN;
    Handle iru = 0, shm = 0, event = 0;
    volatile u8 *mem = NULL;
    bool initialized = false;

    mem = memalign(0x1000, SHM_SIZE);
    if (!mem)
        return CPP_UNKNOWN;
    memset((void *)mem, 0, SHM_SIZE);

    if (R_FAILED(svcCreateMemoryBlock(&shm, (u32)mem, SHM_SIZE, MEMPERM_READ | MEMPERM_WRITE,
                                      MEMPERM_READ | MEMPERM_WRITE)))
        goto out;
    if (R_FAILED(srvGetServiceHandle(&iru, "ir:USER")))
        goto out;

    u32 *cmd = getThreadCommandBuffer();
    cmd[0] = IRUSER_INITIALIZE_SHARED;
    cmd[1] = SHM_SIZE;
    cmd[2] = RECV_SIZE;
    cmd[3] = RECV_PACKETS;
    cmd[4] = SEND_SIZE;
    cmd[5] = SEND_PACKETS;
    cmd[6] = BITRATE_CPP;
    cmd[7] = IPC_Desc_SharedHandles(1);
    cmd[8] = shm;
    if (R_FAILED(svcSendSyncRequest(iru)) || R_FAILED((Result)cmd[1]))
        goto out;
    initialized = true;

    cmd = getThreadCommandBuffer();
    cmd[0] = IRUSER_GET_CONN_EVENT;
    if (R_FAILED(svcSendSyncRequest(iru)) || R_FAILED((Result)cmd[1]))
        goto out;
    event = cmd[3];

    cmd = getThreadCommandBuffer();
    cmd[0] = IRUSER_REQUIRE_CONNECTION;
    cmd[1] = DEVICE_CIRCLE_PAD_PRO;
    if (R_FAILED(svcSendSyncRequest(iru)) || R_FAILED((Result)cmd[1]))
        goto out;

    // The event fires on connection state changes. A timeout means nothing answered.
    svcWaitSynchronization(event, CONNECT_TIMEOUT_NS);
    result = (mem[STATUS_CONNECTION_OFFSET] == STATUS_CONNECTED) ? CPP_PRESENT : CPP_ABSENT;
    simple(iru, IRUSER_DISCONNECT);

out:
    if (initialized)
        simple(iru, IRUSER_FINALIZE);
    if (event)
        svcCloseHandle(event);
    if (iru)
        svcCloseHandle(iru);
    if (shm)
        svcCloseHandle(shm);
    free((void *)mem);
    return result;
}
