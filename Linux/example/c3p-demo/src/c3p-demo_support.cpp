/*
* File:   c3p-demo_support.cpp
* Author: J. Ian Lindsay
* Date:   2024.04.03
*
*/

#include "c3p-demo.h"
#include "C3POnX11.h"


/*******************************************************************************
* Styles, mappings, and anything else that might be clobbered by config file.
*******************************************************************************/

/*
* TODO: All of base_style (GfxUIStyle) should be in main_conf.
* TODO: Color-blind, light, dark profiles for base_style.
*/
GfxUIStyle base_style;


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


/*******************************************************************************
* The program has a set of configurations that it defines and loads at runtime.
* This defines everything required to handle that conf fluidly and safely.
*******************************************************************************/
// Then, we bind those enum values each to a type code, and to a semantic string
//   suitable for storage or transmission to a counterparty.
static const EnumDef<RHoMConfKey> CONF_KEY_LIST[] = {
  { RHoMConfKey::SHOW_PANE_MLINK,       "SHOW_PANE_MLINK",        0, (uint8_t) TCode::BOOLEAN    },
  { RHoMConfKey::SHOW_PANE_BURRITO,     "SHOW_PANE_BURRITO",      0, (uint8_t) TCode::BOOLEAN    },
  { RHoMConfKey::SHOW_PANE_INTERNALS,   "SHOW_PANE_INTERNALS",    0, (uint8_t) TCode::BOOLEAN    },
  { RHoMConfKey::MLINK_XPORT_PATH,      "MLINK_XPORT_PATH",       0, (uint8_t) TCode::STR        },
  { RHoMConfKey::MLINK_TIMEOUT_PERIOD,  "MLINK_TIMEOUT_PERIOD",   0, (uint8_t) TCode::UINT32     },
  { RHoMConfKey::MLINK_KA_PERIOD,       "MLINK_KA_PERIOD",        0, (uint8_t) TCode::UINT32     },
  { RHoMConfKey::MLINK_MTU,             "MLINK_MTU",              0, (uint8_t) TCode::UINT16     },
  { RHoMConfKey::INVALID,               "INVALID",                (ENUM_FLAG_MASK_INVALID_CATCHALL), 0}
};

// The top-level enum wrapper binds the above definitions into a tidy wad
//   of contained concerns.
static const EnumDefList<RHoMConfKey> CONF_LIST(
  CONF_KEY_LIST, (sizeof(CONF_KEY_LIST) / sizeof(CONF_KEY_LIST[0])),
  "RHoMConfKey"  // Doesn't _need_ to be the enum name...
);



StaticHub::StaticHub() :
  ident_program("BIN_ID", (char*) "29c6e2b9-9e68-4e52-9af0-03e9ca10e217"),
  main_config(0, &CONF_LIST)
{}


MouseButtonDef* StaticHub::mouseButtonDefs(uint8_t* count) {
  *count = (uint8_t) sizeof(mouse_buttons) / sizeof(mouse_buttons[0]);
  return mouse_buttons;
}



GfxUIStyle* StaticHub::baseStyle() {  return &base_style;  }


/**
* StaticHub's poll() function is a control point for both complexity and
*   concurrency.
*/
PollResult StaticHub::poll() {
  PollResult ret = PollResult::REPOLL;
  return ret;
}


/*******************************************************************************
* Window management
*******************************************************************************/

void StaticHub::spawnWin_BlobStudy() {
  if (nullptr == hub.blobstudy_window) {
    hub.blobstudy_window = new BlobStudyWindow(0, 0, 1280, 1024, "BlobStudy");
    if (nullptr != hub.blobstudy_window) {
      if (0 == hub.blobstudy_window->createWindow()) {
        // The window thread is running.
        c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Spawned BlobStudy.");
      }
      else {
        c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to create BlobStudy window.");
      }
    }
  }
}



void StaticHub::releaseAllWindows(bool force) {
  if (force & (nullptr != hub.blobstudy_window)) {  hub.blobstudy_window->closeWindow();  }

  if ((nullptr != hub.blobstudy_window) && !hub.blobstudy_window->keepPolling()) {
    BlobStudyWindow* tmp = hub.blobstudy_window;
    hub.blobstudy_window = nullptr;
    delete tmp;
  }
}
