/*
* File:   C3POnX11.h
* Author: J. Ian Lindsay
* Date:   2022.12.03
*
* This is the root include file if you want your program to run on top of X11.
*
*
* TODO: Formalize window setup and teardown contract.
*/

#include <cstdio>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

#include <fstream>
#include <iostream>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <sys/utsname.h>

#include "CppPotpourri.h"
#include "StringBuilder.h"
#include "PriorityQueue.h"
#include "TimerTools/TimerTools.h"
#include "Image/Image.h"
#include "Image/ImageUtils.h"
#include "Image/GfxUI.h"
#include "Linux.h"

#ifndef __C3PX11_HEADER_H__
#define __C3PX11_HEADER_H__

void* gui_thread_handler(void*);


/*******************************************************************************
* Types
*******************************************************************************/

typedef void (*ValueChangeCallback)(GfxUIElement*);


/*
* This is the listing of buttons we will respond to, along with their queues
*   and application response policies. The application should define these if
*   the classes rendered in the window are expected to respond to pointer-type
*   events.
*/
struct MouseButtonDef {
  const char* const label;
  const uint16_t    button_id;
  const GfxUIEvent  gfx_event_down;
  const GfxUIEvent  gfx_event_up;
};


/**
* This class represents a window in X11.
* It implements its own framebuffer, C3P GUI objects, and will run in its own
*   thread.
*/
class C3Px11Window {
  public:
    GfxUIGroup      root;
    UIGfxWrapper    gfx;           // Wrapper for elements rendered in the main pane.
    UIGfxWrapper    gfx_overlay;   // Wrapper for transient overlay data.

    C3Px11Window(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char* TITLE = "C3Px11Window");
    virtual ~C3Px11Window();

    /* These functions must be implemented by the application as typed children of this class. */
    virtual int8_t poll()             =0;
    virtual int8_t createWindow()     =0;
    virtual int8_t closeWindow()      =0;
    virtual int8_t render(bool force) =0;
    virtual int8_t render_overlay()   =0;

    inline uint32_t width() {         return root.elementWidth();    };
    inline uint32_t height() {        return root.elementHeight();   };
    inline Image*   frameBuffer() {   return &_fb;      };
    inline Image*   overlay() {       return &_overlay; };
    inline bool     windowReady() {   return ((nullptr != _ximage) && _fb.allocated());   };
    inline bool     keepPolling() {   return _keep_polling;   };
    inline void setCallback(ValueChangeCallback x) {  _vc_callback = x;   };
    inline bool pointerInWindow() {   return ((0 <= _pointer_x) && (0 <= _pointer_y) && (width() > (uint32_t) _pointer_x) && (height() > (uint32_t) _pointer_y));  };

    inline void queryPointer(int* x, int* y) {
      *x = _pointer_x;
      *y = _pointer_y;
    };

    GfxUIElement* elementUnderPointer();

    int8_t map_button_inputs(MouseButtonDef*, uint32_t count);
    int8_t request_clipboard();
    int8_t request_selection_buffer();

    friend void* gui_thread_handler(void*);  // We allow the ISR access to private members.


  protected:
    const char*     _title;
    int             _pointer_x;
    int             _pointer_y;
    int             _pointer_drag_point_x;
    int             _pointer_drag_point_y;
    unsigned long   _thread_id;
    Window          _win;
    XImage*         _ximage;
    Display*        _dpy;
    Visual*         _visual;
    int             _screen_num;
    MillisTimeout   _refresh_period;
    StopWatch       _redraw_timer;
    Image           _fb;
    Image           _overlay;
    ValueChangeCallback _vc_callback;
    PriorityQueue<MouseButtonDef*> _btn_defs;
    bool            _keep_polling;
    GfxUIElement*   _paste_target;
    GfxUIElement*   _pointer_client;
    GfxUIElement*   _mrtlhne;  // Most-recent top-level hover-notified element. Suffer.

    int8_t _init_window();
    int8_t _deinit_window();
    int8_t _redraw_window();
    int8_t _refit_window();
    int8_t _process_motion();
    int8_t _query_pointer();
    int8_t _proc_changelog(PriorityQueue<GfxUIElement*>* resp_list);
    int8_t _proc_mouse_button(uint16_t btn_id, uint32_t x, uint32_t y, bool pressed);
};

#endif  // __C3PX11_HEADER_H__
