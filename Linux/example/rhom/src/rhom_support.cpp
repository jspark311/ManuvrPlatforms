#include <Linux.h>
#include "RHoM.h"

/*******************************************************************************
* GfxUIUART
*******************************************************************************/

/**
* Constructor
*/
GfxUIUART::GfxUIUART(LinuxUART* u, const GfxUILayout lay, const GfxUIStyle sty, uint32_t f) :
  GfxUIElement(lay, sty, (f | GFXUI_FLAG_ALWAYS_REDRAW)),
  _uart(u) {};



int GfxUIUART::_render(UIGfxWrapper* ui_gfx) {
  int ret = 0;

  if (_uart) {
    uint32_t i_x = internalPosX();
    uint32_t i_y = internalPosY();
    uint16_t i_w = internalWidth();
    //uint16_t i_h = internalHeight();

    UARTOpts* opts = _uart->uartOpts();
    //opts->bitrate
    //opts->start_bits
    //opts->bit_per_word
    //opts->stop_bits
    //opts->parity
    //opts->flow_control
    //opts->xoff_char
    //opts->xon_char
    //opts->padding

    ui_gfx->img()->setCursor(i_x, i_y);
    ui_gfx->img()->setTextSize(_style.text_size);
    ui_gfx->img()->setTextColor((_uart->initialized() ? _style.color_active : _style.color_inactive), _style.color_bg);
    ui_gfx->img()->writeString("Enabled     ");

    ret = 1;
  }

  return ret;
}



bool GfxUIUART::_notify(const GfxUIEvent GFX_EVNT, PixUInt x, PixUInt y, PriorityQueue<GfxUIElement*>* change_log) {
  bool ret = false;
  switch (GFX_EVNT) {
    case GfxUIEvent::TOUCH:
      ret = true;
      break;

    default:
      break;
  }
  if (ret) {
    change_log->insert(this, (int) GFX_EVNT);
    _need_redraw(true);
  }
  return ret;
}
