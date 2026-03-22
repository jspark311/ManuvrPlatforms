/*
* File:   X11Window.cpp
* Author: J. Ian Lindsay
*
*/

#include "WorldCrafter.h"
#include "C3POnX11.h"

#define CONSOLE_INPUT_HEIGHT  200
#define ELEMENT_MARGIN          5

#define NOISE_X_LOCATION    0
#define NOISE_Y_LOCATION    0
#define NOISE_WIDTH      1000
#define NOISE_HEIGHT     1050

extern bool continue_running;         // TODO: (rolled up newspaper) Bad...


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


GfxUINamedSlider _slider_scale(
  GfxUILayout(
    (NOISE_X_LOCATION + NOISE_WIDTH), NOISE_Y_LOCATION,
    150, 30,
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    //0x20B2AA,   // active
    0xFFA07A,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUINamedSlider _slider_octaves(
  GfxUILayout(
    _slider_scale.elementPosX(), (_slider_scale.elementPosY() + _slider_scale.elementHeight()),
    _slider_scale.elementWidth(), _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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


GfxUINamedSlider _slider_fade(
  GfxUILayout(
    _slider_scale.elementPosX(), (_slider_octaves.elementPosY() + _slider_octaves.elementHeight()),
    _slider_scale.elementWidth(), _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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

GfxUINamedSlider _slider_freq(
  GfxUILayout(
    _slider_scale.elementPosX(), _slider_fade.elementPosY() + _slider_fade.elementHeight(),
    _slider_scale.elementWidth(), _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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

GfxUITextButton _button_reapply_0(
  GfxUILayout(
    (_slider_scale.elementPosX() + _slider_scale.elementWidth()), _slider_scale.elementPosY(),
    60, _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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
  "Reapply",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);

GfxUITextButton _button_reshuffle_0(
  GfxUILayout(
    (_slider_octaves.elementPosX() + _slider_octaves.elementWidth()), _slider_octaves.elementPosY(),
    60, _slider_octaves.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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
  "Reshuffle",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);



GfxUINamedSlider _slider_scale_1(
  GfxUILayout(
    _slider_scale.elementPosX(), (_slider_freq.elementPosY() + _slider_freq.elementHeight()),
    _slider_scale.elementWidth(), _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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

GfxUINamedSlider _slider_octaves_1(
  GfxUILayout(
    _slider_scale_1.elementPosX(), (_slider_scale_1.elementPosY() + _slider_scale_1.elementHeight()),
    _slider_scale_1.elementWidth(), _slider_scale_1.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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


GfxUINamedSlider _slider_fade_1(
  GfxUILayout(
    _slider_scale_1.elementPosX(), (_slider_octaves_1.elementPosY() + _slider_octaves_1.elementHeight()),
    _slider_scale_1.elementWidth(), _slider_scale_1.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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

GfxUINamedSlider _slider_freq_1(
  GfxUILayout(
    _slider_scale_1.elementPosX(), _slider_fade_1.elementPosY() + _slider_fade_1.elementHeight(),
    _slider_scale_1.elementWidth(), _slider_scale_1.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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

GfxUITextButton _button_reapply_1(
  GfxUILayout(
    (_slider_scale_1.elementPosX() + _slider_scale_1.elementWidth()), _slider_scale_1.elementPosY(),
    60, _slider_scale_1.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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
  "Reapply",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);

GfxUITextButton _button_reshuffle_1(
  GfxUILayout(
    (_slider_octaves_1.elementPosX() + _slider_octaves_1.elementWidth()), _slider_octaves_1.elementPosY(),
    60, _slider_octaves_1.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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
  "Reshuffle",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);


GfxUINamedSlider _slider_scale_2(
  GfxUILayout(
    _slider_scale_1.elementPosX(), (_slider_freq_1.elementPosY() + _slider_freq_1.elementHeight()),
    _slider_scale.elementWidth(), _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x30AA37,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUINamedSlider _slider_octaves_2(
  GfxUILayout(
    _slider_scale_2.elementPosX(), (_slider_scale_2.elementPosY() + _slider_scale_2.elementHeight()),
    _slider_scale_2.elementWidth(), _slider_scale_2.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x30AA37,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);


GfxUINamedSlider _slider_fade_2(
  GfxUILayout(
    _slider_scale_2.elementPosX(), (_slider_octaves_2.elementPosY() + _slider_octaves_2.elementHeight()),
    _slider_scale_2.elementWidth(), _slider_scale_2.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x30AA37,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUINamedSlider _slider_freq_2(
  GfxUILayout(
    _slider_scale_2.elementPosX(), _slider_fade_2.elementPosY() + _slider_fade_2.elementHeight(),
    _slider_scale_2.elementWidth(), _slider_scale_2.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x30AA37,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  (GFXUI_SLIDER_FLAG_RENDER_VALUE)
);

GfxUITextButton _button_reapply_2(
  GfxUILayout(
    (_slider_scale_2.elementPosX() + _slider_scale_2.elementWidth()), _slider_scale_2.elementPosY(),
    60, _slider_scale_2.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x30AA37,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Reapply",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);

GfxUITextButton _button_reshuffle_2(
  GfxUILayout(
    (_slider_octaves_2.elementPosX() + _slider_octaves_2.elementWidth()), _slider_octaves_2.elementPosY(),
    60, _slider_octaves_2.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
    0, 0, 0, 0               // Border_px(t, b, l, r)
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x30AA37,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Reshuffle",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);





GfxUISlider _slider_ntsc_noise(
  GfxUILayout(
    _slider_scale.elementPosX(), (_slider_freq_2.elementPosY() + _slider_freq_2.elementHeight() + 5),
    _slider_scale.elementWidth(), _slider_scale.elementHeight(),
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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
    60, 30,
    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
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
  "Free-run"
);



  GfxUITextButton _button_facets(
    GfxUILayout(
      _button_freerun.elementPosX(), (_button_freerun.elementPosY() + _button_freerun.elementHeight()),
      90, _button_freerun.elementHeight(),
      0, 5, 5, 0,   // Margins_px(t, b, l, r)
      0, 0, 0, 0    // Border_px(t, b, l, r)
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
    "Facets",
    (GFXUI_BUTTON_FLAG_STATE)  // Button is pressed by default.
  );
  GfxUITextButton _button_wireframe(
    GfxUILayout(
      (_button_facets.elementPosX() + _button_facets.elementWidth()), _button_facets.elementPosY(),
      90, _button_freerun.elementHeight(),
      0, 5, 5, 0,   // Margins_px(t, b, l, r)
      0, 0, 0, 0    // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      0x9962CC,   // active
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    "Wireframe",
    (GFXUI_BUTTON_FLAG_STATE)  // Button is pressed by default.
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


const uint32_t ICO_FREQ = 24;
const float    ICO_AREA = 0.01f;

PixAddr field_drag_initial(0, 0);


IcosphereGfxUI<uint32_t>* icosphere_render = nullptr;
C3PIcosphere<uint32_t> icomesh(ICO_FREQ, ICO_AREA);
C3PIcosphere<float>    icomesh_noise_0(ICO_FREQ, ICO_AREA);
C3PIcosphere<float>    icomesh_noise_1(ICO_FREQ, ICO_AREA);
C3PIcosphere<float>    icomesh_noise_2(ICO_FREQ, ICO_AREA);

IcosphereNoise iconoise_0(&icomesh_noise_0, 0.15, 3, 0.13);
IcosphereNoise iconoise_1(&icomesh_noise_1, 0.8,  2, 0.23);
IcosphereNoise iconoise_2(&icomesh_noise_2, 0.5,  1, 0.56);

float rotation_counter = 0.0f;
float noise_counter = 0.0f;

bool reapply_noise_0 = false;
bool reapply_noise_1 = false;
bool reapply_noise_2 = false;


// TODO: Unfastidiousness elsewhere causes me to write this fxn to avoid repeating myself.
void rerender_perlin_noise() {
  const bool ANY_NOISE_REAPPLIED = (reapply_noise_0 | reapply_noise_1 | reapply_noise_2);
  if (reapply_noise_0 && icomesh_noise_0.initialized()) {
    iconoise_0.apply();
    reapply_noise_0 = false;
  }
  if (reapply_noise_1 && icomesh_noise_1.initialized()) {
    //iconoise_1.reshuffle();
    iconoise_1.apply();
    reapply_noise_1 = false;
  }
  if (reapply_noise_2 && icomesh_noise_2.initialized()) {
    iconoise_2.apply();
    reapply_noise_2 = false;
  }

  if (ANY_NOISE_REAPPLIED) {
    for (uint32_t i = 0; i < icomesh.FACE_COUNT; i++) {
      // if () {
        icomesh.facetById(i)->content = 0xFF000000 | \
          (uint32_t)(0xFF * icomesh_noise_0.facetById(i)->content) | ((uint32_t)(0xFF * icomesh_noise_1.facetById(i)->content) << 8) | ((uint32_t)(0xFF * icomesh_noise_2.facetById(i)->content) << 16);
      // }
    }
  }
}



C3PScheduledLambda schedule_ts_update(
  "ts_update",
  50000, -1, true,
  []() {
    if (nullptr != icosphere_render) {
      if (_button_freerun.pressed()) {
        noise_counter += 0.1;
        float offset_x = 0.0;
        float offset_y = 0.0;
        float offset_z = 0.0;
        iconoise_2.getOffset(&offset_x, &offset_y, &offset_z);
        offset_x = sinf(noise_counter);
        offset_y = sinf(offset_x + offset_y);;
        iconoise_2.setOffset(offset_x, offset_y, offset_z);
        reapply_noise_1 = true;
      }
    }
    return 0;
  }
);



void ui_value_change_callback(GfxUIElement* element) {
  const bool PARAM_0_CHANGE = (
    (&_slider_scale == element) | (&_slider_octaves == element) |
    (&_slider_fade == element) | (&_slider_freq == element) |
    ((&_button_reapply_0 == element) & (_button_reapply_0.pressed()))
  );

  const bool PARAM_1_CHANGE = (
    (&_slider_scale_1 == element) | (&_slider_octaves_1 == element) |
    (&_slider_fade_1 == element) | (&_slider_freq_1 == element) |
    ((&_button_reapply_1 == element) & (_button_reapply_1.pressed()))
  );

  const bool PARAM_2_CHANGE = (
    (&_slider_scale_2 == element) | (&_slider_octaves_2 == element) |
    (&_slider_fade_2 == element) | (&_slider_freq_2 == element) |
    ((&_button_reapply_2 == element) & (_button_reapply_2.pressed()))
  );

  if (PARAM_0_CHANGE) {
    iconoise_0.setParameters(
      ((_slider_scale.value() * 149) + 1.0f),
      ((_slider_octaves.value() * 15) + 1),
      _slider_fade.value(),
      ((_slider_freq.value() * 10) + 1.0f)
    );
    reapply_noise_0 = true;
  }
  else if (&_button_reshuffle_0 == element) {
    if (_button_reshuffle_0.pressed()) {
      iconoise_0.reshuffle();
      reapply_noise_0 = true;
    }
  }

  if (PARAM_1_CHANGE) {
    iconoise_1.setParameters(
      ((_slider_scale_1.value() * 149) + 1.0f),
      ((_slider_octaves_1.value() * 15) + 1),
      _slider_fade_1.value(),
      ((_slider_freq_1.value() * 10) + 1.0f)
    );
    reapply_noise_1 = true;
  }
  else if (&_button_reshuffle_1 == element) {
    if (_button_reshuffle_1.pressed()) {
      iconoise_1.reshuffle();
      reapply_noise_1 = true;
    }
  }

  if (PARAM_2_CHANGE) {
    iconoise_2.setParameters(
      ((_slider_scale_2.value() * 149) + 1.0f),
      ((_slider_octaves_2.value() * 15) + 1),
      _slider_fade_2.value(),
      ((_slider_freq_2.value() * 10) + 1.0f)
    );
    reapply_noise_2 = true;
  }
  else if (&_button_reshuffle_2 == element) {
    if (_button_reshuffle_0.pressed()) {
      iconoise_2.reshuffle();
      reapply_noise_2 = true;
    }
  }

  //else if (&_slider_ntsc_noise == element) {
  //}
  else if (&_button_freerun == element) {
    if (!_button_freerun.pressed()) {
      noise_counter = 0.0;
    }
  }
}


int8_t MainGuiWindow::createWindow() {
  int8_t ret = _init_window();
  if (0 == ret) {
    map_button_inputs(mouse_buttons, sizeof(mouse_buttons) / sizeof(mouse_buttons[0]));
    _overlay.reallocate();

    _slider_scale.setText(  "Scale  ");
    _slider_octaves.setText("Octaves");
    _slider_fade.setText(   "Fade   ");
    _slider_freq.setText(   "Freq   ");
    _slider_scale.value(0.5);
    _slider_octaves.value(0.15);
    _slider_fade.value(0.3);
    _slider_freq.value(0.1);

    _slider_scale_1.setText(  "Scale  ");
    _slider_octaves_1.setText("Octaves");
    _slider_fade_1.setText(   "Fade   ");
    _slider_freq_1.setText(   "Freq   ");
    _slider_scale_1.value(0.1);
    _slider_octaves_1.value(0.13);
    _slider_fade_1.value(0.87);
    _slider_freq_1.value(0.24);

    _slider_scale_2.setText(  "Scale  ");
    _slider_octaves_2.setText("Octaves");
    _slider_fade_2.setText(   "Fade   ");
    _slider_freq_2.setText(   "Freq   ");
    _slider_scale_2.value(0.1);
    _slider_octaves_2.value(0.13);
    _slider_fade_2.value(0.87);
    _slider_freq_2.value(0.24);

    root.add_child(&_slider_scale);
    root.add_child(&_slider_octaves);
    root.add_child(&_slider_fade);
    root.add_child(&_slider_freq);
    root.add_child(&_button_reapply_0);
    root.add_child(&_button_reshuffle_0);

    root.add_child(&_slider_scale_1);
    root.add_child(&_slider_octaves_1);
    root.add_child(&_slider_fade_1);
    root.add_child(&_slider_freq_1);
    root.add_child(&_button_reapply_1);
    root.add_child(&_button_reshuffle_1);

    root.add_child(&_slider_scale_2);
    root.add_child(&_slider_octaves_2);
    root.add_child(&_slider_fade_2);
    root.add_child(&_slider_freq_2);
    root.add_child(&_button_reapply_2);
    root.add_child(&_button_reshuffle_2);

    root.add_child(&_button_freerun);
    root.add_child(&_button_facets);
    root.add_child(&_button_wireframe);


    if (0 == icomesh_noise_0.init()) {
      if (0 == iconoise_0.init(0)) {   // No specified seed value.
        //reapply_noise_0 = true;
      }
    }
    if (0 == icomesh_noise_1.init()) {
      if (0 == iconoise_1.init(0)) {   // No specified seed value.
        //NoiseControlGfxUI* noise_1_gui = new NoiseControlGfxUI(
        //  &iconoise_1,
        //  GfxUILayout(
        //    _slider_ntsc_noise.elementPosX(), (_button_freerun.elementPosY() + _button_freerun.elementHeight() + 5),
        //    200, 150,
        //    0, ELEMENT_MARGIN, ELEMENT_MARGIN, 0,
        //    0, 0, 0, 0               // Border_px(t, b, l, r)
        //  ),
        //  GfxUIStyle(0, // bg
        //    0xFFFFFF,   // border
        //    0xFFFFFF,   // header
        //    0x9932CC,   // active
        //    0xA0A0A0,   // inactive
        //    0xFFFFFF,   // selected
        //    0x202020,   // unselected
        //    1           // t_size
        //  ),
        //  (0)
        //);
        //root.add_child(noise_1_gui);
        //reapply_noise_1 = true;
      }
    }
    if (0 == icomesh_noise_2.init()) {
      if (0 == iconoise_2.init(0)) {   // No specified seed value.
        //reapply_noise_2 = true;
      }
    }
    if (0 == icomesh.init()) {
      icosphere_render = new IcosphereGfxUI<uint32_t>(
        &icomesh,
        GfxUILayout(
          NOISE_X_LOCATION, NOISE_Y_LOCATION,
          NOISE_WIDTH, NOISE_HEIGHT,
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
        (0)
      );
      root.add_child(icosphere_render);
    }

    _refresh_period.reset();
    setCallback(ui_value_change_callback);
    C3PScheduler::getInstance()->addSchedule(&schedule_ts_update);
  }
  return ret;
}



int8_t MainGuiWindow::closeWindow() {
  continue_running = false;
  // if (nullptr != noise_gen) {
  //   ImgPerlinNoise* tmp = noise_gen;
  //   noise_gen = nullptr;
  //   delete tmp;
  // }
  return _deinit_window();
}



int8_t MainGuiWindow::render_overlay() {
  // If the pointer is within the window, we note its location and
  //   annotate the overlay.
  ui_magnifier.pointerLocation(_pointer_x, _pointer_y);
  ui_magnifier.render(&gfx_overlay);
  // if (nullptr != icosphere_render) {
  //   icosphere_render->renderWireframe(_button_wireframe.pressed());
  //   icosphere_render->renderFacet(_button_facets.pressed());
  // }
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
    StringBuilder pitxt;
    pitxt.concat("Perlin noise demo\nBuild date " __DATE__ " " __TIME__);
    struct utsname sname;
    if (1 != uname(&sname)) {
      pitxt.concatf("%s %s (%s)", sname.sysname, sname.release, sname.machine);
      pitxt.concatf("\n%s", sname.version);
    }
    pitxt.concatf("Window: %dx%d", _fb.x(), _fb.y());
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
                    //noise_gen->setOffset(mouse_x, mouse_y);
                    //reapply_noise = true;
                  }
                }
                else {
                  field_drag_initial = PixAddr(0, 0);
                }
                break;


              case 4:
              case 5:
                // Unhandled scroll events adjust the magnifier scale.
                if (_modifiers.value(GUI_MOD_CTRL_HELD)) {
                  ui_magnifier._notify(
                    ((btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP),
                    ui_magnifier.elementPosX(), ui_magnifier.elementPosY(), nullptr
                  );
                }
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
            _modifiers.clear(GUI_MOD_CTRL_HELD);
          }
          else if ((keysym == XK_Alt_L) | (keysym == XK_Alt_R)) {
            _modifiers.clear(GUI_MOD_ALT_HELD);
          }
        }
        break;


      case KeyPress:
        {
          char buf[128] = {0, };
          KeySym keysym;
          int ret_local = XLookupString(&e.xkey, buf, sizeof(buf), &keysym, nullptr);
          switch (keysym) {
            case XK_Escape:
              _keep_polling = false;
              break;
            case XK_Return:
              break;
            case XK_Alt_R:
            case XK_Alt_L:
              _modifiers.set(GUI_MOD_ALT_HELD);
              break;
            case XK_Control_R:
            case XK_Control_L:
              _modifiers.set(GUI_MOD_CTRL_HELD);
              break;

            case XK_s:   // CTRL+S will screencap with datetime.
            case XK_S:
              if (_modifiers.value(GUI_MOD_CTRL_HELD)) {
                StringBuilder outfile_sb("screencap-");
                currentDateTime(&outfile_sb);
                outfile_sb.concat(".png");
                ImageWriter w(gfx.img(), (char*) outfile_sb.string());
                if (w.writePNG()) {
                  ret = 0;
                }
                else {
                  printf("Failed to write PNG: %s\n", outfile_sb.string());
                }
              }
              break;

            case XK_c:   // CTRL+C will copy the working Image to the clipboard.
            case XK_C:
              if (_modifiers.value(GUI_MOD_CTRL_HELD)) {
              }
              break;

            case XK_v:   // CTRL+V will paste from the clipboard.
            case XK_V:
              if (_modifiers.value(GUI_MOD_CTRL_HELD)) {
              }
              break;

            default:
              //if (1 == ret_local) {
              //}
              //else {
                c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Key press: %s (%s)", buf, XKeysymToString(keysym));
              //}
              break;
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
    //rerender_perlin_noise();
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
