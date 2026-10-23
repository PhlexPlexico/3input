"""3Input client library: the BTN bit layout, packet decoder and a TCP receiver.

No GUI dependencies, so it works wherever Python does. The wire format is
defined in source/protocol.h.
"""
from collections import deque
from enum import IntEnum
import socket as soc
import struct
from threading import Thread

BIT = lambda x: 1<<x

class BTN(IntEnum):
    A       = BIT(0) 
    B       = BIT(1) 
    SEL     = BIT(2) 
    START   = BIT(3) 
    D_RIGHT = BIT(4) 
    D_LEFT  = BIT(5) 
    D_UP    = BIT(6) 
    D_DOWN  = BIT(7) 
    R       = BIT(8) 
    L       = BIT(9) 
    X       = BIT(10) 
    Y       = BIT(11) 
    ZL      = BIT(14) 
    ZR      = BIT(15) 
    HOME    = BIT(16)
    C_RIGHT = BIT(24)
    C_LEFT  = BIT(25)
    C_UP    = BIT(26)
    C_DOWN  = BIT(27)
    CPAD_RIGHT = BIT(28)
    CPAD_LEFT  = BIT(29)
    CPAD_UP    = BIT(30)
    CPAD_DOWN  = BIT(31)

# Wire format, see source/protocol.h. Little endian, no padding:
# magic 'I3', version, seq, buttons, cp_x, cp_y, tp_x, tp_y, cs_x, cs_y, ir_status, cs_status, ir_error, cs_error
PACKET = struct.Struct('<2sBBIhhhhhhBBBBII')
MAGIC = b'I3'
VERSION = 4

# source/protocol.h IrStatus: how the ZL/ZR/C-Stick path on the 3DS is doing
IR_STATUS = {
    0: 'pending (patched ir:u not reached yet)',
    1: 'UNPATCHED IR module (code.ips not applied)',
    2: 'passive (live only while a game/system runs ir:rst)',
    3: 'holding an ir:rst session',
    4: 'yielded (IR state byte set, meaning unconfirmed)',
    5: 'could not get an ir:rst session',
}

# source/protocol.h CsStatus: state of the analog C-Stick path (ir:rst shared memory)
CS_STATUS = {
    0: 'pending',
    1: 'mapped (detail = ring pad word, may be stale)',
    2: 'could not open ir:rst',
    3: 'ir:rst GetHandles failed',
    4: 'ir:rst returned no shared memory',
    5: 'mapping ir:rst shared memory failed',
}

# source/protocol.h WifiStatus: the 3DS module's Wi-Fi keep-alive
WIFI_STATUS = {
    0: 'not reported (older module)',
    1: 'starting',
    2: 'holding the connection',
    3: 'link down, reconnecting',
    4: 'link down, gave up',
    5: 'could not open ac',
}


def parse_packet(raw):
    """Turns one PACKET.size byte frame into the dict the UI consumes."""
    magic, version, seq, buttons, cp_x, cp_y, tp_x, tp_y, cs_x, cs_y, ir_status, cs_status, wifi_status, wifi_recoveries, ir_error, cs_error = PACKET.unpack(raw)
    if magic != MAGIC or version != VERSION:
        raise ValueError('bad packet header')
    j = {'seq': seq, 'cp_x': cp_x, 'cp_y': cp_y, 'tp_x': tp_x, 'tp_y': tp_y, 'cs_x': cs_x, 'cs_y': cs_y,
         'ir_status': ir_status, 'ir_error': ir_error,
         'cs_status': cs_status, 'cs_error': cs_error,
         'wifi_status': wifi_status, 'wifi_recoveries': wifi_recoveries}
    for btn in BTN:
        j[btn] = bool(buttons & btn)
    return j


class Conn():
    def __init__(self, ip, port):
        self.inputs = deque(maxlen=4)
        self.s = self.connect(ip, port)
        self.keepGoing = True
        self.netThread = Thread(target=self.netMain)
        self.netThread.start()
    
    def stop(self):
        self.keepGoing = False
    
    def join(self):
        self.netThread.join()
        print('[*] net join')
    
    def connect(self, ip, port):
        s = soc.socket(soc.AF_INET, soc.SOCK_STREAM)
        s.connect((ip, port))
        s.settimeout(1.0)
        return s

    def netMain(self):
        print("[*] starting netMain")
        buf = b''
        while self.keepGoing:
            try:
                chunk = self.s.recv(256)
            except soc.timeout:
                continue
            except OSError:
                break
            if not chunk: # 3DS closed the connection
                break
            buf += chunk
            while len(buf) >= PACKET.size:
                if not buf.startswith(MAGIC):
                    # lost sync, skip ahead to the next possible header
                    nxt = buf.find(MAGIC, 1)
                    buf = buf[nxt:] if nxt != -1 else buf[-1:]
                    continue
                try:
                    self.inputs.append(parse_packet(buf[:PACKET.size]))
                except ValueError:
                    pass
                buf = buf[PACKET.size:]
        self.keepGoing = False
        self.s.close()
        print("[*] exiting netMain")

    def hasInput(self):
        return len(self.inputs) > 0

    def getInput(self):
        return self.inputs.popleft()
