/*
* File:   c3p-demo.h
* Author: J. Ian Lindsay
* Date:   2024.04.01
*
*/

#ifndef __C3PDEMO_HEADER_H__
#define __C3PDEMO_HEADER_H__

/* System-level includes */
#include <cstdio>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <fstream>
#include <iostream>

/* The parts of CppPotpourri we want. */
#include "CppPotpourri.h"
#include "AbstractPlatform.h"
#include "StringBuilder.h"
#include "Console/C3PConsole.h"
#include "ElementPool.h"
#include "Pipes/BufferAccepter/GPSWrapper/GPSWrapper.h"
#include "BusQueue/UARTAdapter.h"
#include "BusQueue/I2CAdapter.h"
#include "C3PValue/KeyValuePair.h"
#include "TimeSeries/TimeSeries.h"
#include "Vector3.h"
#include "TimerTools/TimerTools.h"
#include "Storage/RecordTypes/ConfRecord.h"
#include "cbor-cpp/cbor.h"
#include "Image/Image.h"
#include "Image/ImageUtils.h"
#include "Image/GfxUI.h"
#include "Identity/Identity.h"
#include "Identity/IdentityUUID.h"
#include "M2MLink/M2MLink.h"
#include "CryptoBurrito/CryptoBurrito.h"
#include "C3POnX11.h"
#include "Linux.h"
#include "LinuxStorage.h"


#define PROGRAM_VERSION    "0.0.1"    // Program version.

/*
* Flags used by all windows. Holding CTRL will invoke the zoom tool.
*/
#define GUI_MOD_CTRL_HELD           0x00000001   //
#define GUI_MOD_ALT_HELD            0x00000002   //

/* GUI parameters */
#define ELEMENT_MARGIN          3
#define CONSOLE_INPUT_HEIGHT  200
#define TEST_FILTER_DEPTH     3000

#define U_INPUT_BUFF_SIZE      512    // The maximum size of user input.


/*******************************************************************************
* Configuration keys
*******************************************************************************/
enum class RHoMConfKey : uint16_t {
  SHOW_PANE_MLINK,
  SHOW_PANE_BURRITO,
  SHOW_PANE_INTERNALS,
  MLINK_XPORT_PATH,
  MLINK_TIMEOUT_PERIOD,
  MLINK_KA_PERIOD,
  MLINK_MTU,
  INVALID
};


/*******************************************************************************
* Logging shim for buffer pipelines...
*******************************************************************************/
class CryptoLogShunt : public CryptOpCallback {
  public:
    CryptoLogShunt() {};
    ~CryptoLogShunt() {};

    /* Mandatory overrides from the CryptOpCallback interface... */
    int8_t op_callahead(CryptOp* op) {
      return JOB_Q_CALLBACK_NOMINAL;
    };

    int8_t op_callback(CryptOp* op) {
      StringBuilder output;
      op->printOp(&output);
      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, &output);
      return JOB_Q_CALLBACK_NOMINAL;
    };
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


/*******************************************************************************
* These are the GUI windows that might be spawned by the program.
*
* C3Px11Window is fundamentally an I/O class, and (consistent with C3P's
*   standards) has a poll() function to facilitate the carving of compute on
*   the chronological plane. On linux, that poll() function will be called by
*   a thread devoted to each instantiated C3Px11Window.
*******************************************************************************/

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


/*
* BlobStudy opens in a new window. It is mostly useful for looking at BIN files.
*/
class BlobStudyWindow : public C3Px11Window {
  public:
    GfxUIMagnifier ui_magnifier;

    BlobStudyWindow(PixUInt x, PixUInt y, PixUInt w, PixUInt h, const char* TITLE) :
      C3Px11Window(x, y, w, h, TITLE),
      ui_magnifier(&_fb, 0, 0,
        300, 300, 0xFFFFFF,
        (GFXUI_FLAG_DRAW_FRAME_U | GFXUI_FLAG_DRAW_FRAME_L | GFXUI_MAGNIFIER_FLAG_SHOW_FEED_FRAME)
      ),
      blob_view(0, 0, 0, 0),
      _modifiers(0),
      _key_target(nullptr),
      _paste_target(nullptr),
      file_in_ptr(nullptr),
      _c3pval_input_path("Nothing loaded"),
      _c3pval_bin(nullptr, 0) {};

    /* Obligatory overrides from C3Px11Window. */
    int8_t poll();
    int8_t createWindow();
    int8_t closeWindow();
    int8_t render(bool force);
    int8_t render_overlay();

    void loadPathInSelection();
    void unloadInputFile();
    void unloadMapFile();
    void setTestField();


  private:
    GfxUIGroup      blob_view;
    FlagContainer32 _modifiers;
    GfxUIElement*   _key_target;
    GfxUIElement*   _paste_target;
    C3PFile*        file_in_ptr  = nullptr;
    C3PFile*        file_map_ptr = nullptr;
    C3PValue        _c3pval_input_path;
    C3PValue        _c3pval_bin;
};




/* A GUI object to represent a Linux UART. */
// TODO: Promote, rather than copy-pasting per-project.
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



/*******************************************************************************
* StaticHub is a singleton class in the program that facilitates message passing
*   between windows (that is: between threads). It is created by main(), and
*   lasts the lifetime of the program, as reckoned by the OS that is running it.
* NOTE: IPC concurrency controls should be implemented here, if needed.
*******************************************************************************/
class StaticHub {
  public:
    /* Hard refs to windows and certain GUI elements. */
    IdentityUUID        ident_program;   // The identity of the running program.
    ConfRecordValidation<RHoMConfKey> main_config;   // Hard ref to program configuration.
    MainGuiWindow*      main_window      = nullptr;  // Hard ref to a singleton window.
    BlobStudyWindow*    blobstudy_window = nullptr;  // Hard ref to a singleton window.
    LinuxUART*          uart             = nullptr;  // TODO: This is bad design. But that is what StaticHub is for.
    bool continue_running  = false;     // The program will start its shutdown when this is false.

    StaticHub();
    ~StaticHub() {};

    MouseButtonDef* mouseButtonDefs(uint8_t* count);
    GfxUIStyle* baseStyle();

    /* Functions for window management. */
    void spawnWin_BlobStudy();
    void releaseAllWindows(bool force = false);

    PollResult poll();
};


/*******************************************************************************
* Globals that are extern'd
*******************************************************************************/
extern StaticHub hub;

#endif  // __C3PDEMO_HEADER_H__
