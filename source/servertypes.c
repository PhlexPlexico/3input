#include "servertypes.h"

#include <string.h>
#include <stdio.h>

#include <3ds.h>

#include "ui.h"
#include "my_hid.h"

volatile hid_mem_t *hid = NULL;

#define JSON_COMPACT 1


#if JSON_COMPACT
    #define JSON_ENDL                   ""
    #define JSON_TAB                    ""
    #define JSON_QUOTE                  "\""
#else
    #define JSON_ENDL                   "\n"
    #define JSON_TAB                    "\t"
    #define JSON_QUOTE                  "\""
#endif

#define JSON_START                      "{" JSON_ENDL
#define JSON_KEY_VAL(key,val)           JSON_QUOTE key JSON_QUOTE ":" val
#define JSON_ENTRY(key,val)             JSON_TAB JSON_KEY_VAL(key, val) "," JSON_ENDL
#define JSON_LENTRY(key,val)            JSON_TAB JSON_KEY_VAL(key, val) JSON_ENDL
#define JSON_END                        "}," JSON_ENDL
char input_json_raw[] =     JSON_START
                            JSON_ENTRY("btn","%u")
                            JSON_ENTRY("cp_x","%hd")
                            JSON_ENTRY("cp_y","%hd")
                            JSON_ENTRY("tp_x","%hd")
                            JSON_ENTRY("tp_y","%hd")
                            JSON_ENTRY("ir_btn","14")/*"%u"*/
                            JSON_ENTRY("cpp_x", "14")/*"%hd"*/
                            JSON_LENTRY("cpp_y", "14")/*"%hd"*/
                            JSON_END ;


struct input_server_info_t {
    int i;
};

void input_server_func(net_t* net, void* data) {
    char json[256];
    int json_len;

    volatile pad_t*   curr_pad;
    volatile touch_t* curr_touch;

    struct input_server_info_t* info = (struct input_server_info_t*) data;
    info->i++;

    curr_pad    = &(hid->pad.pads[hid->pad.index]);
    curr_touch  = &(hid->touch.touches[hid->touch.index]);
    
    //TODO: Expensive, change this to binary output instead
    //Parse on client side. Should take care of some heat issues.
    //TODO: Include Circle pad pro/gryo/accel.
    json_len = sprintf(json,input_json_raw,
                    curr_pad->curr.val,
                    curr_pad->cp.x,
                    curr_pad->cp.y,
                    curr_touch->touch.x,
                    curr_touch->touch.y
                    );
    net_send(net, json, json_len);
}
int make_input_server(server_t* server) {
    // Relinquish control and use only shared HID.
    hid = (volatile hid_mem_t *)hidSharedMem;
    
    return server_ctor(server, input_server_func, sizeof(struct input_server_info_t));
}
