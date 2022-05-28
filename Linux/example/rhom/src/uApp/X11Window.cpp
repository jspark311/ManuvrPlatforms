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


/*******************************************************************************
* This is a thread to run the GUI.
*******************************************************************************/
void* gui_thread_handler(void*) {
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Started GUI thread.");
  Display* dpy = XOpenDisplay(nullptr);
  bool keep_polling = (nullptr != dpy);
  bool window_ready = false;
  if (keep_polling) {
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
    //_main_img.reallocate();
    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Reallocated %d", _main_img.reallocate());

    GC gc = DefaultGC(dpy, screen_num);
    XSetForeground(dpy, gc, 0xAAAAAA);
    XSelectInput(dpy, win, ExposureMask | ButtonPress | ButtonRelease | KeyPressMask);
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Right Hand of Manuvr");

    Atom WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &WM_DELETE_WINDOW, 1);
    bool uname_ok = false;
    struct utsname sname;
    int ret = uname(&sname);
    if (ret != -1) {
      uname_ok = true;
    }
    XEvent e;
    Visual* visual = DefaultVisual(dpy, 0);


    while (keep_polling) {
      if (0 < XPending(dpy)) {
        bool handled = false;
        XNextEvent(dpy, &e);

        if (e.type == Expose) {
          handled = true;
          XWindowAttributes wa;
          XGetWindowAttributes(dpy, win, &wa);
          int width = wa.width;
          int height = wa.height;
          char buf[256] = {0, };
          int y_offset = 10;
          const char* HEADER_STR_0 = "Right Hand of Manuvr";
          const char* HEADER_STR_1 = "Build date " __DATE__ " " __TIME__;
          if ((0 < width) && (0 < height)) {
            if ((_main_img.x() != width) | (_main_img.y() != height)) {
              if (ximage) {
                ximage->data = nullptr;  // Do not want X11 to free the Image's buffer.
                XDestroyImage(ximage);
                ximage = nullptr;
              }
              if (_main_img.setSize((uint32_t) width, (uint32_t) height)) {
                ximage = XCreateImage(dpy, visual, DefaultDepth(dpy, screen_num), ZPixmap, 0, (char*)_main_img.buffer(), _main_img.x(), _main_img.y(), 32, 0);
              }
              else {
                c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "setSize() failed.");
              }
            }
          }

          _main_img.setCursor(2, 0);
          _main_img.setTextSize(2);
          _main_img.setTextColor(0xA0A0A0, 0);
          _main_img.writeString(HEADER_STR_0);
          _main_img.setCursor(14, 18);
          _main_img.setTextSize(1);
          _main_img.writeString(HEADER_STR_1);

          StringBuilder txt_render;
          if (uname_ok) {
            txt_render.concatf("%s %s (%s)", sname.sysname, sname.release, sname.machine);
            txt_render.concatf("\n%s", sname.version);
            _main_img.writeString(&txt_render);
            txt_render.clear();
          }
          txt_render.concatf("Window: %dx%d", width, height);
          _main_img.writeString(&txt_render);
          txt_render.clear();
          window_ready = (nullptr != ximage);
        }

        if (e.type == ButtonPress) {
          handled = true;
          char* btn_str = (char*) "";
          switch (e.xbutton.button) {
            case 1:    btn_str = (char*) "Left";
              break;
            case 2:    btn_str = (char*) "Middle";
              break;
            case 3:    btn_str = (char*) "Right";
              break;
            case 4:    btn_str = (char*) "ScrlUp";      break;
            case 5:    btn_str = (char*) "ScrlDwn";     break;
            default:   btn_str = (char*) "Unhandled";   break;
          }
          c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "%s click %d: (%d, %d)", btn_str, e.xbutton.state, e.xbutton.x, e.xbutton.y);
          _main_img.setCursor(0, 0);
          _main_img.setTextSize(2);
          _main_img.setTextColor(0xFFFFFF, 0);
          _main_img.writeString(btn_str);
          _main_img.writeString("      ");
        }
        if (e.type == ButtonRelease) {
          handled = true;
          char* btn_str = (char*) "";
          switch (e.xbutton.button) {
            case 1:    btn_str = (char*) "Left";        break;
            case 2:    btn_str = (char*) "Middle";      break;
            case 3:    btn_str = (char*) "Right";       break;
            case 4:    btn_str = (char*) "ScrlUp";      break;
            case 5:    btn_str = (char*) "ScrlDwn";     break;
            default:   btn_str = (char*) "Unhandled";   break;
          }
          c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "%s release %d: (%d, %d)", btn_str, e.xbutton.state, e.xbutton.x, e.xbutton.y);
        }

        if (e.type == KeyPress) {
          handled = true;
          char buf[128] = {0, };
          KeySym keysym;
          int len = XLookupString(&e.xkey, buf, sizeof buf, &keysym, NULL);
          if (keysym == XK_Escape) {
            keep_polling = false;
          }
          else {
            c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Key press %s: ", buf);
          }
        }

        if ((e.type == ClientMessage) && (static_cast<unsigned int>(e.xclient.data.l[0]) == WM_DELETE_WINDOW)) {
          handled = true;
          keep_polling = false;
        }

        if (!handled) {
          c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unhandled: %d", e.type);
        }
      }

      if (window_ready) {
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

  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Exiting GUI thread...");
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
