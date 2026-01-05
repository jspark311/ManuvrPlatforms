/*
File:   WiFi.cpp
Author: J. Ian Lindsay
Date:   2025.12.27

This file contains a reusable wrapper for the ESP32's radio features.
*/

#include "../ESP32.h"


#define ESP32RADIO_FSM_WAYPOINT_DEPTH  12

/*******************************************************************************
*      _______.___________.    ___   .___________. __    ______     _______.
*     /       |           |   /   \  |           ||  |  /      |   /       |
*    |   (----`---|  |----`  /  ^  \ `---|  |----`|  | |  ,----'  |   (----`
*     \   \       |  |      /  /_\  \    |  |     |  | |  |        \   \
* .----)   |      |  |     /  _____  \   |  |     |  | |  `----.----)   |
* |_______/       |__|    /__/     \__\  |__|     |__|  \______|_______/
*
* Static members and initializers should be located here.
*******************************************************************************/
TaskHandle_t _thread_id_svc = nullptr;   // Deprecated. Retained for link hygiene.

wifi_ap_record_t ESP32Radio::_scan_list_ap[ESP32RADIO_SCAN_RESULTS] = {0};
uint16_t ESP32Radio::_scan_list_count = 0;
uint16_t ESP32Radio::_scan_total_count = 0;


const EnumDef<ESP32RadioState> _STATE_LIST[] = {
  { ESP32RadioState::UNINIT,        "UNINIT"},
  { ESP32RadioState::PRE_INIT,      "PRE_INIT"},
  { ESP32RadioState::RESETTING,     "RESETTING"},
  { ESP32RadioState::INIT,          "INIT"},
  { ESP32RadioState::SCANNING,      "SCANNING"},
  { ESP32RadioState::PROMISCUOUS,   "PROMISCUOUS"},
  { ESP32RadioState::CONNECTING,    "CONNECTING"},
  { ESP32RadioState::CONNECTED,     "CONNECTED"},
  { ESP32RadioState::DISCONNECTING, "DISCONNECTING"},
  { ESP32RadioState::DISCONNECTED,  "DISCONNECTED"},
  { ESP32RadioState::SLEEPING,      "SLEEPING"},
  { ESP32RadioState::WAKING,        "WAKING"},
  { ESP32RadioState::FAULT,         "FAULT"},
  { ESP32RadioState::INVALID,       "INVALID", (ENUM_WRAPPER_FLAG_CATCHALL)}  // FSM hygiene.
};
const EnumDefList<ESP32RadioState> _FSM_STATES(&_STATE_LIST[0], (sizeof(_STATE_LIST) / sizeof(_STATE_LIST[0])), "ESP32RadioState");


// Static storage, no heap. Compiler will know by call-graph if the memory needs
//   to be set aside and linked.
ESP32Radio* ESP32Radio::getInstance() {
  static uint8_t _radio_obj_buf[sizeof(ESP32Radio)] __attribute__((aligned(alignof(ESP32Radio))));
  static ESP32Radio* _inst = nullptr;
  if (nullptr == _inst) {
    _inst = new (_radio_obj_buf) ESP32Radio();
  }
  return _inst;
}


/*******************************************************************************
* Event handlers: signal-only (no driver calls)
*******************************************************************************/
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  static const char* const TAG = "wifi_event_handler";
  ESP32Radio* radio = (ESP32Radio*) arg;
  if (nullptr == radio) return;

  switch (event_id) {
    case WIFI_EVENT_STA_START:
      // Signal only. FSM decides when to call esp_wifi_connect().
      radio->_mb_set_wifi_started(true);
      break;

    case WIFI_EVENT_STA_CONNECTED:
      radio->_mb_set_sta_connected(true);
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      // Signal only. FSM decides when/if to reconnect.
      radio->_mb_set_sta_connected(false);
      // Treat as authoritative routability loss immediately.
      radio->_mb_set_ip4_valid(false);
      radio->_mb_set_ip4_addr(0);
      break;

    case WIFI_EVENT_SCAN_DONE:
      radio->_mb_set_scan_done(true);
      break;

    default:
      //c3p_log(LOG_LEV_DEBUG, TAG, "WIFI event: %d", event_id);
      break;
  }
}


void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  static const char* const TAG = "ip_event_handler";
  ESP32Radio* radio = (ESP32Radio*) arg;
  if (nullptr == radio) return;

  switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      //c3p_log(LOG_LEV_INFO, TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      radio->_mb_set_ip4_addr(event->ip_info.ip.addr);
      radio->_mb_set_ip4_valid(true);
      break;
    }
    case IP_EVENT_STA_LOST_IP:
      //c3p_log(LOG_LEV_INFO, TAG, "lost ip");
      radio->_mb_set_ip4_valid(false);
      radio->_mb_set_ip4_addr(0);
      break;

    default:
      //c3p_log(LOG_LEV_DEBUG, TAG, "IP event: %d", event_id);
      break;
  }
}



/*
* Constructor
*/
ESP32Radio::ESP32Radio() :
  StateMachine<ESP32RadioState>("ESP32Radio-FSM", &_FSM_STATES, ESP32RadioState::UNINIT, ESP32RADIO_FSM_WAYPOINT_DEPTH)
{
  // Event-loop -> poll() mailboxes.
  _mb_wifi_started   = false;
  _mb_sta_connected  = false;
  _mb_ip4_valid      = false;
  _mb_scan_done      = false;
  _mb_ip4_addr       = 0;

  // Poll/FSM-owned latched state.
  _wifi_started      = false;
  _sta_connected     = false;
  _ip4_valid         = false;
  _ip4_addr          = 0;

  _scan_done_latched = false;
}


/*
* Destructor
*/
ESP32Radio::~ESP32Radio() {
  esp_netif_deinit();
  esp_wifi_deinit();
}



int8_t ESP32Radio::init() {
  int8_t ret = 0;
  _fsm_set_route(3, ESP32RadioState::PRE_INIT, ESP32RadioState::INIT, ESP32RadioState::DISCONNECTED);
  return ret;
}



int8_t ESP32Radio::serialize_ap(const uint8_t AP_IDX, StringBuilder* output) {
  int8_t ret = -1;
  return ret;
}



/*******************************************************************************
* Mailbox helpers (safe for event-loop task context)
*******************************************************************************/
void ESP32Radio::_mb_set_wifi_started(const bool v) { _mb_wifi_started  = v; }
void ESP32Radio::_mb_set_sta_connected(const bool v){ _mb_sta_connected = v; }
void ESP32Radio::_mb_set_ip4_valid(const bool v)    { _mb_ip4_valid     = v; }
void ESP32Radio::_mb_set_scan_done(const bool v)    { _mb_scan_done     = v; }
void ESP32Radio::_mb_set_ip4_addr(const uint32_t a) { _mb_ip4_addr      = a; }



/*******************************************************************************
* Public query API (for sibling FSMs like MQTT)
*******************************************************************************/
bool ESP32Radio::linkUp() {  return _sta_connected;  }
bool ESP32Radio::hasIP()  {  return _ip4_valid;      }
uint32_t ESP32Radio::ip4() { return _ip4_addr;       }



void ESP32Radio::printDebug(StringBuilder* output) {
  StringBuilder tmp;
  StringBuilder prod_str("ESP32Radio");
  prod_str.concatf(" [%sinitialized]", (initialized() ? "" : "un"));
  StringBuilder::styleHeader2(&tmp, (const char*) prod_str.string());
  printFSM(&tmp);

  if (initialized()) {
    tmp.concatf("\t STA link:  %c\n", (_sta_connected ? 'y' : 'n'));
    tmp.concatf("\t Has IPv4:  %c\n", (_ip4_valid ? 'y' : 'n'));
    if (_ip4_valid) {
      tmp.concatf(
        "\t IPv4:      %u.%u.%u.%u\n",
        (_ip4_addr & 0xFF),
        ((_ip4_addr >> 8) & 0xFF),
        ((_ip4_addr >> 16) & 0xFF),
        ((_ip4_addr >> 24) & 0xFF)
      );
    }

    if (connected()) {
      tmp.concat("Connected to AP:\n");
      _print_ap_record(&tmp, &_current_ap);
    }
    if (0 < _scan_list_count) {
      tmp.concatf("Most recent scan results (%u/%u):\n", ESP32Radio::_scan_list_count, ESP32Radio::_scan_total_count);
      for (uint16_t i = 0; (i < ESP32Radio::_scan_list_count); i++) {
        tmp.concatf("\t%2u:", i);
        _print_ap_record(&tmp, &_scan_list_ap[i]);
      }
    }
  }
  else if (_pre_init_complete()) {
    tmp.concatf("\t WIFI_INIT:            %c\n", (_flags.value(ESP32RADIO_FLAG_WIFI_INIT) ? 'y' : 'n'));
    tmp.concatf("\t WIFI_STARTED:         %c\n", (_flags.value(ESP32RADIO_FLAG_WIFI_STARTED) ? 'y' : 'n'));
    tmp.concatf("\t INIT_WIFI_AS_STATION: %c\n", (_flags.value(ESP32RADIO_FLAG_INIT_WIFI_AS_STATION) ? 'y' : 'n'));
  }
  else {
    tmp.concatf("\t NETIF_INIT:         %c\n", (_flags.value(ESP32RADIO_FLAG_NETIF_INIT) ? 'y' : 'n'));
    tmp.concatf("\t EVENT_LOOP_CREATED: %c\n", (_flags.value(ESP32RADIO_FLAG_EVENT_LOOP_CREATED) ? 'y' : 'n'));
  }

  tmp.string();
  output->concatHandoff(&tmp);
}



int ESP32Radio::console_handler_esp_radio(StringBuilder* txt_ret, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);

  if (0 == StringBuilder::strcasecmp(cmd, "associate")) {
    if (3 == args->count()) {
      char* ssid = args->position_trimmed(1);
      char* psk = args->position_trimmed(2);
      wifi_config_t wifi_config;
      esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
      memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
      memcpy(wifi_config.sta.password, psk, strlen(psk));
      wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }
    else {
      txt_ret->concatf("Usage: %s <ssid> <psk>.\n", cmd);
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "con")) {
    if (!connected()) {
      ret = _fsm_append_route(2, ESP32RadioState::CONNECTING, ESP32RadioState::CONNECTED);
    }
    else {
      txt_ret->concat("ESP32Radio is already connected.\n");
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "discon")) {
    if (connected()) {
      ret = _fsm_append_route(2, ESP32RadioState::DISCONNECTING, ESP32RadioState::DISCONNECTED);
    }
    else {
      txt_ret->concat("ESP32Radio is already disconnected.\n");
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "deauth")) {
    uint16_t aid = (uint16_t) args->position_as_int(1);
    txt_ret->concatf("esp_wifi_deauth_sta(%u) returns %d.\n", aid, esp_wifi_deauth_sta(aid));
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "scan")) {
    if (initialized()) {
      txt_ret->concatf("wifi_scan() returns %d.\n", wifi_scan());
    }
    else {
      txt_ret->concat("ESP32Radio is not initialized.\n");
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "pack")) {
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "parse")) {
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "fsm")) {
    args->drop_position(0);
    ret = fsm_console_handler(txt_ret, args);  // Shunt into the FSM console handler.
  }
  else {
    printDebug(txt_ret);
  }

  return ret;
}




/*******************************************************************************
* Wifi utilities
*******************************************************************************/

void ESP32Radio::_print_ap_record(StringBuilder* output, wifi_ap_record_t* ap_info) {
  output->concatf("\t[%s]\tRSSI: %d\tCHAN: %d\n", ap_info->ssid, ap_info->rssi, ap_info->primary);
  output->concat("\t\tAuthmode: ");
  switch (ap_info->authmode) {
    case WIFI_AUTH_OPEN:             output->concat("OPEN");             break;
    case WIFI_AUTH_WEP:              output->concat("WEP");              break;
    case WIFI_AUTH_WPA_PSK:          output->concat("WPA_PSK");          break;
    case WIFI_AUTH_WPA2_PSK:         output->concat("WPA2_PSK");         break;
    case WIFI_AUTH_WPA_WPA2_PSK:     output->concat("WPA_WPA2_PSK");     break;
    case WIFI_AUTH_WPA2_ENTERPRISE:  output->concat("WPA2_ENTERPRISE");  break;
    case WIFI_AUTH_WPA3_PSK:         output->concat("WPA3_PSK");         break;
    //case WIFI_AUTH_WPA2_WPA3_PSK:    output->concat("WPA2_WPA3_PSK");    break;
    default:    output->concat("WIFI_AUTH_UNKNOWN");    break;
  }
  if (ap_info->authmode != WIFI_AUTH_WEP) {
    output->concat("\t\tPairwise: ");
    switch (ap_info->pairwise_cipher) {
      case WIFI_CIPHER_TYPE_NONE:       output->concat("NONE");      break;
      case WIFI_CIPHER_TYPE_WEP40:      output->concat("WEP40");     break;
      case WIFI_CIPHER_TYPE_WEP104:     output->concat("WEP104");    break;
      case WIFI_CIPHER_TYPE_TKIP:       output->concat("TKIP");      break;
      case WIFI_CIPHER_TYPE_CCMP:       output->concat("CCMP");      break;
      case WIFI_CIPHER_TYPE_TKIP_CCMP:  output->concat("TKIP_CCMP"); break;
      default:                          output->concat("UNKNOWN");   break;
    }
    output->concat("\tGroup: ");
    switch (ap_info->group_cipher) {
      case WIFI_CIPHER_TYPE_NONE:       output->concat("NONE");      break;
      case WIFI_CIPHER_TYPE_WEP40:      output->concat("WEP40");     break;
      case WIFI_CIPHER_TYPE_WEP104:     output->concat("WEP104");    break;
      case WIFI_CIPHER_TYPE_TKIP:       output->concat("TKIP");      break;
      case WIFI_CIPHER_TYPE_CCMP:       output->concat("CCMP");      break;
      case WIFI_CIPHER_TYPE_TKIP_CCMP:  output->concat("TKIP_CCMP"); break;
      default:                          output->concat("UNKNOWN");   break;
    }
  }
  output->concat("\n");
}


/* Initialize Wi-Fi as sta and set scan method */
int8_t ESP32Radio::wifi_scan() {
  int8_t ret = -1;
  if (initialized() && _fsm_is_stable()) {
    ret = _fsm_append_route(2, ESP32RadioState::SCANNING, currentState());
  }
  return ret;
}


/**
*
*
* @return PollResult
*/
FAST_FUNC PollResult ESP32Radio::poll() {
  // Consume mailboxes (event loop task -> deterministic super-loop).
  // NOTE: We deliberately do NOT clear _mb_scan_done here. The FSM consumes that edge.
  _wifi_started  = _mb_wifi_started;
  _sta_connected = _mb_sta_connected;
  _ip4_valid     = _mb_ip4_valid;
  _ip4_addr      = (_ip4_valid ? _mb_ip4_addr : 0);

  // Edge detection: once set, it stays latched until the FSM consumes it.
  if (_mb_scan_done) {
    _scan_done_latched = true;
  }

  return ((0 == _fsm_poll()) ? PollResult::NO_ACTION : PollResult::ACTION);
}


/**
* Called in idle time by the firmware to prod the driver's state machine forward.
* Considers the current driver state, and decides whether or not to advance the
*   driver's state machine.
*
* @return  1 on state shift
*          0 on no action
*         -1 on error
*/
FAST_FUNC int8_t ESP32Radio::_fsm_poll() {
  int8_t ret = 0;
  bool fsm_advance = false;
  switch (currentState()) {
    // Exit conditions: The next state is PRE_INIT.
    case ESP32RadioState::UNINIT:
      fsm_advance = _fsm_is_next_pos(ESP32RadioState::PRE_INIT);
      break;

    // Exit conditions: The threading, net stack and event loop are all marshalled.
    case ESP32RadioState::PRE_INIT:
      fsm_advance = _pre_init_complete();
      break;

    // Exit conditions:
    case ESP32RadioState::RESETTING:
      break;

    // Exit conditions:
    case ESP32RadioState::INIT:
      fsm_advance = initialized();
      break;

    // Exit conditions: The radio event handler has reported the scan finished.
    case ESP32RadioState::SCANNING:
      fsm_advance = _scan_done_latched;
      if (fsm_advance) {
        _collect_scan_results();
        // Consume scan completion edge here (FSM owns it).
        _scan_done_latched = false;
        _mb_scan_done      = false;
      }
      break;

    // Exit conditions:
    case ESP32RadioState::PROMISCUOUS:
      fsm_advance = !_fsm_is_stable();
      break;

    // Exit conditions:
    case ESP32RadioState::CONNECTING:
      // We consider CONNECTED to mean "associated". IP may arrive later.
      fsm_advance = _sta_connected;
      break;

    // Exit conditions:
    case ESP32RadioState::CONNECTED:
      if (_fsm_is_stable()) {
        if (!_sta_connected) {
          fsm_advance = (0 == _fsm_append_state(ESP32RadioState::DISCONNECTED));
        }
        else {
          // Update AP record opportunistically.
          esp_wifi_sta_get_ap_info(&_current_ap);
        }
      }
      fsm_advance = !_fsm_is_stable();

      break;

    // Exit conditions:
    case ESP32RadioState::DISCONNECTING:
      fsm_advance = !_sta_connected;
      break;

    // Exit conditions:
    case ESP32RadioState::DISCONNECTED:
      if (_fsm_is_stable()) {
        if (_sta_connected) {
          _fsm_append_state(ESP32RadioState::CONNECTED);
        }
      }
      fsm_advance = !_fsm_is_stable();
      break;

    // Exit conditions:
    case ESP32RadioState::SLEEPING:
      break;

    // Exit conditions:
    case ESP32RadioState::WAKING:
      break;

    // We can't step our way out of this mess. We need to call init().
    case ESP32RadioState::FAULT:     // If the driver is in a FAULT, do nothing.
      break;
    default:   // Can't exit from an unknown state.
      ret = -1;
      break;
  }

  // If the current state's exit criteria is met, we advance the FSM.
  if (fsm_advance & (-1 != ret)) {
    ret = (0 == _fsm_advance()) ? 1 : 0;
  }
  return ret;
}


/**
* Takes actions appropriate for entry into the given state, and sets the current
*   FSM position if successful. Records the existing state as having been the
*   prior state.
*
* @param The FSM code to test.
* @return 0 on success, -1 otherwise.
*/
FAST_FUNC int8_t ESP32Radio::_fsm_set_position(ESP32RadioState new_state) {
  int8_t ret = -1;
  const ESP32RadioState CURRENT_STATE = currentState();
  if (_fsm_is_waiting()) return ret;
  bool state_entry_success = false;
  switch (new_state) {
    // Entry conditions: Never.
    case ESP32RadioState::UNINIT:
      _set_fault("Tried to _fsm_set_position(UNINIT)");
      break;

    // Entry conditions: Unconditional. If we're here, we are clear to proceed.
    case ESP32RadioState::PRE_INIT:
      if (!_flags.value(ESP32RADIO_FLAG_NETIF_INIT)) {
        _flags.set(ESP32RADIO_FLAG_NETIF_INIT, (ESP_OK == esp_netif_init()));
      }
      if (!_flags.value(ESP32RADIO_FLAG_EVENT_LOOP_CREATED)) {
        switch (esp_event_loop_create_default()) {
          case ESP_OK:
          case ESP_ERR_INVALID_STATE:
            _flags.set(ESP32RADIO_FLAG_EVENT_LOOP_CREATED);
            ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, &_handler_any_id));
            ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,   ESP_EVENT_ANY_ID, &ip_event_handler,   this, &_handler_ip_any_id));
            break;
          default:
            _set_fault("Unable to create default event loop.");
            break;
        }
      }

      ESP32Radio::_scan_list_count  = 0;
      ESP32Radio::_scan_total_count = 0;
      _mb_scan_done      = false;
      _scan_done_latched = false;
      memset(ESP32Radio::_scan_list_ap, 0, sizeof(ESP32Radio::_scan_list_ap));  // Wipe the record of nearby APs.
      memset(&_current_ap, 0, sizeof(_current_ap));  // Wipe out current AP.
      // Clear network state.
      _mb_sta_connected = false;
      _mb_ip4_valid     = false;
      _mb_ip4_addr      = 0;
      state_entry_success = true;
      break;


    // Entry conditions:
    case ESP32RadioState::RESETTING:
      _flags.clear(~ESP32RADIO_FLAG_RESET_MASK);
      state_entry_success = true;
      break;


    // Entry conditions:
    case ESP32RadioState::INIT:
      if (!_flags.value(ESP32RADIO_FLAG_WIFI_INIT)) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        _flags.set(ESP32RADIO_FLAG_WIFI_INIT, (ESP_OK == esp_wifi_init(&cfg)));
      }
      if (!_flags.value(ESP32RADIO_FLAG_INIT_WIFI_AS_STATION)) {
        // Setup Wifi peripheral in station mode.
        _sta_netif = esp_netif_create_default_wifi_sta();
        if (nullptr != _sta_netif) {
          _flags.set(ESP32RADIO_FLAG_INIT_WIFI_AS_STATION, (ESP_OK == esp_wifi_set_mode(WIFI_MODE_STA)));
        }
      }
      if (nullptr != _sta_netif) {
        if (!_flags.value(ESP32RADIO_FLAG_WIFI_STARTED)) {
          _flags.set(ESP32RADIO_FLAG_WIFI_STARTED, (ESP_OK == esp_wifi_start()));
        }
      }
      state_entry_success = true;
      break;


    // Entry conditions: Scan started. Start scan asynchronously; completion comes via WIFI_EVENT_SCAN_DONE.
    case ESP32RadioState::SCANNING: {
      ESP32Radio::_scan_list_count  = 0;
      ESP32Radio::_scan_total_count = 0;
      _mb_scan_done      = false;
      _scan_done_latched = false;
      memset(ESP32Radio::_scan_list_ap, 0, sizeof(ESP32Radio::_scan_list_ap));  // Wipe the record of nearby APs.

      wifi_scan_config_t scan_cfg;
      memset(&scan_cfg, 0, sizeof(scan_cfg));
      // TODO: Configure scan_cfg if you want active/passive, channel masks, etc.
      state_entry_success = (ESP_OK == esp_wifi_scan_start(&scan_cfg, false));
      break;
    }


    // Entry conditions:
    case ESP32RadioState::PROMISCUOUS:
      state_entry_success = true;
      break;

    // Entry conditions: FSM owns association attempt.
    case ESP32RadioState::CONNECTING:
      state_entry_success = (ESP_OK == esp_wifi_connect());
      break;

    // Entry conditions:
    case ESP32RadioState::CONNECTED:
      // Reset any retry logic you might add later.
      esp_wifi_sta_get_ap_info(&_current_ap);
      state_entry_success = true;
      break;

    // Entry conditions: FSM owns disassociation.
    case ESP32RadioState::DISCONNECTING:
      state_entry_success = (ESP_OK == esp_wifi_disconnect());
      break;

    // Entry conditions:
    case ESP32RadioState::DISCONNECTED:
      memset(&_current_ap, 0, sizeof(_current_ap));  // Wipe out current AP.
      // Losing link implies losing IP.
      _mb_ip4_valid = false;
      _mb_ip4_addr  = 0;
      state_entry_success = true;
      break;

    // Entry conditions:
    case ESP32RadioState::SLEEPING:
      state_entry_success = true;
      break;

    // Entry conditions:
    case ESP32RadioState::WAKING:
      state_entry_success = true;
      break;

    // We allow fault entry to be done this way.
    case ESP32RadioState::FAULT:
      state_entry_success = true;
      _set_fault("Explicit FSM waypoint");
      break;

    default:
      _set_fault("Unhandled ESP32RadioState");
      break;
  }

  if (state_entry_success) {
    if (_log_verbosity >= LOG_LEV_NOTICE) c3p_log(LOG_LEV_NOTICE, "ESP32Radio::_fsm_set_position", "ESP32Radio State %s ---> %s", _FSM_STATES.enumStr(CURRENT_STATE), _FSM_STATES.enumStr(new_state));
    ret = 0;
  }
  return ret;
}



/**
* Collect scan results deterministically after WIFI_EVENT_SCAN_DONE.
*/
void ESP32Radio::_collect_scan_results() {
  uint16_t number = ESP32RADIO_SCAN_RESULTS;
  ESP32Radio::_scan_list_count  = 0;
  ESP32Radio::_scan_total_count = 0;

  if (ESP_OK == esp_wifi_scan_get_ap_num(&ESP32Radio::_scan_total_count)) {
    if (ESP_OK == esp_wifi_scan_get_ap_records(&number, ESP32Radio::_scan_list_ap)) {
      ESP32Radio::_scan_list_count = number;
    }
  }
}


/**
* Put the driver into a FAULT state.
*
* @param msg is a debug string to be added to the log.
*/
void ESP32Radio::_set_fault(const char* msg) {
  if (_log_verbosity >= LOG_LEV_WARN) c3p_log(LOG_LEV_WARN, "ESP32Radio::_set_fault", "ESP32Radio fault: %s", msg);
  _fsm_mark_current_state(ESP32RadioState::FAULT);
}
