/*
* File:   X11Window.cpp
* Author: J. Ian Lindsay
*
*/

#include "C3POnX11.h"
#include "Linux.h"

const char* const LOG_TAG = "C3Px11Window";


/*******************************************************************************
* This is a thread to run the GUI.
*******************************************************************************/
void* gui_thread_handler(void* _ptr) {
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Started GUI thread.");
  C3Px11Window* ptr = (C3Px11Window*) _ptr;
  // The thread's polling loop. Repeat forever until told otherwise.
  while (0 <= ptr->poll()) {}
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Exiting GUI thread...");
  ptr->_thread_id = 0;
  return nullptr;
}


/*******************************************************************************
* Class members
*******************************************************************************/

// TODO: This is NOT a class member, but will be once it is decided if it ought
//   to be a list or a singleton.
GfxUIElement*   _pointer_client = nullptr;
GfxUIElement*   _mrtlhne = nullptr;  // Most-recent top-level hover-notified element. Suffer.


/**
* Constructor
*/
C3Px11Window::C3Px11Window(uint32_t win_x, uint32_t win_y, uint32_t win_w, uint32_t win_h, const char* TITLE)
  : root(0, 0, (uint16_t) win_w, (uint16_t) win_h, 0),
  gfx(&_fb), gfx_overlay(&_overlay),
  _title(TITLE), _pointer_x(0), _pointer_y(0), _thread_id(0),
  _ximage(nullptr), _screen_num(0), _refresh_period(20),
  _fb(win_w, win_h, ImgBufferFormat::R8_G8_B8_ALPHA),
  _overlay(win_w, win_h, ImgBufferFormat::R8_G8_B8_ALPHA),
  _vc_callback(nullptr),
  _keep_polling(true) {}


/**
* Destructor
*/
C3Px11Window::~C3Px11Window() {
  // Sleep until thread ends.
  while (0 != _thread_id) {  sleep_ms(10);  }

  // Clean up the resources we allocated.
  if (_ximage) {
    _ximage->data = nullptr;  // Do not want X11 to free the Image's buffer.
    XDestroyImage(_ximage);
    _ximage = nullptr;
  }
  XDestroyWindow(_dpy, _win);
  XCloseDisplay(_dpy);
}



int8_t C3Px11Window::_init_window() {
  int8_t ret = -1;

  _dpy = XOpenDisplay(nullptr);
  _screen_num = DefaultScreen(_dpy);
  _visual = DefaultVisual(_dpy, 0);

  if ((0 == root.elementWidth()) || (0 == root.elementHeight())) {
    // If the ordered pair for Size is 0, Interpret this to
    //   be shorthand for full-screen.
    root.reposition(0, 0);
    root.resize(
      DisplayWidth(_dpy, _screen_num),
      DisplayHeight(_dpy, _screen_num)
    );
  }
  //Colormap color_map = XDefaultColormap(dpy, screen_num);
  if (_overlay.setSize(width(), height())) {
    ret--;
    if (_fb.setSize(width(), height())) {
      ret--;
      _win = XCreateSimpleWindow(
        _dpy, RootWindow(_dpy, _screen_num),
        root.elementPosX(), root.elementPosY(),
        width(), height(),
        1,
        0x9932CC,  // TODO: Use colormap.
        0x000000   // TODO: Use colormap.
      );

      if (_win) {
        _redraw_timer.reset();
        GC gc = DefaultGC(_dpy, _screen_num);
        XSetForeground(_dpy, gc, 0xAAAAAA);
        XSelectInput(_dpy, _win, ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | PointerMotionMask);
        XMapWindow(_dpy, _win);
        XStoreName(_dpy, _win, _title);

        Atom WM_DELETE_WINDOW = XInternAtom(_dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(_dpy, _win, &WM_DELETE_WINDOW, 1);
        _keep_polling = true;
        ret = 0;
        platform.createThread(&_thread_id, nullptr, gui_thread_handler, (void*) this, nullptr);
      }
    }
  }

  return ret;
}


int8_t C3Px11Window::_deinit_window() {
  int8_t ret = 0;
  _keep_polling = false;
  return ret;
}



int8_t C3Px11Window::_refit_window() {
  int8_t ret = 0;
  XWindowAttributes wa;
  XGetWindowAttributes(_dpy, _win, &wa);
  uint32_t new_width  = (uint32_t) wa.width;
  uint32_t new_height = (uint32_t) wa.height;

  if ((width() != new_width) || (height() != new_height)) {
    ret--;
    if ((0 < new_width) && (0 < new_height)) {
      ret = 0;
      root.resize(new_width, new_height);
    }
  }
  return ret;
}


/*
* This will mutate the state of the pointer as the window reckons.
*/
int8_t C3Px11Window::_process_motion() {
  int8_t ret = -1;
  if (_win) {
    ret--;
    if (pointerInWindow()) {
      ret = 0;
      PriorityQueue<GfxUIElement*> change_log;
      if (nullptr != _pointer_client) {
        _pointer_client->notify(GfxUIEvent::DRAG_START, _pointer_x, _pointer_y, &change_log);
        ret++;
      }

      GfxUIElement* current_hover = elementUnderPointer();
      if (nullptr != current_hover) {
        if (nullptr == _mrtlhne) {
          current_hover->notify(GfxUIEvent::HOVER_IN, _pointer_x, _pointer_y, &change_log);
          ret++;
        }
        else if ((_mrtlhne != current_hover) || (current_hover->trackPointer())) {
          _mrtlhne->notify(GfxUIEvent::HOVER_OUT, _pointer_x, _pointer_y, &change_log);
          current_hover->notify(GfxUIEvent::HOVER_IN, _pointer_x, _pointer_y, &change_log);
          ret++;
        }
      }
      else if (nullptr != _mrtlhne) {
        _mrtlhne->notify(GfxUIEvent::HOVER_OUT, _pointer_x, _pointer_y, &change_log);
        _mrtlhne = nullptr;
        ret++;
      }

      if (0 < ret) {  _mrtlhne = current_hover;   }
      _proc_changelog(&change_log);
    }
  }
  return ret;
}


/*
* This will mutate the state of the pointer as the window reckons.
*/
int8_t C3Px11Window::_query_pointer() {
  int8_t ret = -1;
  if (_win) {
    int garbage = 0;
    uint32_t mask_ret = 0;
    Window win_ret;
    XQueryPointer(_dpy, _win,
      &win_ret, &win_ret,
      &garbage, &garbage,
      &_pointer_x, &_pointer_y,
      &mask_ret
    );
    ret = 0;
  }
  return ret;
}


/*
* Locate and return the element under the pointer.
* Depth-first search. So the result will be the inner-most element that contains
*   the current pointer location.
*/
GfxUIElement* C3Px11Window::elementUnderPointer() {
  GfxUIElement* ret = nullptr;
  //_query_pointer();
  if (pointerInWindow()) {
    PriorityQueue<GfxUIElement*> change_log;
    root.notify(GfxUIEvent::IDENTIFY, _pointer_x, _pointer_y, &change_log);
    if (change_log.hasNext()) {
      ret = change_log.dequeue();
    }
  }
  return ret;
}



int8_t C3Px11Window::map_button_inputs(MouseButtonDef* list, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    MouseButtonDef* temp = (list + i);
    if (0 > _btn_defs.insert(temp, temp->button_id)) {
      return -1;
    }
  }
  return 0;
}


/*
* This function handles returned events from a notify() cycle.
* This function executes before a redraw, and synchronously with user action.
*/
int8_t C3Px11Window::_proc_changelog(PriorityQueue<GfxUIElement*>* change_log) {
  int8_t ret = 0;
  // Unwind the response events...
  while (change_log->hasNext()) {
    GfxUIEvent    response_event   = (GfxUIEvent) change_log->getPriority(0);
    GfxUIElement* response_element = change_log->dequeue();
    switch (response_event) {
      case GfxUIEvent::DRAG_START:
        //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Drag start %p", _pointer_client);
        _pointer_client = response_element;
        break;
      case GfxUIEvent::DRAG_STOP:
        //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Drag stop %p", _pointer_client);
        if (_pointer_client == response_element) {
          _pointer_client = nullptr;  // No ceremony required.
        }
        break;
      case GfxUIEvent::VALUE_CHANGE:
        if (_vc_callback) {
          _vc_callback(response_element);
        }
        break;
      default:
        break;
    }
  }
  return ret;
}




int8_t C3Px11Window::_proc_mouse_button(uint16_t btn_id, uint32_t x, uint32_t y, bool pressed) {
  int8_t ret = 0;
  MouseButtonDef* btn = _btn_defs.getByPriority(btn_id);
  if (nullptr != btn) {
    PriorityQueue<GfxUIElement*> change_log;
    GfxUIElement* current_hover = elementUnderPointer();
    const GfxUIEvent event = pressed ? btn->gfx_event_down : btn->gfx_event_up;

    switch (event) {
      case GfxUIEvent::RELEASE:
        if ((nullptr != _pointer_client) && (_pointer_client != current_hover)) {
          // If we are keeping an object aprised on the state of the mouse, it
          //   needs to know about the release of the button, JiC it didn't
          //   occur while hovering over the element that initiated the drag.
          // TODO: Cross-element drag-and-drop should go here.
          //c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Terminal drag notify %p", _pointer_client);
          _pointer_client->notify(GfxUIEvent::RELEASE, x, y, &change_log);
          _pointer_client = nullptr;
        }
        // NOTE: No break;
      default:
        if (root.notify(event, x, y, &change_log)) {
          ret = 1;
        }
        else {
          //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "%s %s: (%d, %d) (no target)", btn->label, (pressed ? "click" : "release"), x, y);
        }
        break;
      case GfxUIEvent::NONE:
      case GfxUIEvent::INVALID:
        break;
    }
    _proc_changelog(&change_log);   // Unwind the response events.
  }
  //else c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Unhandled %s %d: (%d, %d)", (pressed ? "click" : "release"), btn_id, x, y);
  return ret;
}



int8_t C3Px11Window::_redraw_window() {
  int8_t ret = 0;

  if (_refresh_period.expired()) {
    ret--;
    if ((_fb.x() != width()) | (_fb.y() != height())) {
      if (_ximage) {
        _ximage->data = nullptr;  // Do not want X11 to free the Image's buffer.
        XDestroyImage(_ximage);
        _ximage = nullptr;
      }
      if (_overlay.setSize(width(), height())) {
        if (_fb.setSize(width(), height())) {
          _ximage = XCreateImage(_dpy, _visual, DefaultDepth(_dpy, _screen_num), ZPixmap, 0, (char*)_overlay.buffer(), _overlay.x(), _overlay.y(), 32, 0);
          if (_ximage) {
            c3p_log(LOG_LEV_DEBUG, "_redraw_window()", "Frame buffer resized to %u x %u x %u", _fb.x(), _fb.y(), _fb.bitsPerPixel());
          }
        }
      }
    }

    if (_fb.allocated()) {
      ret--;
      _refresh_period.reset();
      _redraw_timer.markStart();
      _query_pointer();

      render(true);
      root.render(&gfx, true);  // Render the static frame-buffer.

      if (_overlay.allocated() && (_overlay.y() == _fb.y()) && (_overlay.x() == _fb.x())) {
        if (_ximage) {
          if (_overlay.setBufferByCopy(_fb.buffer(), _fb.format())) {
            if (pointerInWindow()) {
              render_overlay();
            }
            XPutImage(_dpy, _win, DefaultGC(_dpy, DefaultScreen(_dpy)), _ximage, 0, 0, 0, 0, _overlay.x(), _overlay.y());
            ret = 1;
          }
        }
        else {
          _ximage = XCreateImage(_dpy, _visual, DefaultDepth(_dpy, _screen_num), ZPixmap, 0, (char*)_overlay.buffer(), _overlay.x(), _overlay.y(), 32, 0);
          ret = 0;
        }
      }
    }
  }

  if (1 == ret) {
    _redraw_timer.markStop();
  }

  return ret;
}
