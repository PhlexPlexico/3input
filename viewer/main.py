#!/usr/bin/python3

import os
import sys

import tkinter as tk

from threeinput import BTN, Conn, IR_STATUS, CS_STATUS, WIFI_STATUS

# Longest line the info labels may have before they wrap, so they fit small windows.
LABEL_WRAP = 260
# Width (in characters) of each button label, so the two columns stay put as values change.
BTN_LABEL_WIDTH = 15

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
    BTN.HOME    : (),
    BTN.C_RIGHT : (),
    BTN.C_LEFT  : (),
    BTN.C_UP    : (),
    BTN.C_DOWN  : (),
    BTN.CPAD_RIGHT : (),
    BTN.CPAD_LEFT  : (),
    BTN.CPAD_UP    : (),
    BTN.CPAD_DOWN  : (),
}

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
        self.overlay_img = tk.PhotoImage(file=os.path.join(os.path.dirname(os.path.abspath(__file__)), "shitty_overlay.png"))
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

        # Side panel next to the overlay. It is its own frame (not rows of the
        # window grid), otherwise the 551 px tall canvas row stretches the first
        # label and pushes everything else off the screen.
        self.info = tk.Frame(master)
        self.info.grid(row=0, column=1, sticky='nw', padx=8, pady=4)

        self.labels = {}
        buttons = list(BTN)
        rows = (len(buttons) + 1) // 2  # two columns of buttons
        for i, btn in enumerate(buttons):
            lbl = tk.Label(self.info, text=btn.name, width=BTN_LABEL_WIDTH, anchor='w')
            self.labels[btn] = lbl
            lbl.grid(row=i % rows, column=i // rows, sticky='w')

        for k, name in enumerate(('cp', 'tp', 'cs', 'ir', 'wifi')):
            lbl = tk.Label(self.info, text=name, wraplength=LABEL_WRAP, justify='left', anchor='w')
            self.labels[name] = lbl
            lbl.grid(row=rows + 1 + k, column=0, columnspan=2, sticky='w', pady=(6 if k == 0 else 0, 0))

    def update_lable(self):
        if self.c.hasInput():
            self.nextInput = self.c.getInput()
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

            self.labels['cs'].config(text=f"c-stick:({self.nextInput['cs_x']},{self.nextInput['cs_y']}) {CS_STATUS.get(self.nextInput['cs_status'], '?')} (0x{self.nextInput['cs_error']:08X})")
            self.labels['wifi'].config(text=f"WIFI: {WIFI_STATUS.get(self.nextInput['wifi_status'], '?')} (recoveries {self.nextInput['wifi_recoveries']})")
            self.labels['ir'].config(text=f"IR: {IR_STATUS.get(self.nextInput['ir_status'], '?')} (0x{self.nextInput['ir_error']:08X})")
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

    