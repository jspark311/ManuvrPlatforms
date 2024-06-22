/*
* File:   X11Window.cpp
* Author: J. Ian Lindsay
*
*/

#include "RHoM.h"
#include "C3POnX11.h"

#define CONSOLE_INPUT_HEIGHT  200
#define TEST_FILTER_DEPTH     700
#define ELEMENT_MARGIN          3

extern bool continue_running;         // TODO: (rolled up newspaper) Bad...
extern M2MLink* m_link;
extern ParsingConsole console;

bool gravepact      = true;   // Closing the GUI window should terminate the main thread?
bool mlink_onscreen = false;  // Has the Link object been rendered?


/*******************************************************************************
* UI definition
*******************************************************************************/

// Create a simple console window, with a full frame.
GfxUITabbedContentPane _main_nav(
  GfxUILayout(
    0, 0,
    1024, 768,
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xFFFFFF,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  ),
  0 //(GFXUI_FLAG_DRAW_FRAME_MASK)
);

GfxUIGroup _main_nav_transports(0, 0, 0, 0);
GfxUIGroup _main_nav_links(0, 0, 0, 0);
GfxUIGroup _main_nav_settings(0, 0, 0, 0);


GfxUIStyle base_style;

//base_style.color_bg          = 0;
//base_style.color_border      = 0xFFFFFF;
//base_style.color_header      = 0x20B2AA;
//base_style.color_active      = 0x20B2AA;
//base_style.color_inactive    = 0xA0A0A0;
//base_style.color_selected    = 0x202020;
//base_style.color_unselected  = 0x202020;
//base_style.text_size         = 2;

// Create a text window, into which we will write running filter stats.
GfxUITextArea _program_info_txt(
  GfxUILayout(
    0, 0,
    500, 60,
    0, 0, 0, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xC0C0C0,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  )
);


GfxUIKeyValuePair _kvp_test_0(
  rhom_conf.getKVP(),
  GfxUILayout(
    _program_info_txt.elementPosX(), (_program_info_txt.elementPosY() + _program_info_txt.elementHeight()),
    240, 140,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    1, 1, 1, 1               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xfefe00,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (0)
);



MouseButtonDef mouse_buttons[] = {
  { .label = "Left",
    .button_id = 1,
    .gfx_event_down = GfxUIEvent::TOUCH,
    .gfx_event_up   = GfxUIEvent::RELEASE
  },
  { .label = "Middle",
    .button_id = 2,
    .gfx_event_down = GfxUIEvent::DRAG,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .label = "Right",
    .button_id = 3,
    .gfx_event_down = GfxUIEvent::SELECT,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .label = "ScrlUp",
    .button_id = 4,
    .gfx_event_down = GfxUIEvent::MOVE_UP,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .label = "ScrlDwn",
    .button_id = 5,
    .gfx_event_down = GfxUIEvent::MOVE_DOWN,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .label = "TiltLeft",
    .button_id = 6,
    .gfx_event_down = GfxUIEvent::MOVE_LEFT,
    .gfx_event_up   = GfxUIEvent::NONE
  },
  { .label = "TiltRight",
    .button_id = 7,
    .gfx_event_down = GfxUIEvent::MOVE_RIGHT,
    .gfx_event_up   = GfxUIEvent::NONE
  }
};


C3PScheduledLambda schedule_ts_update(
  "ts_update",
  31000, -1, true,
  []() {
    const uint32_t RAND_NUM = (randomUInt32() & 0x000FFFFF);
    return 0;
  }
);




void ui_value_change_callback(GfxUIElement* element) {
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "VALUE_CHANGE %p", element);
}



int8_t MainGuiWindow::createWindow() {
  int8_t ret = _init_window();
  if (0 == ret) {
    map_button_inputs(mouse_buttons, sizeof(mouse_buttons) / sizeof(mouse_buttons[0]));
    _overlay.reallocate();

    _main_nav_settings.add_child(&_program_info_txt);
    _main_nav_settings.add_child(&_kvp_test_0);

  if (!mlink_onscreen && (nullptr != m_link)) {
    GfxUIMLink* mlink_ui_obj = new GfxUIMLink(
      GfxUILayout(
        0, 0,
        600, 500,
        1, 1, 1, 1,
        0, 0, 0, 0  // Border_px(t, b, l, r)
      ),
      GfxUIStyle(0, // bg
        0xFFFFFF,   // border
        0xFFFFFF,   // header
        0x8020C0,
        0xA0A0A0,   // inactive
        0xFFFFFF,   // selected
        0x202020,   // unselected
        2
      ),
      m_link,
      (GFXUI_FLAG_DRAW_FRAME_MASK | GFXUI_FLAG_ALWAYS_REDRAW)
    );
    mlink_ui_obj->shouldReap(true);
    _main_nav_links.add_child(mlink_ui_obj);
    mlink_onscreen = true;
  }

    // Adding the contant panes will cause the proper screen co-ords to be imparted
    //   to the group objects. We can then use them for element flow.
    _main_nav.addTab("Transports", &_main_nav_transports);
    _main_nav.addTab("Links", &_main_nav_links);
    _main_nav.addTab("Settings", &_main_nav_settings);
    root.add_child(&_main_nav);

    _refresh_period.reset();
    setCallback(ui_value_change_callback);
    C3PScheduler::getInstance()->addSchedule(&schedule_ts_update);
  }
  return ret;
}



int8_t MainGuiWindow::closeWindow() {
  continue_running = !gravepact;
  return _deinit_window();
}



int8_t MainGuiWindow::render_overlay() {
  // If the pointer is within the window, we note its location and
  //   annotate the overlay.
  ui_magnifier.pointerLocation(_pointer_x, _pointer_y);
  ui_magnifier.render(&gfx_overlay);
  return 0;
}


/*
* Called to unconditionally show the elements in the GUI.
*/
int8_t MainGuiWindow::render(bool force) {
  int8_t ret = 0;
  if (force) {
    //const uint  CONSOLE_INPUT_X_POS = 0;
    //const uint  CONSOLE_INPUT_Y_POS = (height() - CONSOLE_INPUT_HEIGHT) - 1;
    // _txt_area_0.reposition(CONSOLE_INPUT_X_POS, CONSOLE_INPUT_Y_POS);
    // _txt_area_0.resize(width(), CONSOLE_INPUT_HEIGHT);
    StringBuilder pitxt;
    pitxt.concat("Right Hand of Manuvr\nBuild date " __DATE__ " " __TIME__);
    struct utsname sname;
    if (1 != uname(&sname)) {
      pitxt.concatf("%s %s (%s)", sname.sysname, sname.release, sname.machine);
      pitxt.concatf("\n%s", sname.version);
    }
    pitxt.concatf("Window: %dx%d", _fb.x(), _fb.y());
    _program_info_txt.clear();
    _program_info_txt.pushBuffer(&pitxt);
  }
  return ret;
}



// Called from the thread.
int8_t MainGuiWindow::poll() {
  int8_t ret = 0;

  if (0 < XPending(_dpy)) {
    Atom WM_DELETE_WINDOW = XInternAtom(_dpy, "WM_DELETE_WINDOW", False);
    Atom UTF8      = XInternAtom(_dpy, "UTF8_STRING", True);
    Atom CLIPBOARD = XInternAtom(_dpy, "CLIPBOARD", 0);
    Atom PRIMARY   = XInternAtom(_dpy, "PRIMARY", 0);
    XEvent e;
    XNextEvent(_dpy, &e);

    switch (e.type) {
      case SelectionNotify:
        c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "SelectionNotify");
        if ((CLIPBOARD == e.xselection.selection) || (PRIMARY == e.xselection.selection)) {
          if (e.xselection.property) {
            uint8_t* data = nullptr;
            Atom target;
            int format;
            unsigned long ele_count;
            unsigned long size;
            XGetWindowProperty(e.xselection.display, e.xselection.requestor, e.xselection.property, 0L,(~0L), 0, AnyPropertyType, &target, &format, &size, &ele_count, &data);
            if ((target == UTF8) || (target == XA_STRING)) {
              StringBuilder deep_copy(data, size);
              c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, &deep_copy);
              XFree(data);
            }
            else {
              c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "target: %d", (int) target);
            }
            XDeleteProperty(e.xselection.display, e.xselection.requestor, e.xselection.property);
          }
        }
        break;
      case SelectionRequest:
        c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "SelectionRequest");
        break;
      case SelectionClear:
        c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "SelectionClear");
        break;
      case PropertyNotify:
        c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "PropertyNotify");
        break;


      case Expose:
        {
          // Try to resize the window. If it isn't required, _refit_window()
          //   will return zero.
          int8_t local_ret = _refit_window();
          if (0 != local_ret) {
            c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Window resize failed (%d).", local_ret);
          }
        }
        break;

      case ButtonPress:
      case ButtonRelease:
        {
          uint16_t btn_id = e.xbutton.button;
          int8_t ret = _proc_mouse_button(btn_id, e.xbutton.x, e.xbutton.y, (e.type == ButtonPress));
          if (0 == ret) {
            // Any unclaimed input can be handled in this block.
            const GfxUIEvent event = (btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP;
            switch (btn_id) {
              case 4:
              case 5:
                // Unhandled scroll events adjust the magnifier scale.
                //ui_magnifier.notify(
                //  ((btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP),
                //  ui_magnifier.elementPosX(), ui_magnifier.elementPosY()
                //);
              default:
                break;
            }
          }
        }
        break;


      case KeyRelease:
        {
          char buf[128] = {0, };
          KeySym keysym;
          XLookupString(&e.xkey, buf, sizeof(buf), &keysym, nullptr);
          if ((keysym == XK_Control_L) | (keysym == XK_Control_R)) {
            _modifiers.clear(RHOM_GUI_MOD_CTRL_HELD);
          }
          else if ((keysym == XK_Alt_L) | (keysym == XK_Alt_R)) {
            _modifiers.clear(RHOM_GUI_MOD_ALT_HELD);
          }
        }
        break;


      case KeyPress:
        {
          char buf[128] = {0, };
          KeySym keysym;
          int ret_local = XLookupString(&e.xkey, buf, sizeof(buf), &keysym, nullptr);
          if (keysym == XK_Escape) {
            _keep_polling = false;
          }
          else if (keysym == XK_Return) {
          }
          else if ((keysym == XK_Control_L) | (keysym == XK_Control_R)) {
            _modifiers.set(RHOM_GUI_MOD_CTRL_HELD);
          }
          else if ((keysym == XK_Alt_L) | (keysym == XK_Alt_R)) {
            _modifiers.set(RHOM_GUI_MOD_ALT_HELD);
          }
          else if (1 == ret_local) {
            StringBuilder _tmp_sbldr;
            _tmp_sbldr.concat(buf[0]);
            console.pushBuffer(&_tmp_sbldr);
          }
          else {
            c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Key press: %s (%s)", buf, XKeysymToString(keysym));
          }
        }
        break;

      case ClientMessage:
        if (static_cast<unsigned int>(e.xclient.data.l[0]) == WM_DELETE_WINDOW) {
          _keep_polling = false;
        }
        else {
          //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "ClientMessage");
        }
        break;

      case MotionNotify:
        _pointer_x = e.xmotion.x;
        _pointer_y = e.xmotion.y;
        //c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "_process_motion(%d, %d) returns %d.", _pointer_x, _pointer_y, _process_motion());
        _process_motion();
        break;

      default:
        c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Unhandled XEvent: %d", e.type);
        break;
    }
  }

  if (_keep_polling) {
    // Offer to render the UI elements...
    if (1 == _redraw_window()) {
      // If a redraw happened...
    }
  }
  else {
    closeWindow();
    ret = -1;
  }

  return ret;
}
