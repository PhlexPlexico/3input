#pragma once
#include <3ds.h>

/// Cheap to call every loop, does its work about once a second.
void wifiPoll(void);

/// WifiStatus (protocol.h).
u8 wifiStatus(void);

/// Number of times the link came back after a drop (saturates at 255).
u8 wifiRecoveries(void);

/// True once after each recovery. The IP address may have changed, so listening
/// sockets should be rebuilt.
bool wifiTakeRecovered(void);

void wifiExit(void);
