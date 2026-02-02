/*
File:   MQTT.cpp
Author: J. Ian Lindsay
Date:   2025.12.27

This file contains a reusable wrapper for the ESP32's MQTT features.

MQTT does NOT register for IP stack events.
Network readiness is queried from ESP32Radio via injected pointer.
MQTT connection truth is mailbox-driven from ESP-MQTT event handler.
*/

#include "../ESP32.h"

#if defined(__BUILD_HAS_CBOR)
  #include "cbor-cpp/cbor.h"
#endif

#if !defined(CONFIG_MQTT_PROTOCOL_5)
  #error "This driver requires CONFIG_MQTT_PROTOCOL_5. Enable it in menuconfig (Component config → MQTT → MQTT Protocol v5)."
#endif

// TODO: Test for MQTT_TRANSPORT_SSL MQTT_TRANSPORT_WS and sec variants, and
//   build support accordingly. This is mostly to save weight in the binary,
//   but will also prevent surprises at runtime attributable to missing support.

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

const EnumDef<MQTTCliState> _STATE_LIST[] = {
  { MQTTCliState::UNINIT,        "UNINIT"},
  { MQTTCliState::INIT,          "INIT"},
  { MQTTCliState::CONNECTING,    "CONNECTING"},
  { MQTTCliState::CONNECTED,     "CONNECTED"},
  { MQTTCliState::SUBSCRIBING,   "SUBSCRIBING"},
  { MQTTCliState::DISCONNECTING, "DISCONNECTING"},
  { MQTTCliState::DISCONNECTED,  "DISCONNECTED"},
  { MQTTCliState::FAULT,         "FAULT"},
  { MQTTCliState::INVALID,       "INVALID", (ENUM_WRAPPER_FLAG_CATCHALL)}  // FSM hygiene.
};
const EnumDefList<MQTTCliState> _FSM_STATES(&_STATE_LIST[0], (sizeof(_STATE_LIST) / sizeof(_STATE_LIST[0])), "MQTTCliState");



/*******************************************************************************
* ESP-MQTT event handler
* NOTE: This is running inside ESP-IDF's event loop task. It must not call into
*   FSM route APIs. It *may* set mailboxes.
*******************************************************************************/
void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
  static const char* const TAG = "mqtt_event_handler";
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
  esp_mqtt_client_handle_t client = event->client;
  MQTTClientESP32* self = (MQTTClientESP32*) handler_args;

  switch ((esp_mqtt_event_id_t) event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      if (nullptr != self) {
        self->_mb_set_mqtt_connected(true);
        self->_mb_set_mqtt_disconnected(false);
      }
      break;

    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      if (nullptr != self) {
        self->_mb_set_mqtt_connected(false);
        self->_mb_set_mqtt_disconnected(true);
      }
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      self->_deliver_suback(event->msg_id);
      break;

    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;

    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;

    case MQTT_EVENT_DATA:
      {
        MQTTMessage* n_msg = new MQTTMessage();
        if (nullptr != n_msg) {
          n_msg->topic = event->topic;
          n_msg->msg_id = event->msg_id;
          n_msg->qos = event->qos;
          n_msg->retain = event->retain;
          n_msg->dup = event->dup;
          n_msg->data.concat((uint8_t*) event->data, event->data_len);
          // Stall the MQTT thread if the comms wrapper hasn't caught up.
          while (0 != self->_deliver_msg(n_msg)) {
            platform.yieldThread();
          }
        }
      }
      break;


    case MQTT_EVENT_ERROR:
      ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      if (nullptr != self) {
        self->_mb_set_mqtt_connected(false);
        self->_mb_set_mqtt_disconnected(true);
        if (nullptr != event->error_handle) {
          switch (event->error_handle->error_type) {
            case MQTT_ERROR_TYPE_NONE:  break;  // Explicitly ignored.

            case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
              switch (event->error_handle->connect_return_code) {
                // These failures should permanently disable greedy reconnect.
                case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
                case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
                case MQTT_CONNECTION_REFUSE_PROTOCOL:
                  self->_current_broker.autoconnect(false);
                  break;

                case MQTT_CONNECTION_REFUSE_ID_REJECTED:
                case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
                  // Currently ignored, but explicitly acknowledged.
                default:   // Future or unknown connection-refused reasons.
                  ESP_LOGW(TAG, "Unhandled MQTT CONNECTION_REFUSED: (connect_return_code = %u)", (event->error_handle->connect_return_code));
                  break;
              }
              break;

            case MQTT_ERROR_TYPE_TCP_TRANSPORT:
              ESP_LOGW(TAG, "Unhandled MQTT_ERROR_TYPE_TCP_TRANSPORT: (%s)", strerror(event->error_handle->esp_transport_sock_errno));
              break;

            case MQTT_ERROR_TYPE_SUBSCRIBE_FAILED:
              ESP_LOGW(TAG, "MQTT_ERROR_TYPE_SUBSCRIBE_FAILED");
              break;

            // Future ESP-IDF error types land here.
            default:
              ESP_LOGW(TAG, "Unhandled error_type: (%u)", event->error_handle->error_type);
              break;
          }
        }
      }
      break;

    case MQTT_EVENT_BEFORE_CONNECT:
      if (nullptr != self) {
        esp_mqtt5_connection_property_config_t conn_prop = {0};
        conn_prop.maximum_packet_size = MQTT_MAX_PACKET_SIZE;
        // Note: API takes esp_mqtt5_client_handle_t, but the handle type is compatible in ESP-MQTT.
        esp_mqtt5_client_set_connect_property((esp_mqtt5_client_handle_t) client, &conn_prop);
      }
      break;

    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}



/*******************************************************************************
* MQTTBrokerDefESP32
*******************************************************************************/

MQTTBrokerDefESP32::MQTTBrokerDefESP32(const char* LABEL) : MQTTBrokerDef(LABEL) {
  cli_conf = {
    .broker = {
      .address = {
        .uri = _uri,
      }
    },
    .credentials = {
      .username = _usr,
      .authentication = {
        .password = _pass,
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
    .buffer = {
      .size = MQTT_MAX_PACKET_SIZE,
      .out_size = MQTT_MAX_PACKET_SIZE,
    },
  };
}



/*******************************************************************************
* MQTTClientESP32
*******************************************************************************/

/*
* Constructor
*/
MQTTClientESP32::MQTTClientESP32() :
  StateMachine<MQTTCliState>("MQTTClientESP32-FSM", &_FSM_STATES, MQTTCliState::UNINIT, 8)
{
  _radio = nullptr;
  _client_handle = nullptr;

  // Policy defaults.
  _flags.set(MQTT_FLAG_AUTOCONNECT, true);
  _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);

  // Backoff defaults.
  _reconnect_backoff_ms     = 5000;
  _reconnect_backoff_ms_max = 60000;

  _connect_attempt_active     = false;
  _mqtt_connected_latched     = false;
  _mqtt_disconnected_latched  = false;

  _sub_pending_msg_id = -1;
  _mb_suback_msg_id   = -1;
}


/*
* Destructor
*/
MQTTClientESP32::~MQTTClientESP32() {}


int8_t MQTTClientESP32::init() {
  int8_t ret = 0;
  // Do not imply "connect now". We idle in DISCONNECTED and let policy drive.
  _fsm_set_route(2, MQTTCliState::INIT, MQTTCliState::DISCONNECTED);
  return ret;
}


bool MQTTClientESP32::setBroker(MQTTBrokerDef* n_broker) {
  return _current_broker.set(n_broker);
}


/*******************************************************************************
* Mailbox helpers (event-loop context)
*******************************************************************************/
void MQTTClientESP32::_mb_set_mqtt_connected(const bool v) { _mb_mqtt_connected = v; }
void MQTTClientESP32::_mb_set_mqtt_disconnected(const bool v) { _mb_mqtt_disconnected = v; }


/**
* Called when any broker property changes.
* Clears init flags and plans a regression to INIT (disconnecting first if needed).
*/
void MQTTClientESP32::_broker_changed_reinit_plan() {
  // Clear things that are specific to the current client handle.
  _flags.clear(MQTT_FLAG_ESP_MQTT_INIT | MQTT_FLAG_EVENT_REGISTERED);

  // Any in-flight attempt should be considered invalid now.
  _connect_attempt_active = false;
  _mqtt_connected_latched = false;
  _mqtt_disconnected_latched = false;

  // Ensure we end up in INIT so that the client handle is rebuilt using new broker config.
  // If currently connected, we must gracefully stop first.
  if (connected()) {
    // Blow out our existing state plan with one that
    _fsm_set_route(4, MQTTCliState::DISCONNECTING, MQTTCliState::DISCONNECTED, MQTTCliState::INIT, MQTTCliState::DISCONNECTED);
  }
  else {
    // If we're CONNECTING/DISCONNECTED/INIT, just ensure INIT is next so it rebuilds.
    _fsm_prepend_state(MQTTCliState::INIT);
  }
}



/**
*
* @return 0 on success, or negative on failure
*/
int MQTTClientESP32::publish(MQTTMessage* msg) {
  int ret = -1;
  if (connected()) {
    const uint8_t QOS = 0;
    // Use the non-blocking outbox API.
    // TODO: Even though we don't get a msg_id back.
    //msg_id = esp_mqtt_client_enqueue(_client_handle, msg->topic, msg->data.string(), msg->data.length(), msg->qos, (msg->retain ? 1:0), true);
    //if (msg_id) {}
    if (ESP_OK == esp_mqtt_client_enqueue(_client_handle, msg->topic, (const char*) msg->data.string(), msg->data.length(), msg->qos, (msg->retain ? 1:0), true)) {
      msg->msg_id = 0;
      ret = 0;
    }
  }
  return ret;
}






int MQTTClientESP32::_add_sub(MQTTSub* n_sub) {
  int ret = 0;
  if (nullptr != _client_subs) {
    MQTTSub* current = _client_subs;
    ret++;
    while (nullptr != _client_subs->next) {
      current = _client_subs->next;
      ret++;
    }
    _client_subs->next = n_sub;
  }
  else {
    _client_subs = n_sub;
  }
  return ret;
}



int MQTTClientESP32::_drop_sub(MQTTSub* n_sub) {
  int ret = -1;
  return ret;
}





/**
*
* @return At least 0 on success, or negative on failure
*/
int MQTTClientESP32::subscribe(const char* TOPIC_STR, const uint8_t QOS, MQTTTopicCallback CB) {
  int ret = -1;
  MQTTSub* current = ((nullptr == _client_subs) ? nullptr : _client_subs->havingTopic(TOPIC_STR));
  if (nullptr == current) {   // Topic is not currently added.
    current = new MQTTSub(TOPIC_STR, QOS, CB);  // Add it.
    if (nullptr != current) {
      ret = _add_sub(current);
    }
  }
  else {
    // If we aready had that topic defined, just allow calls to set the
    //   trim and report success.
    current->rx_callback = CB;
    current->qos   = QOS;
    current->sub_state = MQTTSubState::NEG;
    ret = 0;
  }

  // What we do now depends on our state.
  if ((MQTTCliState::CONNECTED == currentState()) && _fsm_is_stable()) {
    ret = _fsm_prepend_state(MQTTCliState::SUBSCRIBING);
  }
  return ret;
}


/**
*
* @return topic_idx on success, or negative on failure
*/
int MQTTClientESP32::unsubscribe(const char* TOPIC_STR) {
  int msg_id = -1;
  if (connected()) {
    msg_id = esp_mqtt_client_unsubscribe(_client_handle, TOPIC_STR);
  }
  return msg_id;
}


/**
*
* @return PollResult
*/
FAST_FUNC PollResult MQTTClientESP32::poll() {
  // Latch mailboxes (event-loop -> poll/FSM context).
  if (_mb_mqtt_connected) {
    _mqtt_connected_latched = true;
    _mqtt_disconnected_latched = false;
    _mb_mqtt_connected = false;
  }
  if (_mb_mqtt_disconnected) {
    _mqtt_connected_latched = false;
    _mqtt_disconnected_latched = true;
    _mb_mqtt_disconnected = false;
  }

  // SUBACK mailbox handling.
  if (_mb_suback_msg_id >= 0) {
    // Only accept if it matches current pending.
    if (_sub_pending_msg_id == _mb_suback_msg_id) {
      _sub_pending_msg_id = -1;
    }
    _mb_suback_msg_id = -1;
  }

  return ((0 == _fsm_poll()) ? PollResult::NO_ACTION : PollResult::ACTION);
}


// We always return success.
int MQTTClientESP32::_deliver_suback(const int32_t ACK_ID) {
  int ret = 0;
  if (nullptr != _client_subs) {
    MQTTSub* current = _client_subs->havingAckID(ACK_ID);
    if (nullptr != current) {
      current->sub_state = MQTTSubState::POS;
    }
  }
  return ret;
}


int MQTTClientESP32::_deliver_msg(MQTTMessage* msg) {
  int ret = -1;
  if (nullptr == _mb_waiting_msg) {
    _mb_waiting_msg = msg;
    ret = 0;
  }
  return ret;
}


FAST_FUNC bool MQTTClientESP32::connected() {
  switch (currentState()) {
    case MQTTCliState::CONNECTED:
    case MQTTCliState::SUBSCRIBING:
      return true;

    default:
      break;
  }
  return false;
};



/**
* Called in idle time by the firmware to prod the driver's state machine forward.
*
* @return  1 on state shift
*          0 on no action
*         -1 on error
*/
FAST_FUNC int8_t MQTTClientESP32::_fsm_poll() {
  int8_t ret = 0;
  bool fsm_advance = false;

  switch (currentState()) {
    case MQTTCliState::UNINIT:
      fsm_advance = _fsm_is_next_pos(MQTTCliState::INIT);
      break;

    case MQTTCliState::INIT:
      fsm_advance = initialized();
      break;

    // Exit conditions:
    //   - if network not ready: park
    //   - if mqtt event says connected: advance
    //   - if mqtt event says disconnected/error: park (and backoff applies at DISCONNECTED entry)
    case MQTTCliState::CONNECTING:
      if ((nullptr == _radio) || !_radio->linkUp() || !_radio->hasIP()) {
        fsm_advance = (0 == _fsm_prepend_state(MQTTCliState::DISCONNECTED));
      }
      else if (_mqtt_connected_latched) {
        fsm_advance = true;
      }
      else if (_mqtt_disconnected_latched) {
        fsm_advance = (0 == _fsm_prepend_state(MQTTCliState::DISCONNECTED));
      }
      break;

    case MQTTCliState::CONNECTED:
      if (_fsm_is_stable()) {
        // Network collapse -> stop client, then park.
        if ((nullptr == _radio) || !_radio->linkUp() || !_radio->hasIP()) {
          fsm_advance = (0 == _fsm_append_route(2, MQTTCliState::DISCONNECTING, MQTTCliState::DISCONNECTED));
        }
        // MQTT collapse -> park.
        else if (!_mqtt_connected_latched || _mqtt_disconnected_latched) {
          fsm_advance = (0 == _fsm_append_state(MQTTCliState::DISCONNECTED));
        }
        StringBuilder* subs = _current_broker.subs();
        int local_ret = 0;
        for (int i = 0; i < subs->count(); i++) {
          local_ret -= subscribe(subs->position(i), 0);
        }
      }

      fsm_advance = !_fsm_is_stable();
      if ((!fsm_advance) && (nullptr != _mb_waiting_msg)) {
        _handle_mqtt_message((MQTTMessage*) _mb_waiting_msg);
        delete _mb_waiting_msg;
        _mb_waiting_msg = nullptr;
      }
      break;


    // Exit conditions: There are no MQTTSub subs that are not in MQTTSubState::POS.
    case MQTTCliState::SUBSCRIBING:
      {
        fsm_advance = true;
        MQTTSub* current = _client_subs;
        while (fsm_advance && (nullptr != current)) {
          // NOTE: No breaks;
          switch (current->sub_state) {
            case MQTTSubState::NEG:
              // Send a subscription request.
              {
                int msg_id = esp_mqtt_client_subscribe_single(_client_handle, current->topic, current->qos);
                if (0 <= msg_id) {
                  current->pending_msg_id = msg_id;
                  current->sub_state = MQTTSubState::INFLIGHT_UP;
                }
              }
            case MQTTSubState::INFLIGHT_UP:
            case MQTTSubState::INFLIGHT_DOWN:
              fsm_advance = false;
            case MQTTSubState::POS:
              break;
            // NOTE: No default case to ensure we covered them all explicitly.
          }
          current = current->next;
        }
        _flags.set(MQTT_FLAG_SUBS_COMPLETE, fsm_advance);
      }
      break;


    case MQTTCliState::DISCONNECTING:
      fsm_advance = !_fsm_is_stable();
      break;

    // Greedy policy is applied here (while stable).
    case MQTTCliState::DISCONNECTED:
      if (_fsm_is_stable()) {
        const bool greedy = (autoconnect() && _current_broker.autoconnect() && _current_broker.isValid());
        const bool net_ok = ((nullptr != _radio) && _radio->linkUp() && _radio->hasIP());
        if (greedy && net_ok) {
          _fsm_append_route(2, MQTTCliState::CONNECTING, MQTTCliState::CONNECTED);
        }
      }
      fsm_advance = !_fsm_is_stable();
      break;

    case MQTTCliState::FAULT:
      break;

    default:
      ret = -1;
      break;
  }

  if (fsm_advance & (-1 != ret)) {
    ret = (0 == _fsm_advance()) ? 1 : 0;
  }
  return ret;
}


/**
* Attempt a state entry.
*
* @return 0 on success, -1 otherwise.
*/
FAST_FUNC int8_t MQTTClientESP32::_fsm_set_position(MQTTCliState new_state) {
  int8_t ret = -1;
  const MQTTCliState CURRENT_STATE = currentState();
  if (_fsm_is_waiting()) return ret;

  bool state_entry_success = false;

  switch (new_state) {
    case MQTTCliState::UNINIT:
      _set_fault("Tried to _fsm_set_position(UNINIT)");
      break;

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

      // If we're re-initting, tear down prior handle first.
      if (nullptr != _client_handle) {
        // Stop is safe here (not in event handler stack).
        esp_mqtt_client_stop(_client_handle);
        esp_mqtt_client_destroy(_client_handle);
        _flags.clear(MQTT_FLAG_ESP_MQTT_INIT);
        _client_handle = nullptr;
      }

      if (!_flags.value(MQTT_FLAG_ESP_MQTT_INIT) & _current_broker.isValid()) {
        // (Re)create client and register event handler.
        _client_handle = esp_mqtt_client_init(_current_broker.config());
        _flags.set(MQTT_FLAG_ESP_MQTT_INIT, (nullptr != _client_handle));
      }

      if (_flags.value(MQTT_FLAG_ESP_MQTT_INIT)) {
        switch (esp_mqtt_client_register_event(_client_handle, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, mqtt_event_handler, this)) {
          case ESP_OK:
            _flags.set(MQTT_FLAG_EVENT_REGISTERED);
            break;
          default:
            _set_fault("Unable to esp_mqtt_client_register_event().");
            break;
        }
      }
      state_entry_success = initialized();
      break;

    case MQTTCliState::CONNECTING:
      if ((nullptr != _radio) && _radio->linkUp() && _radio->hasIP() && _current_broker.isValid() && initialized()) {
        _connect_attempt_active = true;
        _mqtt_disconnected_latched = false;  // Clear stale disconnect latch before attempt.
        state_entry_success = (ESP_OK == esp_mqtt_client_start(_client_handle));
      }
      break;


    case MQTTCliState::CONNECTED:
      // Success resets backoff.
      _connect_attempt_active = false;
      _reconnect_backoff_ms = 5000;
      _mqtt_disconnected_latched = false;
      state_entry_success = true;
      break;

    case MQTTCliState::SUBSCRIBING:
      _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);
      state_entry_success = true;
      break;

    case MQTTCliState::DISCONNECTING:
      _connect_attempt_active = false;
      // NOTE: esp_mqtt_client_stop() must not be called from the event handler's stack frame.
      if (nullptr != _client_handle) {
        state_entry_success = (ESP_OK == esp_mqtt_client_stop(_client_handle));
      }
      else {
        state_entry_success = true;
      }
      _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);

      if (nullptr != _mb_waiting_msg) {
        delete _mb_waiting_msg;
        _mb_waiting_msg = nullptr;
      }
      break;


    case MQTTCliState::DISCONNECTED:
      state_entry_success = true;

      // Apply lockout ONLY if we were actively attempting a connection and ended up here.
      if (_connect_attempt_active) {
        _connect_attempt_active = false;

        const bool greedy = (autoconnect() && _current_broker.autoconnect() && _current_broker.isValid());
        if (greedy) {
          _fsm_lockout(_reconnect_backoff_ms);

          if (_reconnect_backoff_ms < _reconnect_backoff_ms_max) {
            uint32_t next = (_reconnect_backoff_ms << 1);
            _reconnect_backoff_ms = (next > _reconnect_backoff_ms_max) ? _reconnect_backoff_ms_max : next;
          }
        }
      }
      break;

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
      c3p_log(LOG_LEV_NOTICE, "MQTTClientESP32::_fsm_set_position",
        "MQTTClientESP32 State %s ---> %s",
        _FSM_STATES.enumStr(CURRENT_STATE), _FSM_STATES.enumStr(new_state)
      );
    }
    ret = 0;
  }

  return ret;
}


/**
* Put the driver into a FAULT state.
*/
void MQTTClientESP32::_set_fault(const char* msg) {
  if (_log_verbosity >= LOG_LEV_WARN) c3p_log(LOG_LEV_WARN, "MQTTClientESP32::_set_fault", "MQTTClientESP32 fault: %s", msg);
  _fsm_mark_current_state(MQTTCliState::FAULT);
}


/*******************************************************************************
* MQTT console support
*******************************************************************************/

void MQTTClientESP32::printDebug(StringBuilder* output) {
  StringBuilder tmp;
  StringBuilder prod_str("MQTTClientESP32");
  prod_str.concatf(" [%sinitialized]", (initialized() ? "" : "un"));
  StringBuilder::styleHeader2(&tmp, (const char*) prod_str.string());
  printFSM(&tmp);

  tmp.concatf("\t Client autoconnect: %c\n", (autoconnect() ? 'y' : 'n'));
  tmp.concatf("\t Broker autoconnect: %c\n", (_current_broker.autoconnect() ? 'y' : 'n'));
  tmp.concatf("\t Broker valid:       %c\n", (_current_broker.isValid() ? 'y' : 'n'));
  tmp.concatf("\t Subs complete:      %c\n", (_flags.value(MQTT_FLAG_SUBS_COMPLETE) ? 'y' : 'n'));
  tmp.concatf("\t Backoff:            %u ms (max %u)\n", _reconnect_backoff_ms, _reconnect_backoff_ms_max);

  if (nullptr != _client_subs) {
    _client_subs->printDebug(&tmp);
  }

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
        tmp.concatf("\t Radio IPv4:    %u.%u.%u.%u\n",
          (ip & 0xFF), ((ip >> 8) & 0xFF), ((ip >> 16) & 0xFF), ((ip >> 24) & 0xFF));
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



int MQTTClientESP32::console_handler_mqtt_client(StringBuilder* txt_ret, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);

  if (0 == StringBuilder::strcasecmp(cmd, "sub")) {
    bool print_usage = false;
    if (1 < args->count()) {
      char* topic = args->position_trimmed(1);
      uint8_t qos = ((uint8_t) args->position_as_int(2)) & 3;
      txt_ret->concatf("subscribe(%s, %s) returned %d.\n", topic, qos, subscribe(topic, qos));
    }
    else {
      if (connected()) {
        ret = _fsm_prepend_state(MQTTCliState::SUBSCRIBING);
      }
      else {
        txt_ret->concat("MQTTClientESP32 is not connected.\n");
      }
    }
    if (print_usage) {  txt_ret->concatf("Usage: %s <topic [qos]> ...\n", cmd); }
  }
  if (0 == StringBuilder::strcasecmp(cmd, "unsub")) {
    bool print_usage = false;
    if (1 < args->count()) {
      char* topic = args->position_trimmed(1);
      txt_ret->concatf("unsubscribe(%s) returned %d.\n", topic, unsubscribe(topic));
    }
    else {
      print_usage = true;
    }
    if (print_usage) {  txt_ret->concatf("Usage: %s <topic> ...\n", cmd); }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "broker")) {
    //m broker home mqtt://192.168.0.3 dgmj-mqtt dgmj-mqtt
    args->drop_position(0);
    ret = _current_broker.console_handler(txt_ret, args);  // Shunt into the BrokerDef console handler.
    if (4 == args->count()) {
      // If a full broker was defined, reconnect.
      _broker_changed_reinit_plan();
      txt_ret->concat("Attempting to connect to new broker...\n");
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "auto")) {
    if (2 == args->count()) {
      autoconnect(0 != args->position_as_int(1));
    }
    txt_ret->concatf("MQTTClientESP32 autoconnect: %c\n", (autoconnect() ? 'y' : 'n'));
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "poll")) {
    txt_ret->concatf("MQTTClientESP32 poll() returns %d\n", (int8_t) poll());
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "brokerauto")) {
    if (2 == args->count()) {
      _current_broker.autoconnect(0 != args->position_as_int(1));
    }
    txt_ret->concatf("Broker autoconnect: %c\n", (_current_broker.autoconnect() ? 'y' : 'n'));
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "backoff")) {
    txt_ret->concatf("Backoff: %u ms (max %u)\n", _reconnect_backoff_ms, _reconnect_backoff_ms_max);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "max_backoff")) {
    if (2 == args->count()) {
      uint32_t v = (uint32_t) args->position_as_int(1);
      // Clamp to something sane: at least 1000ms and not 0.
      _reconnect_backoff_ms_max = strict_max(v, (uint32_t) 1000);
      if (_reconnect_backoff_ms > _reconnect_backoff_ms_max) {
        _reconnect_backoff_ms = _reconnect_backoff_ms_max;
      }
    }
    txt_ret->concatf("Max backoff: %u ms\n", _reconnect_backoff_ms_max);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "con")) {
    // User intent: re-enable greedy connect.
    autoconnect(true);
    if (!connected()) {
      ret = _fsm_append_route(2, MQTTCliState::CONNECTING, MQTTCliState::CONNECTED);
    }
    else {
      txt_ret->concat("MQTTClientESP32 is already connected.\n");
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "discon")) {
    // User intent: override greedy connect (stay parked).
    autoconnect(false);
    if (connected()) {
      ret = _fsm_append_route(2, MQTTCliState::DISCONNECTING, MQTTCliState::DISCONNECTED);
    }
    else {
      // Ensure we are parked and stable.
      ret = _fsm_prepend_state(MQTTCliState::DISCONNECTED);
    }
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
