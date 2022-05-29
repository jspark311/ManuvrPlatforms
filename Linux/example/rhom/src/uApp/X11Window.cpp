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


extern unsigned long gui_thread_id;   // TODO: (rolled up newspaper) Bad...
extern bool continue_running;         // TODO: (rolled up newspaper) Bad...

extern SensorFilter<uint32_t> _filter;

Display* dpy = nullptr;
XImage* ximage = nullptr;

Image _main_img(640, 480, ImgBufferFormat::R8_G8_B8_ALPHA);
UIGfxWrapper ui_gfx(&_main_img);

float scroll_val_0 = 0.5f;
SensorFilter<float> test_filter_0(150, FilteringStrategy::RAW);
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


/*
* This is the listing of buttons we will respond to, along with their queues
*   and application response policies.
*/
struct MouseButtonDef {
  const uint button_id;
  const char* const label;
};

MouseButtonDef mouse_buttons[] = {
  { .button_id = 1,
    .label = "Left"
  },
  { .button_id = 2,
    .label = "Middle"
  },
  { .button_id = 3,
    .label = "Right"
  },
  { .button_id = 4,
    .label = "ScrlUp"
  },
  { .button_id = 5,
    .label = "ScrlDwn"
  }
};


PriorityQueue<GfxUIButton*> button_queue;
PriorityQueue<GfxUISlider*> slider_queue;



/*******************************************************************************
* Mouse/touch handlers
*******************************************************************************/

void proc_mouse_button(uint btn_id, uint x, uint y, bool pressed) {
  const uint SEARCH_SIZE_DEF = sizeof(mouse_buttons) / sizeof(MouseButtonDef);
  for (uint i = 0; i < SEARCH_SIZE_DEF; i++) {
    if (mouse_buttons[i].button_id == btn_id) {
      switch (mouse_buttons[i].button_id) {
        case 1:
        case 2:
        case 3:
          {
            const uint SEARCH_SIZE_QUEUE = button_queue.size();
            for (uint n = 0; n < SEARCH_SIZE_QUEUE; n++) {
              GfxUIButton* ui_btn = button_queue.get(n);
              if (ui_btn->includesPoint(x, y)) {
                ui_btn->pressed(pressed);
                ui_btn->render(&ui_gfx);
                return;
              }
            }
          }
          {
            const uint SEARCH_SIZE_QUEUE = slider_queue.size();
            for (uint n = 0; n < SEARCH_SIZE_QUEUE; n++) {
              GfxUISlider* ui_element = slider_queue.get(n);
              if (pressed) {
                if (ui_element->touch(x, y)) {
                  ui_element->render(&ui_gfx);
                  return;
                }
              }
              else return;
            }
          }
          break;
        case 4:
        case 5:
          if (pressed) {
            // We ignore all release events for the scrollwheel.
            const uint SEARCH_SIZE_QUEUE = slider_queue.size();
            for (uint n = 0; n < SEARCH_SIZE_QUEUE; n++) {
              GfxUISlider* ui_element = slider_queue.get(n);
              if (ui_element->includesPoint(x, y)) {
                if (mouse_buttons[i].button_id == 4) {
                  ui_element->value(strict_min(1.0, (ui_element->value() + 0.02)));
                  test_filter_0.feedFilter(ui_element->value());
                }
                else {
                  ui_element->value(strict_max(0.0, (ui_element->value() - 0.02)));
                  test_filter_0.feedFilter(ui_element->value());
                }
                ui_element->render(&ui_gfx);
                return;
              }
            }
          }
          else {
            return;
          }
          break;
      }
      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "%s %s: (%d, %d) (no target)", mouse_buttons[i].label, (pressed ? "click" : "release"), x, y);
      return;
    }
  }
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Unhandled %s %d: (%d, %d)", (pressed ? "click" : "release"), btn_id, x, y);
}



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

  _button_0.render(&ui_gfx);
  _button_1.render(&ui_gfx);
  _button_2.render(&ui_gfx);
  _button_3.render(&ui_gfx);
  _button_4.render(&ui_gfx);
  _button_5.render(&ui_gfx);
  _button_6.render(&ui_gfx);
  _button_7.render(&ui_gfx);

  _slider_0.render(&ui_gfx);
  _slider_1.render(&ui_gfx);
  _slider_2.render(&ui_gfx);
  _slider_3.render(&ui_gfx);
  _slider_4.render(&ui_gfx);
  _slider_5.render(&ui_gfx);
  _slider_6.render(&ui_gfx);

  ui_gfx.drawGraph(
    280, 125, test_filter_0.windowSize(), 120, 0xA05010,
    true, true, true,
    &test_filter_0
  );
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
      10,
      10,
      800,
      800,
      1,
      0x9932CC,  // TODO: Use colormap.
      0x000000   // TODO: Use colormap.
    );
    _main_img.reallocate();
    test_filter_0.init();
    test_filter_1.init();

    button_queue.insert(&_button_0);
    button_queue.insert(&_button_1);
    button_queue.insert(&_button_2);
    button_queue.insert(&_button_3);
    button_queue.insert(&_button_4);
    button_queue.insert(&_button_5);
    button_queue.insert(&_button_6);
    button_queue.insert(&_button_7);

    slider_queue.insert(&_slider_0);
    slider_queue.insert(&_slider_1);
    slider_queue.insert(&_slider_2);
    slider_queue.insert(&_slider_3);
    slider_queue.insert(&_slider_4);
    slider_queue.insert(&_slider_5);
    slider_queue.insert(&_slider_6);

    GC gc = DefaultGC(dpy, screen_num);
    XSetForeground(dpy, gc, 0xAAAAAA);
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask);  // PointerMotionMask
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
              uint32_t width  = (uint32_t) wa.width;
              uint32_t height = (uint32_t) wa.height;
              if ((0 < width) && (0 < height)) {
                if ((_main_img.x() != width) | (_main_img.y() != height)) {
                  // TODO: Period-bound this operation so we don't thrash.
                  if (ximage) {
                    ximage->data = nullptr;  // Do not want X11 to free the Image's buffer.
                    XDestroyImage(ximage);
                    ximage = nullptr;
                  }
                  if (_main_img.setSize(width, height)) {
                    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "uApp frame buffer resized to %u x %u x %u", _main_img.x(), _main_img.y(), _main_img.bitsPerPixel());
                    ximage = XCreateImage(dpy, visual, DefaultDepth(dpy, screen_num), ZPixmap, 0, (char*)_main_img.buffer(), _main_img.x(), _main_img.y(), 32, 0);
                    render_all_elements();
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

          default:
            c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unhandled: %d", e.type);
            break;
        }
      }

      if (window_ready & refresh_period.expired()) {
        refresh_period.reset();
        if (test_filter_0.dirty()) {
          ui_gfx.drawGraph(
            280, 125, test_filter_0.windowSize(), 120, 0xA05010,
            true, true, true,
            &test_filter_0
          );
        }
        //if (test_filter_1.dirty()) {
        //  test_filter_1.min()
        //  ui_gfx.drawHeatMap(
        //    0, 260, 128, 128,
        //    uint32_t flags,
        //    float* range_min, float* range_max,
        //    SensorFilter<float>* filt
        //  );
        //}
        XPutImage(dpy, win, DefaultGC(dpy, DefaultScreen(dpy)), ximage, 0, 0, 0, 0, _main_img.x(), _main_img.y());
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
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "detatched-modals")) {
    // If enabled, this setting causes modals to be spawned off as distinct
    //   child windows. If disabled, you will get an overlay instead.
  }
  return ret;
}
