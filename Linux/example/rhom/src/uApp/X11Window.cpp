/*
* File:   X11Window.cpp
* Author: J. Ian Lindsay
*
*/

#include "RHoM.h"
#include "C3POnX11.h"

#define CONSOLE_INPUT_HEIGHT  200
#define TEST_FILTER_DEPTH     310
#define ELEMENT_MARGIN          5

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

GfxUILayout test0(
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0
);
GfxUILayout test1(
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0
);
GfxUIGroup test2(
  GfxUILayout{
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
  }, 0
);


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
GfxUISensorFilter<uint32_t> sf_render_0(
  &test_filter_0,
  0, 50,
  TEST_FILTER_DEPTH, 170,
  0xC09020, (GFXUI_SENFILT_FLAG_SHOW_RANGE | GFXUI_SENFILT_FLAG_SHOW_VALUE)
);
// Graph the standard deviation of the screen re-draw period.
GfxUISensorFilter<float> sf_render_1(
  &test_filter_stdev,
  sf_render_0.elementPosX(),
  sf_render_0.elementPosY() + sf_render_0.elementHeight() + 1,
  TEST_FILTER_DEPTH, 60,
  0xC0B020, (GFXUI_SENFILT_FLAG_SHOW_RANGE | GFXUI_SENFILT_FLAG_SHOW_VALUE)
);
// Create a text window, into which we will write running filter stats.
GfxUITextArea _filter_txt_0(
  sf_render_1.elementPosX(),
  sf_render_1.elementPosY() + sf_render_1.elementHeight() + 2,
  sf_render_1.elementWidth(),
  40, 0xC09020
);

GfxUITextButton _button_0(
  "ST",
  sf_render_0.elementPosX() + sf_render_0.elementWidth() + ELEMENT_MARGIN,
  sf_render_0.elementPosY(),
  22, 22, 0x9932CC
);
GfxUIButton _button_1(
  _button_0.elementPosX() + _button_0.elementWidth() + ELEMENT_MARGIN,
  _button_0.elementPosY(),
  22, 22, 0x9932CC,
  GFXUI_BUTTON_FLAG_MOMENTARY
);

GfxUITextButton _button_2(
  "Rm",
  _button_1.elementPosX() + _button_1.elementWidth() + ELEMENT_MARGIN,
  _button_1.elementPosY(),
  22, 22, 0xFF8C00
);

GfxUIButton _button_3(
  _button_2.elementPosX() + _button_2.elementWidth() + ELEMENT_MARGIN,
  _button_2.elementPosY(),
  22, 22, 0xFF8C00,
  GFXUI_BUTTON_FLAG_MOMENTARY
);

GfxUISlider _slider_0(
  _button_0.elementPosX(),
  _button_0.elementPosY() + _button_0.elementHeight() + ELEMENT_MARGIN,
  (22*4) + (ELEMENT_MARGIN * 3),  20,
  0x20B2AA, GFXUI_SLIDER_FLAG_RENDER_VALUE
);

GfxUISlider _slider_1(
  _slider_0.elementPosX(),
  _slider_0.elementPosY() + _slider_0.elementHeight() + ELEMENT_MARGIN,
  (22*4) + (ELEMENT_MARGIN * 3),  20,
  0xFFA07A, GFXUI_SLIDER_FLAG_RENDER_VALUE
);

GfxUISlider _slider_2(
  _slider_1.elementPosX(),
  _slider_1.elementPosY() + _slider_1.elementHeight() + ELEMENT_MARGIN,
  (22*4) + (ELEMENT_MARGIN * 3),  20,
  0xFFA07A, GFXUI_SLIDER_FLAG_RENDER_VALUE
);

GfxUISlider _slider_3(
  _button_3.elementPosX() + _button_3.elementWidth() + ELEMENT_MARGIN,
  _button_3.elementPosY(),
  24,  100, 0x90F5EE, GFXUI_SLIDER_FLAG_RENDER_VALUE | GFXUI_SLIDER_FLAG_VERTICAL
);

GfxUISlider _slider_4(
  _slider_3.elementPosX() + _slider_3.elementWidth() + ELEMENT_MARGIN,
  _slider_3.elementPosY(),
  24,  100, 0xDC143C, GFXUI_SLIDER_FLAG_RENDER_VALUE | GFXUI_SLIDER_FLAG_VERTICAL
);

// Create a simple console window, with a full frame.
GfxUITextArea _txt_area_0(
  _filter_txt_0.elementPosX(),
  _filter_txt_0.elementPosY() + _filter_txt_0.elementHeight() + 2,
  400, 145, 0x00FF00,
  (GFXUI_FLAG_DRAW_FRAME_U)
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


int8_t MainGuiWindow::createWindow() {
  int8_t ret = _init_window();
  if (0 == ret) {
    map_button_inputs(mouse_buttons, sizeof(mouse_buttons) / sizeof(mouse_buttons[0]));
    _overlay.reallocate();
    test_filter_0.init();
    test_filter_1.init();
    test_filter_stdev.init();

    root.add_child(&_button_0);
    root.add_child(&_button_1);
    root.add_child(&_button_2);
    root.add_child(&_button_3);

    root.add_child(&_slider_0);
    root.add_child(&_slider_1);
    root.add_child(&_slider_2);
    root.add_child(&_slider_3);
    root.add_child(&_slider_4);
    root.add_child(&sf_render_0);
    root.add_child(&sf_render_1);
    root.add_child(&_filter_txt_0);
    root.add_child(&_txt_area_0);

    console.setOutputTarget(&_txt_area_0);
    console.hasColor(false);
    console.localEcho(true);

    _filter_txt_0.enableFrames(GFXUI_FLAG_DRAW_FRAME_U);

    _slider_0.value(0.5);
    _refresh_period.reset();
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
    const uint  CONSOLE_INPUT_X_POS = 0;
    const uint  CONSOLE_INPUT_Y_POS = (height() - CONSOLE_INPUT_HEIGHT) - 1;
    _txt_area_0.reposition(CONSOLE_INPUT_X_POS, CONSOLE_INPUT_Y_POS);
    _txt_area_0.resize(width(), CONSOLE_INPUT_HEIGHT);

    _fb.setCursor(2, 0);
    _fb.setTextSize(2);
    _fb.setTextColor(0xA0A0A0, 0);
    _fb.writeString("Right Hand of Manuvr");
    _fb.setCursor(14, 18);
    _fb.setTextSize(1);
    _fb.writeString("Build date " __DATE__ " " __TIME__);

    StringBuilder txt_render;
    struct utsname sname;
    if (1 != uname(&sname)) {
      txt_render.concatf("%s %s (%s)", sname.sysname, sname.release, sname.machine);
      txt_render.concatf("\n%s", sname.version);
      _fb.writeString(&txt_render);
      txt_render.clear();
    }
    txt_render.concatf("Window: %dx%d", _fb.x(), _fb.y());
    _fb.writeString(&txt_render);
    txt_render.clear();
  }

  return ret;
}



// Called from the thread.
int8_t MainGuiWindow::poll() {
  int8_t ret = 0;

  if (!mlink_onscreen && (nullptr != m_link)) {
    GfxUIMLink* mlink_ui_obj = new GfxUIMLink(
      m_link,
      _slider_2.elementPosX(),
      _slider_2.elementPosY() + _slider_2.elementHeight() + ELEMENT_MARGIN,
      360,
      248,
      (GFXUI_FLAG_DRAW_FRAME_MASK)
    );
    mlink_ui_obj->shouldReap(true);
    root.add_child(mlink_ui_obj);
    mlink_onscreen = true;
  }

  if (0 < XPending(_dpy)) {
    Atom WM_DELETE_WINDOW = XInternAtom(_dpy, "WM_DELETE_WINDOW", False);
    XEvent e;
    XNextEvent(_dpy, &e);

    switch (e.type) {
      case Expose:
        {
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
                ui_magnifier.notify(
                  ((btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP),
                  ui_magnifier.elementPosX(), ui_magnifier.elementPosY()
                );
              default:
                break;
            }
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
        break;

      case MotionNotify:
        _pointer_x = e.xmotion.x;
        _pointer_y = e.xmotion.y;
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
