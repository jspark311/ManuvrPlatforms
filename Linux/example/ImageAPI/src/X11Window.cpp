/*
* File:   X11Window.cpp
* Author: J. Ian Lindsay
*
*/

#include "ImageAPI.h"
#include "C3POnX11.h"

#define CONSOLE_INPUT_HEIGHT  200
#define ELEMENT_MARGIN          3

#define NOISE_X_LOCATION   0
#define NOISE_Y_LOCATION  80
#define NOISE_WIDTH      900
#define NOISE_HEIGHT     700


extern bool continue_running;         // TODO: (rolled up newspaper) Bad...
extern ParsingConsole console;

ImgPerlinNoise* noise_gen = nullptr;

bool reapply_noise = false;

PixAddr field_drag_initial(0, 0);


/*******************************************************************************
* UI definition
*******************************************************************************/

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


GfxUISlider _slider_scale(
  GfxUILayout(
    (NOISE_X_LOCATION + NOISE_WIDTH + ELEMENT_MARGIN), NOISE_Y_LOCATION,
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUISlider _slider_octaves(
  GfxUILayout(
    _slider_scale.elementPosX(), (_slider_scale.elementPosY() + _slider_scale.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);


GfxUISlider _slider_fade(
  GfxUILayout(
    _slider_octaves.elementPosX(), (_slider_octaves.elementPosY() + _slider_octaves.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUISlider _slider_freq(
  GfxUILayout(
    _slider_fade.elementPosX(), (_slider_fade.elementPosY() + _slider_fade.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);



GfxUISlider slider_x(
  GfxUILayout(
    _slider_freq.elementPosX(), (_slider_freq.elementPosY() + _slider_freq.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);


GfxUISlider slider_y(
  GfxUILayout(
    slider_x.elementPosX(), (slider_x.elementPosY() + slider_x.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);


GfxUISlider slider_z(
  GfxUILayout(
    slider_y.elementPosX(), (slider_y.elementPosY() + slider_y.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);



GfxUISlider _slider_ntsc_noise(
  GfxUILayout(
    slider_z.elementPosX(), (slider_z.elementPosY() + slider_z.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUITextButton _button_freerun(
  GfxUILayout(
    _slider_ntsc_noise.elementPosX(), (_slider_ntsc_noise.elementPosY() + _slider_ntsc_noise.elementHeight() + 5),
    150, 30,
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
    2           // t_size
  ),
  "Free-running"
);


GfxUITextButton _button_ntsc(
  GfxUILayout(
    (_button_freerun.elementPosX() + _button_freerun.elementWidth()) + 5, _button_freerun.elementPosY(),
    60, 30,
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
  "NTSC",
  (0)
);

GfxUITextButton _button_crt(
  GfxUILayout(
    (_button_ntsc.elementPosX() + _button_ntsc.elementWidth()) + 5, _button_ntsc.elementPosY(),
    60, 30,
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
  "CRT",
  (0)
);


GfxUITextButton _button_v_anchor_lines(
  GfxUILayout(
    _button_freerun.elementPosX(), (_button_freerun.elementPosY() + _button_freerun.elementHeight() + 5),
    60, 30,
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
  "Anchors",
  (0) //(GFXUI_BUTTON_FLAG_MOMENTARY)
);


GfxUITextButton _button_v_value(
  GfxUILayout(
    (_button_v_anchor_lines.elementPosX() + _button_v_anchor_lines.elementWidth()) + 5, _button_v_anchor_lines.elementPosY(),
    60, 30,
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
  "Value",
  (0) //(GFXUI_BUTTON_FLAG_MOMENTARY)
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

GfxNTSCEffect* ntsc_filter = nullptr;
GlobeRender*   globe_render = nullptr;
Vector3Render* vector_render = nullptr;
GfxCRTBloomEffect* crt_effect = nullptr;

// TODO: Unfastidiousness elsewhere causes me to write this fxn to avoid repeating myself.
void rerender_perlin_noise() {
  if (_button_freerun.pressed()) {
    if (nullptr != noise_gen) {
      noise_gen->reshuffle();
      noise_gen->apply();
    }
  }
  if (_button_ntsc.pressed()) {
    ntsc_filter->apply();
  }
}

float rotation_counter = 0.0f;



C3PScheduledLambda schedule_ts_update(
  "ts_update",
  50000, -1, true,
  []() {
    rotation_counter += 0.1f;
    globe_render->setOrientation(
      slider_x.value() + sinf(rotation_counter),
      slider_y.value()
    );
    globe_render->renderWithMarker(
      37.624f,
      -72.644f
    );

    vector_render->setVector(
      _slider_scale.value(),
      _slider_fade.value(),
      _slider_freq.value()
    );
    vector_render->setOrientation(
      slider_x.value(),
      slider_z.value()
    );
    vector_render->drawAnchorLines(_button_v_anchor_lines.pressed());
    vector_render->drawValue(_button_v_value.pressed());
    vector_render->render();

    rerender_perlin_noise();
    return 0;
  }
);



void ui_value_change_callback(GfxUIElement* element) {

  if (&_slider_ntsc_noise == element) {
    ntsc_filter->noiseFactor(_slider_ntsc_noise.value());
  }
  // else if (&_button_v_anchor_lines == element) {
  //   globe_render->renderWithMarker(
  //     _slider_freq.value(),
  //     _slider_fade.value(),
  //     37.624f,
  //     -72.644f
  //   );
  // }
  else {
    noise_gen->setParameters(
      ((_slider_scale.value() * 149) + 1.0f),
      ((_slider_octaves.value() * 15) + 1),
      _slider_fade.value(),
      ((_slider_freq.value() * 10) + 1.0f)
    );
    reapply_noise = true;
  }
}


int8_t MainGuiWindow::createWindow() {
  int8_t ret = _init_window();
  if (0 == ret) {
    map_button_inputs(mouse_buttons, sizeof(mouse_buttons) / sizeof(mouse_buttons[0]));
    _overlay.reallocate();

    root.add_child(&_program_info_txt);
    root.add_child(&_slider_scale);
    root.add_child(&_slider_octaves);
    root.add_child(&_slider_fade);
    root.add_child(&_slider_freq);
    root.add_child(&_slider_ntsc_noise);
    root.add_child(&_button_freerun);
    root.add_child(&_button_ntsc);
    root.add_child(&_button_crt);

    root.add_child(&_button_v_anchor_lines);
    root.add_child(&_button_v_value);
    root.add_child(&slider_x);
    root.add_child(&slider_y);
    root.add_child(&slider_z);

    _slider_scale.value(0.5);
    _slider_octaves.value(0.15);
    _slider_fade.value(0.3);
    _slider_freq.value(0.1);

    slider_x.value(0.3);
    slider_y.value(0.15);
    slider_z.value(0.8);


    // Adding the content panes will cause the proper screen co-ords to be imparted
    //   to the group objects. We can then use them for element flow.
    noise_gen = new ImgPerlinNoise(&_fb,
      NOISE_X_LOCATION, NOISE_Y_LOCATION,
      NOISE_WIDTH, NOISE_HEIGHT,
      ((_slider_scale.value() * 149) + 1.0f),
      ((_slider_octaves.value() * 15) + 1),
      (_slider_fade.value())
    );

    // Same target and source make this class act as a mutating filter on the
    //   entire framebuffer.
    // ImageSubframe in_frame(
    //   &_fb,
    //   PixAddr(NOISE_X_LOCATION, NOISE_Y_LOCATION),
    //   NOISE_WIDTH, NOISE_HEIGHT
    // );
    // ImageSubframe out_frame(
    //   &_fb,
    //   PixAddr(NOISE_X_LOCATION, NOISE_Y_LOCATION),
    //   NOISE_WIDTH, NOISE_HEIGHT
    // );
    //ntsc_filter = new GfxNTSCEffect(in_frame, out_frame);
    ntsc_filter = new GfxNTSCEffect(&_fb, &_fb);
    // ntsc_filter->setSourceFrame(
    //   PixAddr(NOISE_X_LOCATION, NOISE_Y_LOCATION),
    //   NOISE_WIDTH, NOISE_HEIGHT
    // );
    ntsc_filter->setSourceFrame(
      PixAddr(
        _button_v_anchor_lines.elementPosX(),
        (_button_v_anchor_lines.elementPosY() + _button_v_anchor_lines.elementHeight() + 5)
      ),
      300, 300
    );

    ntsc_filter->noiseFactor(0.05f);

    globe_render = new GlobeRender(&_fb);
    globe_render->setSourceFrame(
      PixAddr(
        _button_v_anchor_lines.elementPosX(),
        (_button_v_anchor_lines.elementPosY() + _button_v_anchor_lines.elementHeight() + 5)
      ),
      300, 300
    );

    vector_render = new Vector3Render(&_fb);
    vector_render->setSourceFrame(
      PixAddr(
        _button_v_anchor_lines.elementPosX(),
        (_button_v_anchor_lines.elementPosY() + _button_v_anchor_lines.elementHeight() + 310)
      ),
      300, 300
    );
    vector_render->setOrientation(
      slider_x.value() * 180.0f,
      slider_y.value() * 360.0f
    );


    crt_effect = new GfxCRTBloomEffect(&_fb, &_overlay);
    crt_effect->setSourceFrame(
      PixAddr(
        _button_v_anchor_lines.elementPosX(),
        (_button_v_anchor_lines.elementPosY() + _button_v_anchor_lines.elementHeight() + 5)
      ),
      300, 300
    );


    if (nullptr != noise_gen) {
      if (0 == noise_gen->init(0)) {   // No specified seed value.
        reapply_noise = true;
      }
    }

    _refresh_period.reset();
    setCallback(ui_value_change_callback);
    C3PScheduler::getInstance()->addSchedule(&schedule_ts_update);
  }
  return ret;
}



int8_t MainGuiWindow::closeWindow() {
  continue_running = false;
  if (nullptr != noise_gen) {
    ImgPerlinNoise* tmp = noise_gen;
    noise_gen = nullptr;
    delete tmp;
  }
  return _deinit_window();
}



int8_t MainGuiWindow::render_overlay() {
  // If the pointer is within the window, we note its location and
  //   annotate the overlay.
  ui_magnifier.pointerLocation(_pointer_x, _pointer_y);
  ui_magnifier.render(&gfx_overlay);

  if (_button_crt.pressed()) {
    crt_effect->bloomFactor(slider_y.value());
    crt_effect->edgeCurvature(slider_z.value());
    crt_effect->apply();
  }
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
    _program_info_txt.reposition(CONSOLE_INPUT_X_POS, CONSOLE_INPUT_Y_POS);
    _program_info_txt.resize(width(), CONSOLE_INPUT_HEIGHT);
    StringBuilder pitxt;
    pitxt.concat("Perlin noise demo\nBuild date " __DATE__ " " __TIME__);
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
              case 2:
                // Mouse drag for offset.
                if (ButtonPress == e.type) {
                  int mouse_x = 0;
                  int mouse_y = 0;
                  field_drag_initial = PixAddr(mouse_x, mouse_y);
                  queryPointer(&mouse_x, &mouse_y);
                  //gfx_element.includesPoint(mouse_x, mouse_y)
                  // Is the mouse click in-bounds? Perhaps the worst line of code I've ever written.
                  if ((mouse_x >= NOISE_X_LOCATION) && (mouse_x < (NOISE_X_LOCATION + NOISE_WIDTH)) && (mouse_y >= NOISE_Y_LOCATION) && (mouse_y < (NOISE_Y_LOCATION + NOISE_HEIGHT))) {
                    noise_gen->setOffset(mouse_x, mouse_y);
                    reapply_noise = true;
                  }
                }
                else {
                  field_drag_initial = PixAddr(0, 0);
                }
                break;


              case 4:
              case 5:
                // Unhandled scroll events adjust the magnifier scale.
                //ui_magnifier.notify(
                //  ((btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP),
                //  ui_magnifier.elementPosX(), ui_magnifier.elementPosY()
                //);
                break;

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
    if (reapply_noise) {
      reapply_noise = false;
      rerender_perlin_noise();
    }
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
