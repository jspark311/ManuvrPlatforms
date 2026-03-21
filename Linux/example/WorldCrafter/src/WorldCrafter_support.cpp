#include "WorldCrafter.h"
#include "Image/Image.h"
#include "Image/ImageUtils.h"
#include "Image/GfxUI.h"


/*******************************************************************************
* GfxUIElement functions
*******************************************************************************/
#if 0

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
  _add_child(&_slider_scale);
  _add_child(&_slider_octaves);
  _add_child(&_slider_fade);
  _add_child(&_slider_freq);
  _add_child(&_button_reapply);
  _add_child(&_button_reshuffle);
}
#endif
