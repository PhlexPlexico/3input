#!/usr/bin/python3

from enum import IntEnum
import json
import signal
import socket as soc
from threading import Thread

import sys

import tkinter as tk

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

btn_cords = {
    BTN.A       : (934,212),
    BTN.B       : (879,263),
    BTN.SEL     : (826, 437),
    BTN.START   : (826, 374),
    BTN.D_RIGHT : (143, 316),
    BTN.D_LEFT  : ( 55, 316),
    BTN.D_UP    : ( 99, 272),
    BTN.D_DOWN  : ( 99, 360),
    BTN.R       : (),
    BTN.L       : (),
    BTN.X       : (879,162),
    BTN.Y       : (826, 213),
    BTN.ZL      : (),
    BTN.ZR      : (),
}

class Conn():
    def __init__(self, ip, port):
        self.jsons = []
        self.buf = b''
        self.s = self.connect(ip, port)
        self.keepGoing = True
        self.netThread = Thread(target=self.netMain)
        self.parseThread = Thread(target=self.parseMain)
        self.netThread.start()
        self.parseThread.start()
    
    def stop(self):
        self.keepGoing = False;
    
    def join(self):
        self.netThread.join()
        print('[*] net join')
        self.parseThread.join()
        print('[*] pasrse join')
    
    def connect(self, ip, port):
        s = soc.socket(soc.AF_INET, soc.SOCK_STREAM)
        s.connect((ip, port))
        return s

    def netMain(self):
        print("[*] starting netMain")
        while self.keepGoing:
            buf = self.s.recv(256)
            if b'disconnecting' in buf:
                self.stop()
                continue
            if len(self.buf) > 192:
                self.buf = buf
            else:
                self.buf += buf
        self.s.close()
        print("[*] exiting netMain")

    def parseMain(self):
        print("[*] starting parseMain")
        while self.keepGoing:
            if not self.buf:
                continue
            if b'disconnecting' in self.buf:
                self.stop()
                continue
            if self.buf.find(b'{') == -1: #buffer non empty but doesn't have a json start
                self.buf = b''
            elif self.buf.find(b'{'):
                self.buf = self.buf[self.buf.find(b'{'):] #trim up to the start of the json
            print(self.buf)
            json_end = self.buf.find(b'}')+1
            if json_end != 0:
                json_raw, self.buf = self.buf[:json_end], self.buf[json_end:] # strip off until the first } set
                
                last_open_curly = json_raw.rfind(b'{')
                
                if json_raw!=0:
                    #print('\t[@]',json_raw.decode())
                    json_raw = json_raw[last_open_curly:]
                    #print('\t[@]',json_raw.decode())
                try:
                    #print(json_raw.decode())
                    j = json.loads(json_raw)#try and parse the { ... } pair
                    
                    btn_raw = j.pop('btn',0)
                    
                    for btn in BTN:
                        if btn_raw & btn:
                            j[btn] = True
                        else:
                            j[btn] = False
                    self.jsons.append(j) 
                except ValueError as e:
                    print('Invalid Json blob, ', json_raw.decode())

        print("[*] exiting parseMain")

    def hasJson(self):
        l = len(self.jsons)
        if l > 3:
            print(f'[#] backlog too big {l} jsons')
            self.jsons = []
            return False
        return l > 0

    def getJson(self):       
        return self.jsons.pop(0)

class Arrow():
    def __init__(self, x, y, cvs, scale = 1/3.0):
        self.x = x
        self.y = y
        self.cvs = cvs
        self.arrow = None
        self.scale = scale
    def draw(self, x, y):
        if self.arrow is not None:
            self.cvs.delete(self.arrow)
        if abs(x) < 10 and abs(y) < 10: return
        self.arrow = self.cvs.create_line(self.x, self.y, self.x + x*self.scale, self.y - y*self.scale, width=5, arrow=tk.LAST, fill='red')

class ButtonPress():
    def __init__(self, x, y, cvs, r = 20):
        self.x = x
        self.y = y
        self.cvs = cvs
        self.r = r
        self.press = None

    def update(self, pressed):
        if pressed:
            if self.press is not None:
                self.cvs.delete(self.press)
            self.press = self.cvs.create_oval(self.x - self.r, self.y - self.r, self.x + self.r, self.y + self.r, width=0, fill='red')
        else:
            if self.press is not None:
                self.cvs.delete(self.press)
                self.press = None

class TouchScreen():
    def __init__(self, x, y, w, h, master=None, background='green'):
        self.x = x
        self.y = y
        self.w = w
        self.h = h
        self.real_w = 320
        self.real_h = 240
        self.touch_cvs = tk.Canvas(master, width=w, height=h)
        self.touch_cvs.place(x=x,y=y)
        self.background = background
        self.touch_cvs.create_rectangle(0,0,w,h,fill=background)
        self.touched = None
    
    def draw_circle(self, x, y, r, **kwargs):
        if x==0 and y==0:
            return None
        return self.touch_cvs.create_oval(x-r, y-r, x+r, y+r, width=0, **kwargs)

    def draw(self, x,y):
        x *= self.w / self.real_w
        y *= self.h / self.real_h

        if self.touched is not None:
            self.touch_cvs.delete(self.touched)

        self.touched = self.draw_circle(x,y, 10, fill="blue")

class Window(tk.Frame):
    def __init__(self, ip, port, master=None):
        tk.Frame.__init__(self, master)
        self.master = master
        self.overlay_img = tk.PhotoImage(file="shitty_overlay.png")
        self.overlay_cvs = tk.Canvas(master, width=980, height=551)
        self.overlay_cvs.place(x=0, y=0)
        self.overlay_cvs.create_image((490,276),image=self.overlay_img)
        self.overlay_cvs.grid()
        
        self.cp_arrow = Arrow(99, 161, self.overlay_cvs)
        
        self.btn = {}
        for btn in BTN:
            if btn_cords[btn]:
                x,y = btn_cords[btn]
                self.btn[btn] = ButtonPress(x,y, self.overlay_cvs)
            else:
                self.btn[btn] = None

        self.touch = TouchScreen(226,85,527,395, master)

        self.c = Conn(ip, port)
        self.master.after(0, self.update_lable)
        self.nextInput = None

        self.keepGoing = True

        self.labels = {}
        j = 0
        for i,btn in enumerate(BTN):
            lbl = tk.Label(text=btn.name)
            self.labels[btn] = lbl
            lbl.grid(row=i, column=1,sticky='nwes')
            j = i
        self.labels['cp'] = lbl = tk.Label(text='cp')
        lbl.grid(row=j, column = 1, stick='nwes')
        j += 1

        self.labels['tp'] = lbl = tk.Label(text='tp')
        lbl.grid(row=j, column = 1, stick='nwes')
        j += 1
            
        
    
    def update_lable(self):
        if self.c.hasJson():
            self.nextInput = self.c.getJson()
            for btn in BTN:
                if not self.keepGoing:
                    break
                self.labels[btn].config(text=f'{btn.name} : {self.nextInput[btn]}')
                if self.btn[btn] is not None:
                    self.btn[btn].update(self.nextInput[btn])
            cp_x = self.nextInput['cp_x']
            cp_y = self.nextInput['cp_y']
            self.labels['cp'].config(text=f'cp:({cp_x},{cp_y})')
            
            self.cp_arrow.draw(cp_x, cp_y)

            tp_x = self.nextInput['tp_x']
            tp_y = self.nextInput['tp_y']
            self.labels['tp'].config(text=f'tp:({tp_x},{tp_y})')

            self.touch.draw(tp_x,tp_y)
        if not self.c.keepGoing:
            self.master.quit()
        if self.keepGoing:
            self.master.after(1, self.update_lable)
        

    
    def on_close(self):
        self.keepGoing = False
        if self.c is not None:
            self.c.stop()
            self.c.join()
            #self.updateThread.join()
            print('[*] update join')
            self.c = None


def main():
    ip = '192.168.1.218'
    if len(sys.argv) > 1:
        ip = sys.argv[1]

    print('[&] connecting to ', ip)
    root = tk.Tk()
    win = Window(ip, 65534, root)
    root.mainloop()
    win.on_close()

    print('3ds exiting')
        
    
if __name__=='__main__':
   
    main()

    