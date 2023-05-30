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
extern SensorFilter<uint32_t> _filter;
extern M2MLink* m_link;
extern ParsingConsole console;

SensorFilter<uint32_t> test_filter_0(TEST_FILTER_DEPTH, FilteringStrategy::RAW);
SensorFilter<float> test_filter_1(TEST_FILTER_DEPTH, FilteringStrategy::RAW);
SensorFilter<float> test_filter_stdev(TEST_FILTER_DEPTH, FilteringStrategy::RAW);

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


GfxUIGroup _main_nav_data_viewer(0, 0, 0, 0);
GfxUIGroup _main_nav_crypto(0, 0, 0, 0);
GfxUIGroup _main_nav_links(0, 0, 0, 0);
GfxUIGroup _main_nav_console(0, 0, 0, 0);
GfxUIGroup _main_nav_internals(0, 0, 0, 0);
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



// Graph the screen re-draw period.
GfxUITimeSeriesDetail<uint32_t> data_examiner(
  GfxUILayout(
    0, 0,                    // Position(x, y)
    TEST_FILTER_DEPTH, 500,  // Size(w, h)
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,  // Margins_px(t, b, l, r)
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(
    0,          // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x40B0D0,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  ),
  &test_filter_0
);


// Create a text window, into which we will write running filter stats.
GfxUITextArea _filter_txt_0(
  GfxUILayout(
    data_examiner.elementPosX(), (data_examiner.elementPosY() + data_examiner.elementHeight()),
    data_examiner.elementWidth(), 120,
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xC09030,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  )
);




GfxUITextButton _button_0(
  GfxUILayout(
    0, 0, 30, 30,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x9932CC,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "ST"
);

GfxUIButton _button_1(
  GfxUILayout(
    (_button_0.elementPosX() + _button_0.elementWidth() + 1), _button_0.elementPosY(),
    30, 30,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x9932CC,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  )
);

GfxUITextButton _button_2(
  GfxUILayout(
    (_button_1.elementPosX() + _button_1.elementWidth() + 1), _button_1.elementPosY(),
    30, 30,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xFF8C00,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Rm",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);

GfxUIButton _button_3(
  GfxUILayout(
    (_button_2.elementPosX() + _button_2.elementWidth() + 1), _button_2.elementPosY(),
    30, 30,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xFF8C00,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);


GfxUISlider _slider_0(
  GfxUILayout(
    _button_0.elementPosX(), (_button_0.elementPosY() + _button_0.elementHeight() + 1),
    ((_button_3.elementPosX() + _button_3.elementWidth()) - _button_0.elementPosX()), 20,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x20B2AA,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUISlider _slider_1(
  GfxUILayout(
    _slider_0.elementPosX(), (_slider_0.elementPosY() + _slider_0.elementHeight() + 1),
    ((_button_3.elementPosX() + _button_3.elementWidth()) - _button_0.elementPosX()), 20,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xFFA07A,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUISlider _slider_2(
  GfxUILayout(
    _slider_1.elementPosX(), (_slider_1.elementPosY() + _slider_1.elementHeight() + 1),
    ((_button_3.elementPosX() + _button_3.elementWidth()) - _button_0.elementPosX()), 20,
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xFFA07A,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUISlider _slider_3(
  GfxUILayout(
    _button_3.elementPosX() + _button_3.elementWidth(), (_button_3.elementPosY() + 1),
    24, ((_slider_2.elementPosY() + _slider_2.elementHeight()) - _button_0.elementPosY()),
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x90F5EE,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE | GFXUI_SLIDER_FLAG_VERTICAL)
);

GfxUISlider _slider_4(
  GfxUILayout(
    (_slider_3.elementPosX() + _slider_3.elementWidth() + 1), (_slider_3.elementPosY() + 1),
    24, ((_slider_2.elementPosY() + _slider_2.elementHeight()) - _button_0.elementPosY()),
    0, ELEMENT_MARGIN, 0, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xDC143C,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE | GFXUI_SLIDER_FLAG_VERTICAL)
);

// Create a text window, into which we will write running filter stats.
GfxUITextArea _program_info_txt(
  GfxUILayout(
    (_slider_4.elementPosX() + _slider_4.elementWidth() + 1), (_slider_4.elementPosY() + 1),
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


// Create a simple console window, with a full frame.
GfxUITextArea _txt_area_0(
  GfxUILayout(
    0, 0,
    _main_nav.internalWidth(), (_main_nav.internalHeight() - 80),
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 2, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x00FF00,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  ),
  (GFXUI_FLAG_DRAW_FRAME_D | GFXUI_TXTAREA_FLAG_SCROLLABLE)
);

// Create a simple console entry line, with a full frame.
GfxUITextArea _txt_area_1(
  GfxUILayout(
    0, _txt_area_0.xCornerLowerLeft(),
    _txt_area_0.elementWidth(), 80,
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xC0C0C0,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  ),
  (0)
);



GfxUICryptoBurrito crypto_pane(
  GfxUILayout(
    0, 0,                    // Position(x, y)
    _main_nav.internalWidth(), _main_nav.internalHeight()-20,
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,  // Margins_px(t, b, l, r)
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(
    0,          // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x708010,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  )
);


GfxUIC3PScheduler _scheduler_gui(
  GfxUILayout(
    0, 0,                    // Position(x, y)
    _main_nav.internalWidth(), _main_nav.internalHeight()-20,
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,  // Margins_px(t, b, l, r)
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(
    0,          // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x70E080,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    2           // t_size
  )
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
  31000, -1, true,
  []() {
    const uint32_t RAND_NUM = (randomUInt32() & 0x000FFFFF);
    for (uint32_t i = 0; i < RAND_NUM; i++) {
    }
    return 0;
  }
);



int callback_gui_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = -1;
  // NOTE: The GUI is running in a separate thread. It should only be
  //   manipulated indirectly by an IPC mechanism, or by a suitable stand-ins.
  char* cmd = args->position_trimmed(0);
  if (0 == StringBuilder::strcasecmp(cmd, "gravepact")) {
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
  else if (0 == StringBuilder::strcasecmp(cmd, "resize")) {
    // NOTE: This is against GUI best-practices (according to Xorg). But it
    //   might be useful later.
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "border-pix")) {
    //XSetWindowBorder(dpy, win, 40);
  }
  return ret;
}



void ui_value_change_callback(GfxUIElement* element) {
  if (element == ((GfxUIElement*) &_slider_1)) {
    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Slider-1 %.2f", (double) _slider_1.value());
  }
  else if (element == ((GfxUIElement*) &_slider_2)) {
    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Slider-2 %.2f", (double) _slider_2.value());
  }
  else if (element == ((GfxUIElement*) &_slider_3)) {
    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Slider-3 %.2f", (double) _slider_3.value());
  }
  else {
    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "VALUE_CHANGE %p", element);
  }
}


void MainGuiWindow::setConsole(ParsingConsole* con) {
  con->setOutputTarget(&_txt_area_0);
  con->hasColor(false);
  con->localEcho(false);
  _txt_area_0.scrollbackLength(16384);
}



int8_t MainGuiWindow::createWindow() {
  int8_t ret = _init_window();
  if (0 == ret) {
    map_button_inputs(mouse_buttons, sizeof(mouse_buttons) / sizeof(mouse_buttons[0]));
    _overlay.reallocate();
    test_filter_0.init();
    test_filter_1.init();
    test_filter_stdev.init();

    _main_nav_settings.add_child(&_button_0);
    _main_nav_settings.add_child(&_button_1);
    _main_nav_settings.add_child(&_button_2);
    _main_nav_settings.add_child(&_button_3);
    _main_nav_settings.add_child(&_slider_0);
    _main_nav_settings.add_child(&_slider_1);
    _main_nav_settings.add_child(&_slider_2);
    _main_nav_settings.add_child(&_slider_3);
    _main_nav_settings.add_child(&_slider_4);
    _main_nav_settings.add_child(&_program_info_txt);

    _main_nav_data_viewer.add_child(&data_examiner);
    _main_nav_data_viewer.add_child(&_filter_txt_0);

    _main_nav_crypto.add_child(&crypto_pane);

    _main_nav_console.add_child(&_txt_area_0);
    _main_nav_console.add_child(&_txt_area_1);

    _main_nav_internals.add_child(&_scheduler_gui);


    // Adding the contant panes will cause the proper screen co-ords to be imparted
    //   to the group objects. We can then use them for element flow.
    _main_nav.addTab("Timeseries", &_main_nav_data_viewer, true);
    _main_nav.addTab("CryptoBench", &_main_nav_crypto);
    _main_nav.addTab("Links", &_main_nav_links);
    _main_nav.addTab("Console", &_main_nav_console);
    _main_nav.addTab("Internals", &_main_nav_internals);
    _main_nav.addTab("Settings", &_main_nav_settings);

    root.add_child(&_main_nav);

    _filter_txt_0.enableFrames(GFXUI_FLAG_DRAW_FRAME_U);

    _slider_0.value(0.5);
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
    _program_info_txt.provideBuffer(&pitxt);
  }
  return ret;
}



// Called from the thread.
int8_t MainGuiWindow::poll() {
  int8_t ret = 0;

  if (!mlink_onscreen && (nullptr != m_link)) {
    GfxUIMLink* mlink_ui_obj = new GfxUIMLink(
      GfxUILayout(
        0, 0,
        TEST_FILTER_DEPTH, 500,
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
      (GFXUI_FLAG_DRAW_FRAME_MASK)
    );
    mlink_ui_obj->shouldReap(true);
    _main_nav_links.add_child(mlink_ui_obj);
    mlink_onscreen = true;
  }

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
            //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "CTRL release");
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
            StringBuilder _tmp_sbldr;
            _tmp_sbldr.concat('\n');
            console.provideBuffer(&_tmp_sbldr);
          }
          else if ((keysym == XK_Control_L) | (keysym == XK_Control_R)) {
            _modifiers.set(RHOM_GUI_MOD_CTRL_HELD);
            //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "CTRL press");
            _request_clipboard();
          }
          else if ((keysym == XK_Alt_L) | (keysym == XK_Alt_R)) {
            _modifiers.set(RHOM_GUI_MOD_ALT_HELD);
            _request_selection_buffer();
          }
          else if (1 == ret_local) {
            StringBuilder _tmp_sbldr;
            _tmp_sbldr.concat(buf[0]);
            console.provideBuffer(&_tmp_sbldr);
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

  if (!_keep_polling) {
    closeWindow();
    ret = -1;
  }
  else {
    // Render the UI elements...
    // TODO: Should be in the relvant class.
    if (test_filter_0.dirty()) {
      StringBuilder _tmp_sbldr;
      _tmp_sbldr.concatf("RMS:      %.2f\n", (double) test_filter_0.rms());
      _tmp_sbldr.concatf("STDEV:    %.2f\n", (double) test_filter_0.stdev());
      _tmp_sbldr.concatf("SNR:      %.2f\n", (double) test_filter_0.snr());
      _tmp_sbldr.concatf("Min/Max:  %.2f / %.2f\n", (double) test_filter_0.minValue(), (double) test_filter_0.maxValue());
      _filter_txt_0.clear();
      _filter_txt_0.provideBuffer(&_tmp_sbldr);
    }
    if (1 == _redraw_window()) {
      if (1 == test_filter_0.feedFilter(_redraw_timer.lastTime())) {
        test_filter_stdev.feedFilter(test_filter_0.stdev());
      }
    }
  }

  return ret;
}
