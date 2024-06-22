/*
* File:   RHoM.h
* Author: J. Ian Lindsay
*
*/

#include "CppPotpourri.h"
#include "AbstractPlatform.h"
#include "StringBuilder.h"
#include "Console/C3PConsole.h"
#include "ElementPool.h"
#include "Pipes/BufferAccepter/GPSWrapper/GPSWrapper.h"
#include "BusQueue/UARTAdapter.h"
#include "BusQueue/I2CAdapter.h"
#include "C3PValue/KeyValuePair.h"
#include "SensorFilter.h"
#include "Vector3.h"
#include "TimerTools/TimerTools.h"
#include "Storage/RecordTypes/ConfRecord.h"
#include "uuid.h"
#include "cbor-cpp/cbor.h"
#include "Image/Image.h"
#include "Image/ImageUtils.h"
#include "Image/GfxUI.h"
#include "Identity/Identity.h"
#include "Identity/IdentityUUID.h"
#include "M2MLink/M2MLink.h"
#include "CryptoBurrito/CryptoBurrito.h"
#include "C3POnX11.h"
#include <Linux.h>

#ifndef __RHOM_HEADER_H__
#define __RHOM_HEADER_H__


#define PROGRAM_VERSION    "0.0.4"    // Program version.

#define RHOM_GUI_MOD_CTRL_HELD           0x00000001   //
#define RHOM_GUI_MOD_ALT_HELD            0x00000002   //

/* This program loads configuration data from a file. These are the keys. */
enum class RHoMConfKey : uint16_t {
  SHOW_PANE_MLINK,
  MLINK_XPORT_PATH,
  MLINK_TIMEOUT_PERIOD,
  MLINK_KA_PERIOD,
  MLINK_MTU,
  MLINK_MAX_QUEUED_MSGS,
  MLINK_MAX_PARSE_FAILS,
  MLINK_MAX_ACK_FAILS,
  MLINK_DEFAULT_ENCODING,
  MLINK_REQUIRE_AUTH,
  MLINK_FARSIDE_LOGGING,
  UART_BITRATE,
  UART_START_BITS,
  UART_BIT_PER_WORD,
  UART_STOP_BITS,
  UART_PARITY,
  UART_FLOW_CONTROL,
  INVALID
};



// TODO: Don't code against this too much. It will probably be templated and promoted to C3P.
class LinkSockPair {
  public:
    LinuxSockPipe* sock;
    M2MLink* link;
    uint32_t established;

    LinkSockPair(LinuxSockPipe* s, M2MLink* l) :
      sock(s),
      link(l),
      established(millis())
    {};

    ~LinkSockPair() {
      delete sock;
      delete link;
    };
};


class MainGuiWindow : public C3Px11Window {
  public:
    // Firmware UIs are small. If the host is showing the UI on a 4K monitor, it
    //   will cause "the squints".
    // This element should not be added to the element list, and it ignores the
    //   position arguments.
    GfxUIMagnifier ui_magnifier;

    MainGuiWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char* TITLE) :
      C3Px11Window(x, y, w, h, TITLE),
      ui_magnifier(&_fb, 0, 0,
        200, 200, 0xFFFFFF,
        (GFXUI_FLAG_DRAW_FRAME_U | GFXUI_FLAG_DRAW_FRAME_L | GFXUI_MAGNIFIER_FLAG_SHOW_FEED_FRAME)
      ),
      _modifiers(0),
      _key_target(nullptr),
      _paste_target(nullptr) {};

    void setConsole(ParsingConsole*);

    /* Obligatory overrides from C3Px11Window. */
    int8_t poll();
    int8_t createWindow();
    int8_t closeWindow();
    int8_t render(bool force);
    int8_t render_overlay();


  private:
    FlagContainer32 _modifiers;
    GfxUIElement*   _key_target;
    GfxUIElement*   _paste_target;
};





class GfxUIUART : public GfxUIElement {
  public:
    GfxUIUART(LinuxUART*, const GfxUILayout, const GfxUIStyle, uint32_t f = 0);
    ~GfxUIUART() {};

    /* Implementation of GfxUIElement. */
    virtual int  _render(UIGfxWrapper* ui_gfx);
    virtual bool _notify(const GfxUIEvent GFX_EVNT, PixUInt x, PixUInt y, PriorityQueue<GfxUIElement*>* change_log);


  private:
    LinuxUART*  _uart;
};



extern IdentityUUID ident_uuid;
extern ConfRecordValidation<RHoMConfKey> rhom_conf;

#endif  // __RHOM_HEADER_H__
