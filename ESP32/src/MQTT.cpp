/*
File:   MQTTClient.cpp
Author: J. Ian Lindsay
Date:   2025.12.27

This file contains a reusable wrapper for the ESP32's MQTT features.
*/

#include "../ESP32.h"


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
MQTTClient* MQTT_CLIENT_INSTANCE = nullptr;


const EnumDef<MQTTCliState> _STATE_LIST[] = {
  { MQTTCliState::UNINIT,        "UNINIT"},
  { MQTTCliState::INIT,          "INIT"},
  { MQTTCliState::CONNECTING,    "CONNECTING"},
  { MQTTCliState::CONNECTED,     "CONNECTED"},
  { MQTTCliState::DISCONNECTING, "DISCONNECTING"},
  { MQTTCliState::DISCONNECTED,  "DISCONNECTED"},
  { MQTTCliState::FAULT,         "FAULT"},
  { MQTTCliState::INVALID,       "INVALID", (ENUM_WRAPPER_FLAG_CATCHALL)}  // FSM hygiene.
};
const EnumDefList<MQTTCliState> _FSM_STATES(&_STATE_LIST[0], (sizeof(_STATE_LIST) / sizeof(_STATE_LIST[0])), "MQTTCliState");



/*******************************************************************************
* ESP-MQTT event handler
* NOTE: This is still running inside ESP-IDF's event loop task. It should not
*   call into FSM methods (route changes) unless you add a mailbox mechanism.
*******************************************************************************/
void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
  static const char* const TAG = "mqtt_event_handler";
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;

  switch ((esp_mqtt_event_id_t) event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
      ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
      msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
      msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
      msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
      ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
      break;

    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
      ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
      break;

    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;

    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;

    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG, "MQTT_EVENT_DATA");
      printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
      printf("DATA=%.*s\r\n", event->data_len, event->data);
      break;

    case MQTT_EVENT_ERROR:
      ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
      }
      break;

    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}



/*******************************************************************************
* MQTTBrokerDef
* NOTE: This would be a good abstraction point for AWS-IoT implementation.
*******************************************************************************/

MQTTBrokerDef::MQTTBrokerDef(const char* LABEL) {
  label(LABEL);
  _cli_conf = {
    .broker = {
      .address = {
        .uri = "mqtt://192.168.0.3",
      }
    },
    .credentials = {
      .username = "dgmj-mqtt",
      .authentication = {
        .password = "dgmj-mqtt",
      }
    },
    .session = {
      .last_will = {
        .topic = "/topic/will",
        .msg = "i will leave",
        .msg_len = 12,
        .qos = 1,
        .retain = true,
      },
      .protocol_ver = MQTT_PROTOCOL_V_5,
    },
    .network = {
      .disable_auto_reconnect = true,
    },
  };
}


void MQTTBrokerDef::label(const char* LABEL) {
  const uint32_t FIELD_SIZE = sizeof(_label);
  memset(_label, 0, FIELD_SIZE);
  //memcpy(_label, LABEL, strict_min((uint32_t) strlen(LABEL), FIELD_SIZE-1));
}


void MQTTBrokerDef::uri(const char* URI) {
  const uint32_t FIELD_SIZE = sizeof(_cli_conf.broker.address.uri);
  memset((void*) _cli_conf.broker.address.uri, 0, FIELD_SIZE);
  memcpy((void*) _cli_conf.broker.address.uri, URI, strict_min((uint32_t) strlen(URI), FIELD_SIZE-1));
}


void MQTTBrokerDef::user(const char* USER) {
  const uint32_t FIELD_SIZE = sizeof(_cli_conf.credentials.username);
  memset((void*) _cli_conf.credentials.username, 0, FIELD_SIZE);
  memcpy((void*) _cli_conf.credentials.username, USER, strict_min((uint32_t) strlen(USER), FIELD_SIZE-1));
}


void MQTTBrokerDef::passwd(const char* PASSWD) {
  const uint32_t FIELD_SIZE = sizeof(_cli_conf.credentials.authentication.password);
  memset((void*) _cli_conf.credentials.authentication.password, 0, FIELD_SIZE);
  memcpy((void*) _cli_conf.credentials.authentication.password, PASSWD, strict_min((uint32_t) strlen(PASSWD), FIELD_SIZE-1));
}


int8_t MQTTBrokerDef::serialize(StringBuilder* output) {
  int8_t ret = 0;
  return ret;
}


void MQTTBrokerDef::printDebug(StringBuilder* output) {
  const char* LABEL  = label();
  const char* URI    = uri();
  const char* USER   = user();
  const char* PASSWD = passwd();
  if ((0 < strlen(LABEL)) & (0 < strlen(URI))) {
    const char* safe_u_str = ((0 == strlen(USER)) ? "[UNSET]" : USER);
    const char* safe_p_str = ((0 == strlen(PASSWD)) ? "[UNSET]" : PASSWD);
    output->concatf("\t[%s]\t%s:%s@%s\n", LABEL, safe_u_str, safe_p_str, URI);
    //if ((nullptr != auth) & (0 < auth_len)) {
    //  output->concatf("\t\tExtra authentication data: (%u bytes):\n", auth_len);
    //  StringBuilder::printBuffer(output, auth, (uint32_t) auth_len, "\t\t");
    //}
  }
  output->concat("\n");
}



/*
* Constructor
*/
MQTTClient::MQTTClient() :
  StateMachine<MQTTCliState>("MQTTClient-FSM", &_FSM_STATES, MQTTCliState::UNINIT, 8)
{
  MQTT_CLIENT_INSTANCE = this;
  _radio = nullptr;
}


/*
* Destructor
*/
MQTTClient::~MQTTClient() {}



int8_t MQTTClient::init() {
  int8_t ret = 0;
  _fsm_set_route(2, MQTTCliState::INIT, MQTTCliState::CONNECTING);
  return ret;
}


/*******************************************************************************
* MQTT console support
*******************************************************************************/

void MQTTClient::printDebug(StringBuilder* output) {
  StringBuilder tmp;
  StringBuilder prod_str("MQTTClient");
  prod_str.concatf(" [%sinitialized]", (initialized() ? "" : "un"));
  StringBuilder::styleHeader2(&tmp, (const char*) prod_str.string());
  printFSM(&tmp);

  if (initialized()) {
    if (connected()) {
      tmp.concat("Connected to broker:\n");
      _current_broker.printDebug(&tmp);
    }
    if (nullptr != _radio) {
      tmp.concatf("\t Radio link:    %c\n", (_radio->linkUp() ? 'y' : 'n'));
      tmp.concatf("\t Radio hasIP:   %c\n", (_radio->hasIP() ? 'y' : 'n'));
      if (_radio->hasIP()) {
        const uint32_t ip = _radio->ip4();
        tmp.concatf(
          "\t Radio IPv4:    %u.%u.%u.%u\n",
          (ip & 0xFF),
          ((ip >> 8) & 0xFF),
          ((ip >> 16) & 0xFF),
          ((ip >> 24) & 0xFF)
        );
      }
    }
    else {
      tmp.concat("\t Radio:         [UNBOUND]\n");
    }
  }
  else {
    tmp.concatf("\t ESP_MQTT_INIT:      %c\n", (_flags.value(MQTT_FLAG_ESP_MQTT_INIT) ? 'y' : 'n'));
    tmp.concatf("\t EVENT_REGISTERED:   %c\n", (_flags.value(MQTT_FLAG_EVENT_REGISTERED) ? 'y' : 'n'));
    tmp.concatf("\t EVENT_LOOP_CREATED: %c\n", (_flags.value(MQTT_FLAG_EVENT_LOOP_CREATED) ? 'y' : 'n'));
  }

  tmp.string();
  output->concatHandoff(&tmp);
}



int MQTTClient::console_handler_mqtt_client(StringBuilder* txt_ret, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);

  if (0 == StringBuilder::strcasecmp(cmd, "broker")) {
    if (4 == args->count()) {
      char* uri = args->position_trimmed(1);
      char* user = args->position_trimmed(3);
      char* pass = args->position_trimmed(2);

      _current_broker.uri(uri);
      _current_broker.user(user);
      _current_broker.passwd(pass);
      txt_ret->concat("Broker updated.\n");
    }
    else {
      txt_ret->concatf("Usage: %s <uri> <user> <pass>.\n", cmd);
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "con")) {
    if (!connected()) {
      ret = _fsm_append_route(2, MQTTCliState::CONNECTING, MQTTCliState::CONNECTED);
    }
    else {
      txt_ret->concat("MQTTClient is already connected.\n");
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "discon")) {
    if (connected()) {
      ret = _fsm_append_route(2, MQTTCliState::DISCONNECTING, MQTTCliState::DISCONNECTED);
    }
    else {
      txt_ret->concat("MQTTClient is already disconnected.\n");
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




/**
*
*
* @return PollResult
*/
FAST_FUNC PollResult MQTTClient::poll() {
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
FAST_FUNC int8_t MQTTClient::_fsm_poll() {
  int8_t ret = 0;
  bool fsm_advance = false;

  switch (currentState()) {
    // Exit conditions: The next state is INIT.
    case MQTTCliState::UNINIT:
      fsm_advance = _fsm_is_next_pos(MQTTCliState::INIT);
      break;

    // Exit conditions:
    case MQTTCliState::INIT:
      fsm_advance = initialized();
      break;

    // Exit conditions: Wait for routability.
    case MQTTCliState::CONNECTING:
      if (nullptr != _radio) {
        if (!_radio->linkUp() || !_radio->hasIP()) {
          // Network isn't ready. Park in DISCONNECTED and prepare to retry.
          fsm_advance = (0 == _fsm_prepend_state(MQTTCliState::DISCONNECTED));
        }
        else {
          fsm_advance = true;  // Ready to advance into CONNECTED waypoint (or next planned).
        }
      }
      if (fsm_advance & _fsm_is_stable()) {
        fsm_advance = (0 == _fsm_append_state(MQTTCliState::CONNECTED));
      }
      break;

    // Exit conditions:
    case MQTTCliState::CONNECTED:
      if (_fsm_is_stable()) {
        // If network collapses beneath us, drive toward disconnect.
        if ((nullptr != _radio) && (!_radio->linkUp() || !_radio->hasIP())) {
          fsm_advance = (0 == _fsm_append_route(2, MQTTCliState::DISCONNECTING, MQTTCliState::DISCONNECTED));
        }
      }
      fsm_advance = !_fsm_is_stable();
      break;

    // Exit conditions:
    case MQTTCliState::DISCONNECTING:
      fsm_advance = !_fsm_is_stable();
      break;

    // Exit conditions:
    case MQTTCliState::DISCONNECTED:
      fsm_advance = !_fsm_is_stable();
      break;

    // We can't step our way out of this mess. We need to call init().
    case MQTTCliState::FAULT:
      break;

    default:
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
FAST_FUNC int8_t MQTTClient::_fsm_set_position(MQTTCliState new_state) {
  int8_t ret = -1;
  const MQTTCliState CURRENT_STATE = currentState();
  if (_fsm_is_waiting()) return ret;
  bool state_entry_success = false;
  switch (new_state) {
    // Entry conditions: Never.
    case MQTTCliState::UNINIT:
      _set_fault("Tried to _fsm_set_position(UNINIT)");
      break;

    // Entry conditions: Unconditional. If we're here, we are clear to proceed.
    case MQTTCliState::INIT:
      if (!_flags.value(MQTT_FLAG_EVENT_LOOP_CREATED)) {
        switch (esp_event_loop_create_default()) {
          case ESP_OK:
          case ESP_ERR_INVALID_STATE:
            _flags.set(MQTT_FLAG_EVENT_LOOP_CREATED);
            break;
          default:
            _set_fault("Unable to create default event loop.");
            break;
        }
      }

      if (!_flags.value(MQTT_FLAG_ESP_MQTT_INIT)) {
        _client_handle = esp_mqtt_client_init(_current_broker.config());
        _flags.set(MQTT_FLAG_ESP_MQTT_INIT, (nullptr != _client_handle));
      }

      if (!_flags.value(MQTT_FLAG_EVENT_REGISTERED)) {
        // We pass our own pointer to the event registration so that we can track
        //   context between multiple broker connections, if necessary.
        switch (esp_mqtt_client_register_event(_client_handle, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, mqtt_event_handler, this)) {
          case ESP_OK:
            _flags.set(MQTT_FLAG_EVENT_REGISTERED);
            break;
          default:
            _set_fault("Unable to esp_mqtt_client_register_event().");
            break;
        }
      }
      state_entry_success = true;
      break;

    // Entry conditions: Network must be ready. We gate on radio status.
    case MQTTCliState::CONNECTING:
      if ((nullptr != _radio) && _radio->linkUp() && _radio->hasIP()) {
        state_entry_success = (ESP_OK == esp_mqtt_client_start(_client_handle));
      }
      break;

    // Entry conditions:
    case MQTTCliState::CONNECTED:
      state_entry_success = true;
      break;

    // Entry conditions:
    case MQTTCliState::DISCONNECTING:
      // NOTE: esp_mqtt_client_stop() must not be called from the event handler's
      //   stack frame.
      state_entry_success = (ESP_OK == esp_mqtt_client_stop(_client_handle));
      break;

    // Entry conditions:
    case MQTTCliState::DISCONNECTED:
      state_entry_success = true;
      break;

    // We allow fault entry to be done this way.
    case MQTTCliState::FAULT:
      state_entry_success = true;
      _set_fault("Explicit FSM waypoint");
      break;

    default:
      _set_fault("Unhandled MQTTCliState");
      break;
  }

  if (state_entry_success) {
    if (_log_verbosity >= LOG_LEV_NOTICE) {
      c3p_log(LOG_LEV_NOTICE, "MQTTClient::_fsm_set_position", "MQTTClient State %s ---> %s",
        _FSM_STATES.enumStr(CURRENT_STATE), _FSM_STATES.enumStr(new_state));
    }
    ret = 0;
  }
  return ret;
}


/**
* Put the driver into a FAULT state.
*
* @param msg is a debug string to be added to the log.
*/
void MQTTClient::_set_fault(const char* msg) {
  if (_log_verbosity >= LOG_LEV_WARN) c3p_log(LOG_LEV_WARN, "MQTTClient::_set_fault", "MQTTClient fault: %s", msg);
  _fsm_mark_current_state(MQTTCliState::FAULT);
}
