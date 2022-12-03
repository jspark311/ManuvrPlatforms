/*
* File:   C3POnX11.h
* Author: J. Ian Lindsay
* Date:   2022.12.03
*
* This is the root include file if you want your program to run on top of X11.
*
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
#include <sys/utsname.h>

#include "RHoM.h"

#ifndef __C3PX11_HEADER_H__
#define __C3PX11_HEADER_H__


/*******************************************************************************
* Types
*******************************************************************************/

/*
* This is the listing of buttons we will respond to, along with their queues
*   and application response policies. The application should define these if
*   the classes rendered in the window are expected to respond to pointer-type
*   events.
*/
struct MouseButtonDef {
  const uint button_id;
  const char* const label;
  const GfxUIEvent gfx_event_down;
  const GfxUIEvent gfx_event_up;
};


/*******************************************************************************
* This class represents a window in X11.
* It implements its own framebuffer, C3P GUI objects, and will run in its own
*   thread.
*******************************************************************************/

class C3Px11Window {
  public:
    C3Px11Window(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    ~C3Px11Window();

    int8_t poll();
    int8_t console_callback(StringBuilder* text_return, StringBuilder* args);


  private:
    uint32_t        _width;
    uint32_t        _height;
    uint32_t        _pointer_x;
    uint32_t        _pointer_y;
    unsigned long   _thread_id;
    XImage*         _ximage;
    Display*        _dpy;
    int             _screen_num;
    Window          _win;
    PeriodicTimeout _refresh_period;
    StopWatch       _redraw_timer;
    Image           _fb;

    int8_t _resize_and_render_all_elements();
};

#endif  // __C3PX11_HEADER_H__
