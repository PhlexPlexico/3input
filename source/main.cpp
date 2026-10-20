/*
    3Input - Input Reader background process for the 3DS.
    Copyright (C) 2020
    PhlexPlexico (https://github.com/phlexplexico)
    n3rdswithgame (https://github.com/n3rdswithgame)


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.


    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <3ds.h>

#include "hid_shared.h"
#include "irkeys.hpp"
#include "net.h"
#include "protocol.h"

// libctru's default startup also brings up apt, fs and sdmc, none of which we
// use, and (through hidInit) opens an ir:rst session of its own. We must not
// be an ir:rst client at all, games need those sessions, so only start what
// this module needs.
extern "C" {

bool hidShouldUseIrrst(void) {
    return false;
}

void __appInit(void) {
    srvInit();
    hidInit();
}

void __appExit(void) {
    hidExit();
    srvExit();
}

} // extern "C"

namespace {

constexpr int RATE_MIN     = 1;
constexpr int RATE_MAX     = 250;
constexpr int RATE_DEFAULT = 60;
/// Loop rate while nobody is connected: just enough to catch hotkeys and
/// new clients.
constexpr int RATE_IDLE    = 20;
constexpr int RATE_STEP    = 10;

constexpr u64 ACCEPT_INTERVAL_TICKS = SYSCLOCK_ARM11 / 5;

// srv notifications, https://www.3dbrew.org/wiki/Services (same ids rehid uses).
constexpr u32 NOTIF_EXIT        = 0x100; ///< Sent to every process asked to terminate.
constexpr u32 NOTIF_HOME_BUTTON = 0x204;

/// The HOME button is not part of the HID pad state, the system only announces
/// presses and never releases. Report each press as KEY_HOME_BIT for this long
/// (and for at least one packet).
constexpr u64 HOME_PULSE_TICKS = SYSCLOCK_ARM11 / 10;

int clampRate(int hz) {
    return hz < RATE_MIN ? RATE_MIN : (hz > RATE_MAX ? RATE_MAX : hz);
}

void setTimerRate(Handle timer, int hz) {
    s64 period = 1000000000LL / hz;
    svcCancelTimer(timer);
    svcClearTimer(timer);
    svcSetTimer(timer, period, period);
}

/// SELECT + START quits, SELECT + A/B/X/Y picks a send rate, SELECT + L/R
/// nudges it. Returns false when the module should exit.
bool handleHotkeys(u32 held, u32 prevHeld, int& rate) {
    if (!(held & KEY_SELECT))
        return true;

    u32 down = held & ~prevHeld;
    if (down & KEY_START) return false;
    if (down & KEY_B)     rate = 1;
    if (down & KEY_A)     rate = 10;
    if (down & KEY_Y)     rate = 60;
    if (down & KEY_X)     rate = 140;
    if (down & KEY_L)     rate = clampRate(rate - RATE_STEP);
    if (down & KEY_R)     rate = clampRate(rate + RATE_STEP);
    return true;
}

} // namespace

int main(int, char**) {
    const volatile HidShared* hid = reinterpret_cast<const volatile HidShared*>(hidSharedMem);
    if (!hid)
        return -1;

    NetServer net;
    if (!netInit(&net))
        return -1;

    Handle timer;
    svcCreateTimer(&timer, RESET_PULSE);

    // The HOME button reaches us as an srv notification. If this fails the
    // module still works, it just never reports HOME.
    Handle notifications = 0;
    if (R_SUCCEEDED(srvEnableNotification(&notifications))) {
        srvSubscribe(NOTIF_HOME_BUTTON);
    } else {
        notifications = 0;
    }
    Handle waitOn[2] = {timer, notifications};
    s32 waitCount = notifications ? 2 : 1;

    bool homeActive = false;   // a HOME press is being reported
    bool homeReported = false; // ... and has been in at least one packet
    u64 homeUntil = 0;

    int rate = RATE_DEFAULT;
    int timerHz = 0;
    u32 prevHeld = 0;
    u8 seq = 0;
    u64 nextAccept = 0;

    for (;;) {
        // Sleep on a kernel timer: zero CPU between frames, and drop to a slow
        // tick when there is nobody to send to.
        int wantHz = net.num_clients ? rate : RATE_IDLE;
        if (wantHz != timerHz) {
            setTimerRate(timer, wantHz);
            timerHz = wantHz;
        }
        s32 woke = 0;
        svcWaitSynchronizationN(&woke, waitOn, waitCount, false, U64_MAX);

        if (woke == 1) {
            u32 id = 0;
            if (R_SUCCEEDED(srvReceiveNotification(&id))) {
                if (id == NOTIF_EXIT)
                    break;
                if (id == NOTIF_HOME_BUTTON) {
                    homeActive = true;
                    homeReported = false;
                    homeUntil = svcGetSystemTick() + HOME_PULSE_TICKS;
                }
            }
            continue; // the timer keeps its own schedule
        }

        // The HID module keeps a ring of the last 8 states; just read the newest.
        u32 padIdx = hid->pad.index;
        if (padIdx >= HID_RING_SIZE) padIdx = 0;
        u32 touchIdx = hid->touch.index;
        if (touchIdx >= HID_RING_SIZE) touchIdx = 0;

        u32 held = hid->pad.entries[padIdx].curr;
        if (!handleHotkeys(held, prevHeld, rate))
            break;
        prevHeld = held;

        irkeysPoll();

        u64 now = svcGetSystemTick();
        if (now >= nextAccept) {
            netAccept(&net);
            nextAccept = now + ACCEPT_INTERVAL_TICKS;
        }

        if (net.num_clients) {
            InputPacket pkt;
            pkt.magic[0] = INPUT_MAGIC0;
            pkt.magic[1] = INPUT_MAGIC1;
            pkt.version  = INPUT_VERSION;
            pkt.seq      = seq++;
            pkt.buttons  = held | irkeysRead() | (homeActive ? KEY_HOME_BIT : 0);
            homeReported = homeReported || homeActive;
            pkt.cp_x     = hid->pad.entries[padIdx].cp_x;
            pkt.cp_y     = hid->pad.entries[padIdx].cp_y;
            pkt.tp_x     = hid->touch.entries[touchIdx].x;
            pkt.tp_y     = hid->touch.entries[touchIdx].y;
            s16 csX, csY;
            irkeysCstick(&csX, &csY);
            pkt.cs_x     = csX;
            pkt.cs_y     = csY;
            pkt.ir_status   = irkeysStatus();
            pkt.reserved[0] = pkt.reserved[1] = 0;
            pkt.ir_error    = irkeysError();
            u8 csStatus;
            u32 csDetail;
            irkeysCstickInfo(&csStatus, &csDetail);
            pkt.cs_status   = csStatus;
            pkt.cs_error    = csDetail;
            netBroadcast(&net, &pkt, sizeof(pkt));
        }
        // Nobody listening counts as reported too, so a stale press cannot
        // show up on the next connection.
        if (homeActive && (homeReported || !net.num_clients) && now >= homeUntil)
            homeActive = homeReported = false;
    }

    svcCancelTimer(timer);
    svcCloseHandle(timer);
    if (notifications)
        svcCloseHandle(notifications);
    irkeysExit();
    netExit(&net);
    return 0;
}
