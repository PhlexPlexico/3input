#include "wifi.hpp"

#include "protocol.h"

namespace {

constexpr u64 CHECK_TICKS  = SYSCLOCK_ARM11;     // look at the link about once a second
constexpr u64 RETRY_TICKS  = SYSCLOCK_ARM11 * 3; // between recovery attempts
constexpr int MAX_ATTEMPTS = 10;                 // per drop, then wait for the link to return on its own
constexpr u32 AC_CONNECTED = 3;                  // ACU_GetStatus: 1 = not connected, 3 = connected

u8   status = WIFI_PENDING;
u8   recoveries = 0;
bool recovered = false;

bool acUp = false;
bool nwmUp = false;
bool requestHeld = false;  // we have asked AC to connect at least once while the link was up
bool linkUp = false;
bool everDropped = false;
int  attempts = 0;
u64  nextCheck = 0;
Handle connectEvent = 0;

bool linkIsUp() {
    u32 st = 0, ap = 0;
    return R_SUCCEEDED(ACU_GetStatus(&st)) && st == AC_CONNECTED
        && R_SUCCEEDED(ACU_GetWifiStatus(&ap)) && ap != 0;
}

/// Issues our own connection request, like Rosalina's forced connection.
/// Returns false if one is still pending or the request failed.
bool requestConnection() {
    if (connectEvent) {
        if (R_FAILED(svcWaitSynchronization(connectEvent, 0)))
            return false;  // previous request not finished yet
        svcCloseHandle(connectEvent);
        connectEvent = 0;
    }

    acuConfig config;
    ACU_CreateDefaultConfig(&config);
    ACU_SetNetworkArea(&config, 2);
    // The argument is a u8 of allowed slots. libctru's header numbers them from bit 1,
    // Rosalina from bit 0, so allow every bit rather than guess.
    ACU_SetAllowApType(&config, 0xFF);
    ACU_SetRequestEulaVersion(&config);

    if (R_FAILED(svcCreateEvent(&connectEvent, RESET_ONESHOT))) {
        connectEvent = 0;
        return false;
    }
    if (R_FAILED(ACU_ConnectAsync(&config, connectEvent))) {
        svcCloseHandle(connectEvent);
        connectEvent = 0;
        return false;
    }
    return true;
}

} // namespace

void wifiPoll(void) {
    u64 now = svcGetSystemTick();
    if (now < nextCheck)
        return;
    nextCheck = now + CHECK_TICKS;

    if (!acUp) {
        // Never acExit() this: an open ac session is part of how the link is kept.
        if (R_FAILED(acInit())) {
            status = WIFI_AC_FAILED;
            nextCheck = now + RETRY_TICKS;
            return;
        }
        acUp = true;
    }

    if (linkIsUp()) {
        if (!linkUp) {
            linkUp = true;
            attempts = 0;
            if (everDropped) {
                if (recoveries < 255)
                    recoveries++;
                recovered = true;
            }
        }
        if (!requestHeld)
            requestHeld = requestConnection();  // hold a request of our own while it is up
        status = WIFI_HOLDING;
        return;
    }

    // Link is down.
    if (linkUp) {
        linkUp = false;
        everDropped = true;
        attempts = 0;
    }
    if (attempts >= MAX_ATTEMPTS) {
        status = WIFI_GAVE_UP;
        return;
    }
    attempts++;
    status = WIFI_RECONNECTING;
    nextCheck = now + RETRY_TICKS;

    // Some games switch the radio off after they start.
    if (!nwmUp)
        nwmUp = R_SUCCEEDED(nwmExtInit());
    if (nwmUp)
        NWMEXT_ControlWirelessEnabled(true);  // an "already on" error is harmless

    requestConnection();
}

u8 wifiStatus(void) {
    return status;
}

u8 wifiRecoveries(void) {
    return recoveries;
}

bool wifiTakeRecovered(void) {
    bool r = recovered;
    recovered = false;
    return r;
}

void wifiExit(void) {
    if (connectEvent) {
        svcCloseHandle(connectEvent);
        connectEvent = 0;
    }
    if (nwmUp) {
        nwmExtExit();
        nwmUp = false;
    }
    if (acUp) {
        acExit();
        acUp = false;
    }
}

