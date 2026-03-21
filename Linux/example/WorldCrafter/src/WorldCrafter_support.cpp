#include "WorldCrafter.h"

/*******************************************************************************
* GfxUIElement functions
*******************************************************************************/

NoiseControlGfxUI::NoiseControlGfxUI(IcosphereNoise* noise, const GfxUILayout lay, const GfxUIStyle sty, uint32_t f) :
  GfxUIElement(lay, sty, f),

  _slider_scale(
    GfxUILayout(
      internalPosX(), internalPosY(),
      150, 30,
      0, 5, 5, 0,
      0, 0, 0, 0               // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      sty.color_active,
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    (GFXUI_SLIDER_FLAG_RENDER_VALUE)
  ),

  _slider_octaves(
    GfxUILayout(
      _slider_scale.elementPosX(), (_slider_scale.elementPosY() + _slider_scale.elementHeight()),
      _slider_scale.elementWidth(), _slider_scale.elementHeight(),
      0, 5, 5, 0,
      0, 0, 0, 0               // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      sty.color_active,
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    (GFXUI_SLIDER_FLAG_RENDER_VALUE)
  ),

  _slider_fade(
    GfxUILayout(
      _slider_scale.elementPosX(), (_slider_octaves.elementPosY() + _slider_octaves.elementHeight()),
      _slider_scale.elementWidth(), _slider_scale.elementHeight(),
      0, 5, 5, 0,
      0, 0, 0, 0               // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      sty.color_active,
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    (GFXUI_SLIDER_FLAG_RENDER_VALUE)
  ),

  _slider_freq(
    GfxUILayout(
      _slider_scale.elementPosX(), _slider_fade.elementPosY() + _slider_fade.elementHeight(),
      _slider_scale.elementWidth(), _slider_scale.elementHeight(),
      0, 5, 5, 0,
      0, 0, 0, 0               // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      sty.color_active,
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    (GFXUI_SLIDER_FLAG_RENDER_VALUE)
  ),

  _button_reapply(
    GfxUILayout(
      (_slider_scale.elementPosX() + _slider_scale.elementWidth()), _slider_scale.elementPosY(),
      60, _slider_scale.elementHeight(),
      0, 5, 5, 0,
      0, 0, 0, 0               // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      sty.color_active,
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    "Reapply",
    (GFXUI_BUTTON_FLAG_MOMENTARY)
  ),

  _button_reshuffle(
    GfxUILayout(
      (_slider_octaves.elementPosX() + _slider_octaves.elementWidth()), _slider_octaves.elementPosY(),
      60, _slider_octaves.elementHeight(),
      0, 5, 5, 0,
      0, 0, 0, 0               // Border_px(t, b, l, r)
    ),
    GfxUIStyle(0, // bg
      0xFFFFFF,   // border
      0xFFFFFF,   // header
      sty.color_active,
      0xA0A0A0,   // inactive
      0xFFFFFF,   // selected
      0x202020,   // unselected
      1           // t_size
    ),
    "Reshuffle",
    (GFXUI_BUTTON_FLAG_MOMENTARY)
  ),
  _noise_obj(noise)
{
  _slider_scale.setText(  "Scale  ");
  _slider_octaves.setText("Octaves");
  _slider_fade.setText(   "Fade   ");
  _slider_freq.setText(   "Freq   ");
  _slider_scale.value(0.5);
  _slider_octaves.value(0.15);
  _slider_fade.value(0.3);
  _slider_freq.value(0.1);

  _add_child(&_slider_scale);
  _add_child(&_slider_octaves);
  _add_child(&_slider_fade);
  _add_child(&_slider_freq);
  _add_child(&_button_reapply);
  _add_child(&_button_reshuffle);
}



void NoiseControlGfxUI::applyValues() {
  if (nullptr != _noise_obj) {
    _noise_obj->setParameters(
      ((_slider_scale.value() * 149) + 1.0f),
      ((_slider_octaves.value() * 15) + 1),
      _slider_fade.value(),
      ((_slider_freq.value() * 10) + 1.0f)
    );
    _noise_obj->apply();
    _reapply_noise = false;
  }
}


bool NoiseControlGfxUI::valuesChanged() {
  bool ret = false;
  return ret;
}


int NoiseControlGfxUI::_render(UIGfxWrapper* ui_gfx) {
  int ret = 0;
  return ret;
}


bool NoiseControlGfxUI::_notify(const GfxUIEvent GFX_EVNT, PixUInt x, PixUInt y, PriorityQueue<GfxUIElement*>* change_log) {
  bool ret = false;
  switch (GFX_EVNT) {
    case GfxUIEvent::TOUCH:
      if (_button_reapply.underPointer()) {
        _reapply_noise = true;
        ret = true;
      }
      else if (_button_reshuffle.underPointer()) {
        applyValues();
        _noise_obj->reshuffle();
        ret = true;
      }
      break;
    default:
      break;
  }

  if (ret) {
    _need_redraw(true);
  }
  return ret;
}
