/*
* File:   X11Window.cpp
* Author: J. Ian Lindsay
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

#define INSET_SIZE   200


extern unsigned long gui_thread_id;   // TODO: (rolled up newspaper) Bad...
extern bool continue_running;         // TODO: (rolled up newspaper) Bad...

extern SensorFilter<uint32_t> _filter;

Display* dpy = nullptr;
XImage* ximage = nullptr;

Image _main_img(1, 1, ImgBufferFormat::R8_G8_B8_ALPHA);
Image _overlay_img(1, 1, ImgBufferFormat::R8_G8_B8_ALPHA);
UIGfxWrapper ui_gfx(&_main_img);

SensorFilter<uint32_t> test_filter_0(150, FilteringStrategy::RAW);
SensorFilter<float> test_filter_1(256, FilteringStrategy::RAW);

GfxUIButton _button_0(0,   100, 22, 22, 0x9932CC);
GfxUIButton _button_1(25,  100, 22, 22, 0xFF8C00, GFXUI_BUTTON_FLAG_MOMENTARY);
GfxUIButton _button_2(50,  100, 22, 22, 0x9932CC);
GfxUIButton _button_3(75,  100, 22, 22, 0x9932CC, GFXUI_BUTTON_FLAG_MOMENTARY);
GfxUIButton _button_4(100, 100, 22, 22, 0xE9967A, GFXUI_BUTTON_FLAG_MOMENTARY);
GfxUIButton _button_5(125, 100, 22, 22, 0x9932CC);
GfxUIButton _button_6(150, 100, 22, 22, 0x9932CC, GFXUI_BUTTON_FLAG_MOMENTARY);
GfxUIButton _button_7(175, 100, 22, 22, 0x9932CC);

GfxUISlider _slider_0(0,   125, 197, 20, 0x20B2AA, GFXUI_SLIDER_FLAG_RENDER_VALUE);
GfxUISlider _slider_1(0,   150, 197, 20, 0xFFA07A, GFXUI_SLIDER_FLAG_RENDER_VALUE);
GfxUISlider _slider_2(0,   175, 197, 20, 0x0000CD, GFXUI_SLIDER_FLAG_RENDER_VALUE);
GfxUISlider _slider_3(0,   200, 197, 20, 0x6B8E23, GFXUI_SLIDER_FLAG_RENDER_VALUE);
GfxUISlider _slider_4(0,   225, 197, 20, 0xFFF5EE);
GfxUISlider _slider_5(205, 125, 25, 120, 0x90F5EE, GFXUI_SLIDER_FLAG_RENDER_VALUE | GFXUI_SLIDER_FLAG_VERTICAL);
GfxUISlider _slider_6(235, 125, 25, 120, 0xDC143C, GFXUI_SLIDER_FLAG_RENDER_VALUE | GFXUI_SLIDER_FLAG_VERTICAL);

GfxUISensorFilter<uint32_t> sf_render_0(&test_filter_0, 280, 125, 150, 120, 0xA05010, GFXUI_SENFILT_FLAG_SHOW_RANGE);

StopWatch redraw_timer;

bool gravepact = true;

uint pointer_x = 0;
uint pointer_y = 0;
uint window_w  = 0;
uint window_h  = 0;


/*
* This is the listing of buttons we will respond to, along with their queues
*   and application response policies.
*/
struct MouseButtonDef {
  const uint button_id;
  const char* const label;
  const GfxUIEvent gfx_event_down;
  const GfxUIEvent gfx_event_up;
};

MouseButtonDef mouse_buttons[] = {
  { .button_id = 1,
    .label = "Left",
    .gfx_event_down = GfxUIEvent::TOUCH,
    .gfx_event_up   = GfxUIEvent::RELEASE
  },
  { .button_id = 2,
    .label = "Middle",
    .gfx_event_down = GfxUIEvent::DRAG,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .button_id = 3,
    .label = "Right",
    .gfx_event_down = GfxUIEvent::SELECT,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .button_id = 4,
    .label = "ScrlUp",
    .gfx_event_down = GfxUIEvent::MOVE_UP,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .button_id = 5,
    .label = "ScrlDwn",
    .gfx_event_down = GfxUIEvent::MOVE_DOWN,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .button_id = 6,
    .label = "TiltLeft",
    .gfx_event_down = GfxUIEvent::MOVE_LEFT,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .button_id = 7,
    .label = "TiltRight",
    .gfx_event_down = GfxUIEvent::MOVE_RIGHT,
    .gfx_event_up   = GfxUIEvent::NONE
  }
};

PriorityQueue<GfxUIElement*> element_queue;



/*******************************************************************************
* Mouse/touch handlers
*******************************************************************************/

void proc_mouse_button(uint btn_id, uint x, uint y, bool pressed) {
  const uint SEARCH_SIZE_DEF = sizeof(mouse_buttons) / sizeof(MouseButtonDef);
  for (uint i = 0; i < SEARCH_SIZE_DEF; i++) {
    if (mouse_buttons[i].button_id == btn_id) {
      const uint SEARCH_SIZE_QUEUE = element_queue.size();
      bool local_ret = false;
      for (uint n = 0; n < SEARCH_SIZE_QUEUE; n++) {
        GfxUIElement* ui_obj = element_queue.get(n);
        const GfxUIEvent event = pressed ? mouse_buttons[i].gfx_event_down : mouse_buttons[i].gfx_event_up;
        if (GfxUIEvent::NONE != event) {
          local_ret = ui_obj->notify(event, x, y);
        }
        if (local_ret) {
          break;
        }
      }
      if (!local_ret) {
        c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "%s %s: (%d, %d) (no target)", mouse_buttons[i].label, (pressed ? "click" : "release"), x, y);
        return;
      }
    }
  }
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Unhandled %s %d: (%d, %d)", (pressed ? "click" : "release"), btn_id, x, y);
}


/*
* Called to unconditionally show the elements in the GUI.
*/
void render_all_elements() {
  const char* HEADER_STR_0 = "Right Hand of Manuvr";
  const char* HEADER_STR_1 = "Build date " __DATE__ " " __TIME__;
  _main_img.setCursor(2, 0);
  _main_img.setTextSize(2);
  _main_img.setTextColor(0xA0A0A0, 0);
  _main_img.writeString(HEADER_STR_0);
  _main_img.setCursor(14, 18);
  _main_img.setTextSize(1);
  _main_img.writeString(HEADER_STR_1);

  StringBuilder txt_render;
  struct utsname sname;
  if (1 != uname(&sname)) {
    txt_render.concatf("%s %s (%s)", sname.sysname, sname.release, sname.machine);
    txt_render.concatf("\n%s", sname.version);
    _main_img.writeString(&txt_render);
    txt_render.clear();
  }
  txt_render.concatf("Window: %dx%d", _main_img.x(), _main_img.y());
  _main_img.writeString(&txt_render);
  txt_render.clear();

  const uint SEARCH_SIZE_QUEUE = element_queue.size();
  bool local_ret = false;
  for (uint n = 0; n < SEARCH_SIZE_QUEUE; n++) {
    GfxUIElement* ui_obj = element_queue.get(n);
    ui_obj->render(&ui_gfx, true);
  }
}



/*******************************************************************************
* This is a thread to run the GUI.
*******************************************************************************/
void* gui_thread_handler(void*) {
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Started GUI thread.");
  Display* dpy = XOpenDisplay(nullptr);
  bool keep_polling = (nullptr != dpy);
  bool window_ready = false;
  if (keep_polling) {
    PeriodicTimeout refresh_period(20);
    int screen_num = DefaultScreen(dpy);
    //Colormap color_map = XDefaultColormap(dpy, screen_num);
    Window win = XCreateSimpleWindow(
      dpy, RootWindow(dpy, screen_num),
      10, 10,
      800, 600,
      1,
      0x9932CC,  // TODO: Use colormap.
      0x000000   // TODO: Use colormap.
    );
    _main_img.reallocate();
    _overlay_img.reallocate();
    test_filter_0.init();
    test_filter_1.init();

    element_queue.insert(&_button_0);
    element_queue.insert(&_button_1);
    element_queue.insert(&_button_2);
    element_queue.insert(&_button_3);
    element_queue.insert(&_button_4);
    element_queue.insert(&_button_5);
    element_queue.insert(&_button_6);
    element_queue.insert(&_button_7);

    element_queue.insert(&_slider_0);
    element_queue.insert(&_slider_1);
    element_queue.insert(&_slider_2);
    element_queue.insert(&_slider_3);
    element_queue.insert(&_slider_4);
    element_queue.insert(&_slider_5);
    element_queue.insert(&_slider_6);
    element_queue.insert(&sf_render_0);

    redraw_timer.reset();
    _slider_0.value(0.5);

    GC gc = DefaultGC(dpy, screen_num);
    XSetForeground(dpy, gc, 0xAAAAAA);
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask); // | PointerMotionMask);
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Right Hand of Manuvr");

    Atom WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &WM_DELETE_WINDOW, 1);
    Visual* visual = DefaultVisual(dpy, 0);

    // The thread's polling loop. Repeat forever until told otherwise.
    while (keep_polling) {
      if (0 < XPending(dpy)) {
        XEvent e;
        XNextEvent(dpy, &e);

        switch (e.type) {
          case Expose:
            {
              XWindowAttributes wa;
              XGetWindowAttributes(dpy, win, &wa);
              window_w = (uint32_t) wa.width;
              window_h = (uint32_t) wa.height;
              if ((0 < window_w) && (0 < window_h)) {
                if ((_main_img.x() != window_w) | (_main_img.y() != window_h)) {
                  // TODO: Period-bound this operation so we don't thrash.
                  if (ximage) {
                    ximage->data = nullptr;  // Do not want X11 to free the Image's buffer.
                    XDestroyImage(ximage);
                    ximage = nullptr;
                  }
                  if (_main_img.setSize(window_w, window_h)) {
                    if (_overlay_img.setSize(window_w, window_h)) {
                      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "uApp frame buffer resized to %u x %u x %u", _overlay_img.x(), _overlay_img.y(), _overlay_img.bitsPerPixel());
                      ximage = XCreateImage(dpy, visual, DefaultDepth(dpy, screen_num), ZPixmap, 0, (char*)_overlay_img.buffer(), _overlay_img.x(), _overlay_img.y(), 32, 0);
                      render_all_elements();
                    }
                    else {
                      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Overlay frame buffer resize failed.");
                    }
                  }
                  else {
                    c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "uApp frame buffer resize failed.");
                  }
                }
              }
              window_ready = (nullptr != ximage) & _main_img.allocated();
            }
            break;

          case ButtonPress:
          case ButtonRelease:
            proc_mouse_button(e.xbutton.button, e.xbutton.x, e.xbutton.y, (e.type == ButtonPress));
            break;

          case KeyPress:
            {
              char buf[128] = {0, };
              KeySym keysym;
              XLookupString(&e.xkey, buf, sizeof buf, &keysym, nullptr);
              if (keysym == XK_Escape) {
                keep_polling = false;
              }
              else {
                c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Key press %s: ", buf);
              }
            }
            break;

          case ClientMessage:
            if (static_cast<unsigned int>(e.xclient.data.l[0]) == WM_DELETE_WINDOW) {
              keep_polling = false;
            }
            break;

          case MotionNotify:
            pointer_x = e.xmotion.x;
            pointer_y = e.xmotion.y;
            break;

          default:
            c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unhandled: %d", e.type);
            break;
        }
      }

      if (window_ready & refresh_period.expired()) {
        refresh_period.reset();
        redraw_timer.markStart();
        int garbage = 0;
        int temp_ptr_x = 0;
        int temp_ptr_y = 0;
        uint mask_ret = 0;
        Window win_ret;
        XQueryPointer(dpy, win,
          &win_ret, &win_ret,
          &garbage, &garbage,
          &temp_ptr_x, &temp_ptr_y,
          &mask_ret
        );

        // Render the UI elements...
        const uint SEARCH_SIZE_QUEUE = element_queue.size();
        bool local_ret = false;
        for (uint n = 0; n < SEARCH_SIZE_QUEUE; n++) {
          GfxUIElement* ui_obj = element_queue.get(n);
          ui_obj->render(&ui_gfx, true);
        }

        if (_overlay_img.setBufferByCopy(_main_img.buffer(), _main_img.format())) {
          const bool  POINTER_IN_WINDOW = (0 <= temp_ptr_x) && (0 <= temp_ptr_y) && (window_w > (uint) temp_ptr_x) && (window_h > (uint) temp_ptr_y);
          if (POINTER_IN_WINDOW) {
            // If the pointer is within the window...
            pointer_x = (uint) temp_ptr_x;
            pointer_y = (uint) temp_ptr_y;

            const uint  INSET_X_POS = (window_w - INSET_SIZE) - 1;
            const uint  INSET_Y_POS = (window_h - INSET_SIZE) - 1;
            const float INSET_SCALE = 0.4 + (_slider_0.value() * 5.0);
            const int   INSET_FEED_SIZE   = (INSET_SIZE / INSET_SCALE);
            const int   INSET_FEED_OFFSET = (INSET_FEED_SIZE/2);
            const uint  INSET_FEED_X_POS  = (uint) strict_max(INSET_FEED_OFFSET, strict_min((int) (window_w - INSET_FEED_OFFSET), (int) pointer_x)) - INSET_FEED_OFFSET;
            const uint  INSET_FEED_Y_POS  = (uint) strict_max(INSET_FEED_OFFSET, strict_min((int) (window_h - INSET_FEED_OFFSET), (int) pointer_y)) - INSET_FEED_OFFSET;
            //c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "(%u %u)\t(%u %u)", INSET_FEED_X_POS, INSET_FEED_Y_POS, pointer_x, pointer_y);
            ImageScaler scale_window(&_main_img, &_overlay_img, INSET_SCALE, INSET_FEED_X_POS, INSET_FEED_Y_POS, INSET_FEED_SIZE, INSET_FEED_SIZE, INSET_X_POS, INSET_Y_POS);
            scale_window.apply();

            //_overlay_img.fillRect(pointer_x, pointer_y, 5, 5, 0xFFFFFF);
            _overlay_img.drawRect(INSET_FEED_X_POS, INSET_FEED_Y_POS, INSET_FEED_SIZE, INSET_FEED_SIZE, 0xFFFFFF);
            _overlay_img.drawLine(INSET_FEED_X_POS, (INSET_FEED_Y_POS + INSET_FEED_SIZE), INSET_X_POS, (INSET_Y_POS + INSET_SIZE), 0xFFFFFF);
            _overlay_img.drawLine((INSET_FEED_X_POS + INSET_FEED_SIZE), INSET_FEED_Y_POS, (INSET_X_POS + INSET_SIZE), INSET_Y_POS, 0xFFFFFF);
            _overlay_img.drawRect(INSET_X_POS, INSET_Y_POS, INSET_SIZE, INSET_SIZE, 0xFFFFFF);
          }
          XPutImage(dpy, win, DefaultGC(dpy, DefaultScreen(dpy)), ximage, 0, 0, 0, 0, _overlay_img.x(), _overlay_img.y());
        }
        redraw_timer.markStop();
        test_filter_0.feedFilter(redraw_timer.lastTime());
      }

      // If either this thread, or the main thread decided we should terminate,
      //   don't run another polling loop.
      keep_polling &= continue_running;
    }
    // We are about to end the thread.
    // Clean up the resources we allocated.
    if (ximage) {
      ximage->data = nullptr;  // Do not want X11 to free the Image's buffer.
      XDestroyImage(ximage);
      ximage = nullptr;
    }
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
  }
  else {
    c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Cannot open display.");
  }

  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Exiting GUI thread...");
  gui_thread_id = 0;
  if (gravepact) {
    continue_running = false;
  }
  return nullptr;
}



int callback_gui_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = -1;
  // NOTE: The GUI is running in a separate thread. It should only be
  //   manipulated indirectly by an IPC mechanism, or by a suitable stand-ins.
  char* cmd = args->position_trimmed(0);
  if (0 == StringBuilder::strcasecmp(cmd, "resize")) {
    // NOTE: This is against GUI best-practices (according to Xorg). But it
    //   might be useful later.
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "border-pix")) {
    //XSetWindowBorder(dpy, win, 40);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "gravepact")) {
    // If this is enabled, and the user closes the GUI, the main program will
    //   also terminate in an orderly manner.
    if (1 < args->count()) {
      gravepact = (0 != args->position_as_int(1));
    }
    text_return->concatf("Closing the GUI will %sterminate the entire program.\n", gravepact?"":"not ");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "detatched-modals")) {
    // If enabled, this setting causes modals to be spawned off as distinct
    //   child windows. If disabled, you will get an overlay instead.
  }
  return ret;
}
