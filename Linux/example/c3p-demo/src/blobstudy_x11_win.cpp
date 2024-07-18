/*
* File:   blobstudy_x11_win.cpp
* Author: J. Ian Lindsay
* Date:   2023.12.16
*
*/

#include "c3p-demo.h"
#include "C3POnX11.h"

    StringBuilder   input_path;
    StringBuilder   bin_field;

/*******************************************************************************
* Definitions and mappings
*******************************************************************************/
/* Flags that are specific to this window. */
#define GUI_SEL_BUF_HAS_PATH    0x80000000  // Inbound selection buffer will be holding a path.


/*******************************************************************************
* GUI objects
*******************************************************************************/

GfxUITextButton button_paste_from_selbuf(
  GfxUILayout(
    0, 20,
    120, 26,
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x10bbcc,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Selection Buf",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);

GfxUITextButton button_paste_from_clpbrd(
  GfxUILayout(
    (button_paste_from_selbuf.elementPosX() + button_paste_from_selbuf.elementWidth()), button_paste_from_selbuf.elementPosY(),
    button_paste_from_selbuf.elementWidth(), button_paste_from_selbuf.elementHeight(),
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x10bbcc,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Clipboard",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);


GfxUITextButton button_paste_from_testfield(
  GfxUILayout(
    (button_paste_from_clpbrd.elementPosX() + button_paste_from_clpbrd.elementWidth()), button_paste_from_clpbrd.elementPosY(),
    button_paste_from_clpbrd.elementWidth(), button_paste_from_clpbrd.elementHeight(),
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x10bbcc,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Test Field",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);

GfxUITextButton button_unload_file(
  GfxUILayout(
    (button_paste_from_testfield.elementPosX() + button_paste_from_testfield.elementWidth() + 20), button_paste_from_testfield.elementPosY(),
    90, button_paste_from_testfield.elementHeight(),
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0xFF1010,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Unload File",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);


GfxUITextButton button_paste_from_file(
  GfxUILayout(
    (button_unload_file.elementPosX() + button_unload_file.elementWidth() + 20), button_unload_file.elementPosY(),
    90, button_unload_file.elementHeight(),
    ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
    0, 0, 0, 0
  ),
  GfxUIStyle(0, // bg
    0xFFFFFF,   // border
    0xFFFFFF,   // header
    0x10FF10,   // active
    0xA0A0A0,   // inactive
    0xFFFFFF,   // selected
    0x202020,   // unselected
    1           // t_size
  ),
  "Paste Path",
  (GFXUI_BUTTON_FLAG_MOMENTARY)
);


/*******************************************************************************
* Callbacks and other non-class functions
*******************************************************************************/

void ui_value_change_cb_blobstudy(GfxUIElement* element) {
  if (nullptr == hub.blobstudy_window) {
    return;   // Bailout
  }
  else if (element == ((GfxUIElement*) &button_paste_from_clpbrd)) {
    if (button_paste_from_clpbrd.pressed()) {
      hub.blobstudy_window->request_clipboard();
    }
  }
  else if (element == ((GfxUIElement*) &button_paste_from_selbuf)) {
    if (button_paste_from_selbuf.pressed()) {
      hub.blobstudy_window->request_selection_buffer();
    }
  }
  else if (element == ((GfxUIElement*) &button_paste_from_file)) {
    if (button_paste_from_file.pressed()) {
      hub.blobstudy_window->loadPathInSelection();
    }
  }
  else if (element == ((GfxUIElement*) &button_unload_file)) {
    if (button_unload_file.pressed()) {
      hub.blobstudy_window->unloadInputFile();
    }
  }

  else if (element == ((GfxUIElement*) &button_paste_from_testfield)) {
    if (button_paste_from_testfield.pressed()) {
      hub.blobstudy_window->setTestField();
    }
  }

  else {
    c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "VALUE_CHANGE %p", element);
  }
}


void BlobStudyWindow::setTestField() {
  // Byte test field.
  const uint32_t TEST_FIELD_SIZE = 16384;
  uint8_t test_field[TEST_FIELD_SIZE];
  for (uint32_t i = 0; i < TEST_FIELD_SIZE; i++) {
    if (0x00000100 & i) {
      test_field[i] = (255 - (uint8_t) i);
    }
    else {
      test_field[i] = (uint8_t) i;
    }
  }
  _c3pval_input_path.set("Test field");
  bin_field.clear();
  bin_field.concat(test_field, TEST_FIELD_SIZE);
}


void BlobStudyWindow::loadPathInSelection() {
  _modifiers.set(GUI_SEL_BUF_HAS_PATH);
  request_selection_buffer();
}

void BlobStudyWindow::unloadInputFile() {
  if (nullptr != file_in_ptr) {
    delete file_in_ptr;
    file_in_ptr = nullptr;
    unloadMapFile();
  }
  bin_field.clear();
  _c3pval_input_path.set("Nothing loaded");
}


void BlobStudyWindow::unloadMapFile() {
  if (nullptr != file_map_ptr) {
    delete file_map_ptr;
    file_map_ptr = nullptr;
  }
}



/*******************************************************************************
* BlobStudyWindow implementation
*******************************************************************************/

int8_t BlobStudyWindow::createWindow() {
  int8_t ret = _init_window();
  if (0 == ret) {
    uint8_t mb_count = 0;
    MouseButtonDef* mouse_buttons = hub.mouseButtonDefs(&mb_count);
    map_button_inputs(mouse_buttons, mb_count);
    _overlay.reallocate();

    GfxUIC3PValue* input_path_render = new GfxUIC3PValue(
      &_c3pval_input_path,
      GfxUILayout(
        button_paste_from_selbuf.elementPosX(), (button_paste_from_selbuf.elementPosY() + button_paste_from_selbuf.elementHeight()),
        750, 28,
        ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
        0, 0, 0, 0   // Border_px(t, b, l, r)
      ),
      GfxUIStyle(0, // bg
        0xc0c0c0,   // border
        0xFFFFFF,   // header
        0xc0c0c0,   // active
        0xA0A0A0,   // inactive
        0xFFFFFF,   // selected
        0x202020,   // unselected
        2           // t_size
      ),
      (GFXUI_FLAG_FREE_THIS_ELEMENT)
    );


    GfxUIBlobRender* value_render_bin = new GfxUIBlobRender(
      &_c3pval_bin,
      GfxUILayout(
        input_path_render->elementPosX(), (input_path_render->elementPosY() + input_path_render->elementHeight() + 1),
        1080, 900,
        ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN, ELEMENT_MARGIN,
        1, 1, 1, 1   // Border_px(t, b, l, r)
      ),
      GfxUIStyle(0, // bg
        0xc0c0c0,   // border
        0xFFFFFF,   // header
        0xc0c0c0,   // active
        0xA0A0A0,   // inactive
        0xFFFFFF,   // selected
        0x202020,   // unselected
        1           // t_size
      ),
      (GFXUI_FLAG_FREE_THIS_ELEMENT)
    );

    // Assemble the overview pane...
    root.add_child(input_path_render);
    root.add_child(&button_paste_from_file);
    root.add_child(&button_unload_file);
    root.add_child(&button_paste_from_clpbrd);
    root.add_child(&button_paste_from_selbuf);
    root.add_child(&button_paste_from_testfield);
    root.add_child(value_render_bin);

    _refresh_period.reset();

    _c3pval_bin.set(bin_field.string(), bin_field.length(), TCode::BINARY);
    setCallback(ui_value_change_cb_blobstudy);
  }
  return ret;
}





int8_t BlobStudyWindow::closeWindow() {
  return _deinit_window();   // TODO: Ensure that this is both neccessary and safe.
}



int8_t BlobStudyWindow::render_overlay() {
  // If the pointer is within the window, we note its location and
  //   annotate the overlay.
  ui_magnifier.pointerLocation(_pointer_x, _pointer_y);
  if (_modifiers.value(GUI_MOD_CTRL_HELD)) {
    ui_magnifier.render(&gfx_overlay);
  }
  return 0;
}


/*
* Called to unconditionally show the elements in the GUI.
*/
int8_t BlobStudyWindow::render(bool force) {
  int8_t ret = 0;
  uint8_t* render_ptr = nullptr;
  uint32_t render_len = 0;
  if (0 != _c3pval_bin.get_as(&render_ptr, &render_len)) {
    c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Failed to get BLOB from _c3pval_bin.");
  }
  else {
    const bool BLOB_CHANGED = (bin_field.string() != render_ptr);
    if (BLOB_CHANGED) {
      c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "BLOB_CHANGED");
      _c3pval_bin.set(bin_field.string(), bin_field.length(), TCode::BINARY);
    }
    else if (force) {
    }

    if (force | BLOB_CHANGED) {
      ret = 1;
    }
  }
  return ret;
}



// Called from the thread.
int8_t BlobStudyWindow::poll() {
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
              if (_modifiers.value(GUI_SEL_BUF_HAS_PATH)) {
                _modifiers.clear(GUI_SEL_BUF_HAS_PATH);
                // If it hasn't been done already, create a working C3PFile with
                //   the given path name.
                if (nullptr == file_in_ptr) {
                  file_in_ptr = new C3PFile((char*) data);
                  if (nullptr != file_in_ptr) {
                    bool should_free_file = true;
                    if (file_in_ptr->isFile() & file_in_ptr->exists()) {
                      StringBuilder file_data;
                      int32_t file_read_ret = file_in_ptr->read(&file_data);
                      if (0 < file_read_ret) {
                        should_free_file = false;
                        _c3pval_input_path.set(file_in_ptr->path());
                        bin_field.clear();
                        bin_field.concatHandoff(&file_data);
                      }
                      else c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Path %s appears to be an empty file.", (char*) data);
                    }
                    else c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Path %s doesn't exist or isn't a file.", (char*) data);

                    if (should_free_file) {
                      delete file_in_ptr;
                      file_in_ptr = nullptr;
                    }
                  }
                  else c3p_log(LOG_LEV_ALERT, __PRETTY_FUNCTION__, "Failed to instance a C3PFile.");
                }
                else c3p_log(LOG_LEV_WARN, __PRETTY_FUNCTION__, "Can't set a new file until the one already open has been closed.");
              }
              else {
                // Deep-copy the clipboard data into the blob buffer.
                bin_field.clear();
                bin_field.concat(data, size);
                c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Set blob from clipboard (%d bytes).", size);
              }
              XFree(data);
            }
            else c3p_log(LOG_LEV_NOTICE, __PRETTY_FUNCTION__, "target: %d", (int) target);

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
          if (0 == local_ret) {
          }
          else {
            c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Window resize failed (%d).", local_ret);
          }
        }
        break;

      case ButtonPress:
      case ButtonRelease:
        {
          uint16_t btn_id = e.xbutton.button;
          if (_modifiers.value(GUI_MOD_CTRL_HELD)) {
            const GfxUIEvent event = (btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP;
            switch (btn_id) {
              case 4:
              case 5:
                // Scroll events adjust the magnifier scale if CTRL is held.
                ui_magnifier.notify(
                  ((btn_id == 5) ? GfxUIEvent::MOVE_DOWN : GfxUIEvent::MOVE_UP),
                  ui_magnifier.elementPosX(), ui_magnifier.elementPosY(), nullptr
                );
              default:
                break;
            }
          }
          else {
            int8_t ret = _proc_mouse_button(btn_id, e.xbutton.x, e.xbutton.y, (e.type == ButtonPress));
            if (0 == ret) {
              // Any unclaimed input can be handled in this block.
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
            //c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "CTRL release");
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
          if (keysym == XK_Escape) {
            _keep_polling = false;
          }
          else if (keysym == XK_Return) {
            // StringBuilder _tmp_sbldr;
            // _tmp_sbldr.concat('\n');
            // console.pushBuffer(&_tmp_sbldr);
          }
          else if ((keysym == XK_Control_L) | (keysym == XK_Control_R)) {
            _modifiers.set(GUI_MOD_CTRL_HELD);
          }
          else if ((keysym == XK_Alt_L) | (keysym == XK_Alt_R)) {
            _modifiers.set(GUI_MOD_ALT_HELD);
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
    //_c3p_value_3.set(test_filter_0.snr());
    if (1 == _redraw_window()) {
      // TODO: We aren't flexing the TimeSeries render here. But we should still
      //   capture some render and processing timing stats for internals.
      // if (1 == test_filter_0.feedFilter(_redraw_timer.lastTime())) {
      //   test_filter_stdev.feedFilter(test_filter_0.stdev());
      // }
    }
  }

  return ret;
}
