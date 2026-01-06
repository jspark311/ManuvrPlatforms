/*
File:   MQTT.cpp
Author: J. Ian Lindsay
Date:   2025.12.27

This file contains a reusable wrapper for the ESP32's MQTT features.

Refactor notes:
  - MQTT does NOT register for IP stack events.
  - Network readiness is queried from ESP32Radio via injected pointer.
  - MQTT connection truth is mailbox-driven from ESP-MQTT event handler.
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
* NOTE: This is running inside ESP-IDF's event loop task. It must not call into
*   FSM route APIs. It *may* set mailboxes.
*******************************************************************************/
void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
  static const char* const TAG = "mqtt_event_handler";
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
  esp_mqtt_client_handle_t client = event->client;
  MQTTClient* self = (MQTTClient*) handler_args;

  int msg_id;

  switch ((esp_mqtt_event_id_t) event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      if (nullptr != self) {
        self->_mb_set_mqtt_connected(true);
        self->_mb_set_mqtt_disconnected(false);
      }
      // Demo traffic as before.
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
      if (nullptr != self) {
        self->_mb_set_mqtt_connected(false);
        self->_mb_set_mqtt_disconnected(true);
      }
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


    case MQTT_EVENT_BEFORE_CONNECT:   // Unhandled. No special actions required.
      break;

    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}



/*******************************************************************************
* MQTTBrokerDef
*******************************************************************************/

MQTTBrokerDef::MQTTBrokerDef(const char* LABEL) {
  label(LABEL);
  memset(_uri,   0, sizeof(_uri));
  memset(_usr,   0, sizeof(_usr));
  memset(_pass,  0, sizeof(_pass));
  _autoconnect = true;
  _cli_conf = {
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
  };
}


bool MQTTBrokerDef::isValid() {
  const char* LABEL = label();   // These can never be nullptr. Only zero length.
  const char* URI   = uri();     // These can never be nullptr. Only zero length.
  if ((0 == strlen(LABEL)) || (0 == strlen(URI))) {  return false;  }

  // "Sensible URI": require mqtt:// or mqtts:// prefix.
  if (0 == strncmp(URI, "mqtt://", 7))  return true;
  if (0 == strncmp(URI, "mqtts://", 8)) return true;
  if (0 == strncmp(URI, "ws://", 5))  return true;
  if (0 == strncmp(URI, "wss://", 6)) return true;

  return false;
}

void MQTTBrokerDef::label(const char* LABEL) {
  const uint32_t FIELD_SIZE = sizeof(_label);
  memset(_label, 0, FIELD_SIZE);
  if (nullptr != LABEL) {
    memcpy(_label, LABEL, strict_min((uint32_t) strlen(LABEL), FIELD_SIZE-1));
  }
}


void MQTTBrokerDef::uri(const char* URI) {
  const uint32_t FIELD_SIZE = sizeof(_uri);
  memset(_uri, 0, FIELD_SIZE);
  if (nullptr != URI) {
    memcpy(_uri, URI, strict_min((uint32_t) strlen(URI), FIELD_SIZE-1));
  }
}


void MQTTBrokerDef::user(const char* USER) {
  const uint32_t FIELD_SIZE = sizeof(_usr);
  memset(_usr, 0, FIELD_SIZE);
  if (nullptr != USER) {
    memcpy(_usr, USER, strict_min((uint32_t) strlen(USER), FIELD_SIZE-1));
  }
}


void MQTTBrokerDef::passwd(const char* PASSWD) {
  const uint32_t FIELD_SIZE = sizeof(_pass);
  memset(_pass, 0, FIELD_SIZE);
  if (nullptr != PASSWD) {
    memcpy(_pass, PASSWD, strict_min((uint32_t) strlen(PASSWD), FIELD_SIZE-1));
  }
}

uint32_t MQTTBrokerDef::topicCount() {  return _subs.count();  }
int MQTTBrokerDef::clearTopics() {      _subs.clear();   return 0;   }


char* MQTTBrokerDef::topic(const int IDX) {
  if (_subs.count() > IDX) {
    return _subs.position_trimmed(IDX);
  }
  return nullptr;
}


/**
* Sanitizes and adds a topic string to the broker.
* - rejects nullptr, empty, or pure whitespace after trim
* - de-dupes against existing topics
* Returns >=0 on success: the index of the topic in the list.
*/
int MQTTBrokerDef::addTopic(const char* TOPIC_IN) {
  int ret = -1;
  if (nullptr != TOPIC_IN) {
    ret--;
    StringBuilder tmp(TOPIC_IN);
    tmp.trim();  // kill leading/trailing whitespace
    if (0 < tmp.length()) {
      ret--;
      const int EXISTING_COUNT = _subs.count();
      for (int i = 0; i < EXISTING_COUNT; i++) {
        char* t = _subs.position_trimmed(i);
        if (nullptr != t) {
          // MQTT topics are case-sensitive, so strcmp is correct.
          if (0 == strcmp(t, (const char*) tmp.string())) {
            return i;  // Already present: treat as success with no mutation.
          }
        }
      }
      // If we didn't bail, we didn't have the topic in the list. Add it.
      _subs.concat((const char*) tmp.string());
      ret = EXISTING_COUNT;
    }
  }
  return ret;
}




int MQTTBrokerDef::serialize(StringBuilder* out, const TCode FORMAT) {
  int ret = -1;
  if (!isValid()) {  return ret;  }

  switch (FORMAT) {
    case TCode::STR:
      printDebug(out);
      ret = 0;
      break;

    case TCode::BINARY:  // Unimplemented. Do not implement.
      break;

    #if defined(__BUILD_HAS_CBOR)
    case TCode::CBOR:
      {
        cbor::output_stringbuilder output(out);
        cbor::encoder encoder(output);

        const uint8_t KEY_COUNT = (topicCount() > 0) ? 6:5;   // Five baselines values to be encoded.

        // Encode this into IANA space as a vendor code.
        encoder.write_tag(C3P_CBOR_VENDOR_CODE | TcodeToInt(FORMAT));

        // {"MQTTBrokerDef": {"label":..,"uri":..,"user":..,"passwd":..,"autoconnect":..,"topics":[..]}}
        encoder.write_map(1);
        encoder.write_string("MQTTBrokerDef"); encoder.write_map(KEY_COUNT);
          encoder.write_string("label");       encoder.write_string(label());
          encoder.write_string("uri");         encoder.write_string(uri());
          encoder.write_string("user");        encoder.write_string(user());
          encoder.write_string("passwd");      encoder.write_string(passwd());
          encoder.write_string("autoconnect"); encoder.write_bool(autoconnect());
          if (topicCount() > 0) {
            encoder.write_string("topics");
            const int TCOUNT = _subs.count();
            encoder.write_array(TCOUNT);
            for (uint32_t t_id = 0; t_id < TCOUNT; t_id++) {
              char* t = _subs.position_trimmed(t_id);
              encoder.write_string((nullptr == t) ? "" : t);
            }
          }
        ret = 0;
      }
      break;
    #endif  // __BUILD_HAS_CBOR

    default:
      break;
  }
  return ret;
}



MQTTBrokerDef* MQTTBrokerDef::deserialize(StringBuilder* in) {
  MQTTBrokerDef* ret = nullptr;
  #if defined(__BUILD_HAS_CBOR)
  if ((nullptr == in) || (in->length() <= 0)) {
    return ret;
  }

  class MQTTBrokerDefListener : public cbor::listener {
   public:
    MQTTBrokerDefListener() {}
    ~MQTTBrokerDefListener() {}

    MQTTBrokerDef* result() { return _obj; }
    bool failed() const { return _failed; }

    // Integer callbacks (unused)
    void on_integer(int8_t) override {}
    void on_integer(int16_t) override {}
    void on_integer(int32_t) override {}
    void on_integer(int64_t) override {}
    void on_integer(uint8_t) override {}
    void on_integer(uint16_t) override {}
    void on_integer(uint32_t) override {}
    void on_integer(uint64_t) override {}
    void on_float32(float) override {}
    void on_double(double) override {}
    void on_bytes(uint8_t*, int) override {}

    void on_tag(unsigned int) override {
      // Tag is optional for our purposes. We accept and ignore.
    }

    void on_array(int size) override {
      if (_failed) return;
      if (_in_inner_map && !_expecting_key && (0 == strcmp(_last_key, "topics"))) {
        if (nullptr == _obj) {
          _obj = new MQTTBrokerDef();
        }
        _obj->clearTopics();
        _in_topics = true;
        _topics_remaining = size;
        _expecting_key = true;   // Value consumed by the array container.
      }
    }

    void on_map(int) override {
      if (_failed) return;
      if (!_in_outer_map) {
        _in_outer_map = true;
        _expecting_key = true;
        return;
      }
      if (_in_outer_map && !_in_inner_map) {
        // Outer map value for key "MQTTBrokerDef" is the inner map.
        if (!_expecting_key && (0 == strcmp(_last_key, "MQTTBrokerDef"))) {
          _in_inner_map = true;
          _expecting_key = true;
          if (nullptr == _obj) {
            _obj = new MQTTBrokerDef();
          }
          return;
        }
      }
      // Deeper nesting not expected; ignore.
    }

    void on_string(char* str) override {
      if (_failed || (nullptr == str)) return;

      if (_in_topics) {
        if (nullptr != _obj) {
          _obj->addTopic(str);
        }
        if (_topics_remaining > 0) {
          _topics_remaining--;
        }
        if (_topics_remaining <= 0) {
          _in_topics = false;
        }
        return;
      }

      if (_in_outer_map && !_in_inner_map) {
        // Expecting the single outer key "MQTTBrokerDef".
        if (_expecting_key) {
          _copy_key(str);
          _expecting_key = false;  // Next should be the inner map.
        }
        return;
      }

      if (_in_inner_map) {
        if (_expecting_key) {
          _copy_key(str);
          _expecting_key = false;
          return;
        }

        if (nullptr == _obj) {
          _obj = new MQTTBrokerDef();
        }

        if (0 == strcmp(_last_key, "label")) {
          _obj->label(str);
        }
        else if (0 == strcmp(_last_key, "uri")) {
          _obj->uri(str);
        }
        else if (0 == strcmp(_last_key, "user")) {
          _obj->user(str);
        }
        else if (0 == strcmp(_last_key, "passwd")) {
          _obj->passwd(str);
        }
        // "topics" is handled by on_array().

        _expecting_key = true;
      }
    }

    void on_bool(bool v) override {
      if (_failed) return;
      if (_in_inner_map && !_expecting_key && (0 == strcmp(_last_key, "autoconnect"))) {
        if (nullptr == _obj) {
          _obj = new MQTTBrokerDef();
        }
        _obj->autoconnect(v);
        _expecting_key = true;
      }
    }

    void on_null() override {}
    void on_undefined() override {}
    void on_special(unsigned int) override {}

    void on_error(const char*) override { _failed = true; }
    void on_extra_integer(uint64_t, int) override {}
    void on_extra_tag(uint64_t) override {}
    void on_extra_special(uint64_t) override {}

   private:
    MQTTBrokerDef* _obj = nullptr;
    bool _failed = false;
    bool _in_outer_map = false;
    bool _in_inner_map = false;
    bool _expecting_key = true;

    bool _in_topics = false;
    int _topics_remaining = 0;

    char _last_key[16] = {0};
    void _copy_key(const char* k) {
      memset(_last_key, 0, sizeof(_last_key));
      if (nullptr != k) {
        const uint32_t cpy = strict_min((uint32_t) strlen(k), (uint32_t) (sizeof(_last_key) - 1));
        memcpy(_last_key, k, cpy);
      }
    }
  };

  // Consume input as we decode (as requested).
  cbor::input_stringbuilder input(in, true, false);
  MQTTBrokerDefListener listener;
  cbor::decoder decoder(input, listener);
  decoder.run();

  if (listener.failed() || decoder.failed()) {
    MQTTBrokerDef* o = listener.result();
    if (nullptr != o) {
      delete o;
    }
    return nullptr;
  }
  return listener.result();
  #else
  return nullptr;
  #endif  // __BUILD_HAS_CBOR
}




void MQTTBrokerDef::printDebug(StringBuilder* output) {
  if (isValid()) {
    const char* USER   = user();
    const char* PASSWD = passwd();
    const char* safe_u_str = ((0 == strlen(USER)) ? "[UNSET]" : USER);
    const char* safe_p_str = ((0 == strlen(PASSWD)) ? "[UNSET]" : PASSWD);
    output->concatf("\t[%s]\t%s:%s@%s\n", label(), safe_u_str, safe_p_str, uri());
  }
  else {
    output->concat("Broker is invalid.\n");
  }
}



void MQTTBrokerDef::printTopicList(StringBuilder* output) {
  const int TCOUNT = _subs.count();
  if (TCOUNT > 0) {
    StringBuilder tmp("\tTopics:\n");
    for (int i = 0; i < TCOUNT; i++) {
      char* t = _subs.position_trimmed(i);
      if ((nullptr != t) && (0 < strlen(t))) {
        tmp.concatf("\t\t[%d] %s\n", i, t);
      }
    }
    tmp.string();
    output->concatHandoff(&tmp);
  }
}



/*******************************************************************************
* MQTTClient
*******************************************************************************/

/*
* Constructor
*/
MQTTClient::MQTTClient() :
  StateMachine<MQTTCliState>("MQTTClient-FSM", &_FSM_STATES, MQTTCliState::UNINIT, 8)
{
  MQTT_CLIENT_INSTANCE = this;
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

  _sub_cursor         = 0;
  _sub_pending_msg_id = -1;
  _mb_suback_msg_id   = -1;
}


/*
* Destructor
*/
MQTTClient::~MQTTClient() {}


int8_t MQTTClient::init() {
  int8_t ret = 0;
  // Do not imply "connect now". We idle in DISCONNECTED and let policy drive.
  _fsm_set_route(2, MQTTCliState::INIT, MQTTCliState::DISCONNECTED);
  return ret;
}


/*******************************************************************************
* Mailbox helpers (event-loop context)
*******************************************************************************/
void MQTTClient::_mb_set_mqtt_connected(const bool v) { _mb_mqtt_connected = v; }
void MQTTClient::_mb_set_mqtt_disconnected(const bool v) { _mb_mqtt_disconnected = v; }


/**
* Called when any broker property changes.
* Clears init flags and plans a regression to INIT (disconnecting first if needed).
*/
void MQTTClient::_broker_changed_reinit_plan() {
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



/*******************************************************************************
* MQTT console support
*******************************************************************************/

void MQTTClient::printDebug(StringBuilder* output) {
  StringBuilder tmp;
  StringBuilder prod_str("MQTTClient");
  prod_str.concatf(" [%sinitialized]", (initialized() ? "" : "un"));
  StringBuilder::styleHeader2(&tmp, (const char*) prod_str.string());
  printFSM(&tmp);

  tmp.concatf("\t Client autoconnect: %c\n", (autoconnect() ? 'y' : 'n'));
  tmp.concatf("\t Broker autoconnect: %c\n", (_current_broker.autoconnect() ? 'y' : 'n'));
  tmp.concatf("\t Broker valid:       %c\n", (_current_broker.isValid() ? 'y' : 'n'));
  tmp.concatf("\t Subs complete:      %c\n", (_flags.value(MQTT_FLAG_SUBS_COMPLETE) ? 'y' : 'n'));
  tmp.concatf("\t Subs cursor:        %d\n", _sub_cursor);
  tmp.concatf("\t Backoff:            %u ms (max %u)\n", _reconnect_backoff_ms, _reconnect_backoff_ms_max);

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



int MQTTClient::console_handler_mqtt_client(StringBuilder* txt_ret, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);

  if (0 == StringBuilder::strcasecmp(cmd, "topic")) {
    bool print_usage = false;
    if (1 < args->count()) {
      char* subcmd = args->position_trimmed(1);
      if (0 == StringBuilder::strcasecmp(subcmd, "add")) {
        if (2 <= args->count()) {
          args->drop_position(0);  // Drop the string "topic".
          args->drop_position(0);  // Drop the string "add".
          while (0 < args->count()) {  // While we have any left over...
            char* t = args->position_trimmed(0);
            int idx = _current_broker.addTopic(t);
            if (idx >= 0) {
              txt_ret->concatf("Topic [%s] added at index %d.\n", t, idx);
            }
            else {
              txt_ret->concatf("Topic [%s] rejected.\n", t);
            }
            args->drop_position(0);  // Drop the just-handled topic argument.
          }
        }
        else {
          txt_ret->concat("Usage: topic add <topic> [topic] [topic] ...\n");
        }
      }
      else if (0 == StringBuilder::strcasecmp(subcmd, "list")) {
        _current_broker.printTopicList(txt_ret);
      }
      else if (0 == StringBuilder::strcasecmp(subcmd, "clear")) {
        _current_broker.clearTopics();
        txt_ret->concat("Topics cleared.\n");
      }
      else {
        print_usage = true;
      }
    }
    else {
      print_usage = true;
    }
    if (print_usage) {  txt_ret->concat("Usage: topic [add|list|clear] ...\n"); }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "broker")) {
    if (5 == args->count()) {
      //m broker home mqtt://192.168.0.3 dgmj-mqtt dgmj-mqtt
      char* lab  = args->position_trimmed(1);
      char* uri  = args->position_trimmed(2);
      char* user = args->position_trimmed(3);
      char* pass = args->position_trimmed(4);

      _current_broker.label(lab);
      _current_broker.uri(uri);
      _current_broker.user(user);
      _current_broker.passwd(pass);

      _broker_changed_reinit_plan();
      txt_ret->concat("Broker updated.\n");
      _current_broker.printDebug(txt_ret);
    }
    else if (1 == args->count()) {
      _current_broker.printDebug(txt_ret);
    }
    else {
      txt_ret->concatf("Usage: %s [<label> <uri> <user> <pass>].\n", cmd);
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "auto")) {
    if (2 == args->count()) {
      autoconnect(0 != args->position_as_int(1));
    }
    txt_ret->concatf("MQTTClient autoconnect: %c\n", (autoconnect() ? 'y' : 'n'));
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
      txt_ret->concat("MQTTClient is already connected.\n");
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
  else if (0 == StringBuilder::strcasecmp(cmd, "pack")) {
    StringBuilder cbor_str;
    _current_broker.serialize(&cbor_str, TCode::CBOR);
    cbor_str.printDebug(txt_ret);
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
* @return PollResult
*/
FAST_FUNC PollResult MQTTClient::poll() {
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
      _sub_cursor++;
    }
    _mb_suback_msg_id = -1;
  }

  return ((0 == _fsm_poll()) ? PollResult::NO_ACTION : PollResult::ACTION);
}


/**
* Called in idle time by the firmware to prod the driver's state machine forward.
*
* @return  1 on state shift
*          0 on no action
*         -1 on error
*/
FAST_FUNC int8_t MQTTClient::_fsm_poll() {
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
      }
      fsm_advance = !_fsm_is_stable();
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
FAST_FUNC int8_t MQTTClient::_fsm_set_position(MQTTCliState new_state) {
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

    case MQTTCliState::DISCONNECTING:
      _connect_attempt_active = false;
      // NOTE: esp_mqtt_client_stop() must not be called from the event handler's stack frame.
      if (nullptr != _client_handle) {
        state_entry_success = (ESP_OK == esp_mqtt_client_stop(_client_handle));
      }
      else {
        state_entry_success = true;
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
      c3p_log(LOG_LEV_NOTICE, "MQTTClient::_fsm_set_position",
        "MQTTClient State %s ---> %s",
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
void MQTTClient::_set_fault(const char* msg) {
  if (_log_verbosity >= LOG_LEV_WARN) c3p_log(LOG_LEV_WARN, "MQTTClient::_set_fault", "MQTTClient fault: %s", msg);
  _fsm_mark_current_state(MQTTCliState::FAULT);
}
