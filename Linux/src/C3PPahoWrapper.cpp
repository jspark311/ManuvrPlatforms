/*
File:   MQTTPaho.cpp
Author: OpenAI
Date:   2026.04.18

Linux/Paho-backed implementation of the C3P MQTT wrapper.

Notes:
- MQTT v5 only.
- C3P owns reconnect policy. Paho automatic reconnect is disabled.
- Callback threads only enqueue mailboxes / queue nodes. FSM and poll() do real work.
- This first pass supports mqtt://, tcp://, and ws://.
- TODO: Add TLS / secure websockets support when linking against paho-mqtt3as.
- TODO: Fold topic ownership into MQTTMessage / MQTTSub destructors in shared code.
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "C3PPaho.h"   // Replace with your actual Linux platform header path.

#if defined(CONFIG_C3P_WITH_PAHO)


/*******************************************************************************
* Static FSM metadata
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
  { MQTTCliState::INVALID,       "INVALID", (ENUM_WRAPPER_FLAG_CATCHALL)}
};
const EnumDefList<MQTTCliState> _FSM_STATES(
  &_STATE_LIST[0],
  (sizeof(_STATE_LIST) / sizeof(_STATE_LIST[0])),
  "MQTTCliState"
);


/*******************************************************************************
* MQTTBrokerDefPaho
*******************************************************************************/

MQTTBrokerDefPaho::MQTTBrokerDefPaho(const char* LABEL) : MQTTBrokerDef(LABEL) {
  memset(_client_id, 0, sizeof(_client_id));
}


MQTTBrokerDefPaho::MQTTBrokerDefPaho(const MQTTBrokerDefPaho& SRC) : MQTTBrokerDef(SRC) {
  memcpy(_client_id, SRC._client_id, sizeof(_client_id));
}


void MQTTBrokerDefPaho::clientId(const char* ID) {
  const uint32_t FIELD_SIZE = sizeof(_client_id);
  memset(_client_id, 0, FIELD_SIZE);
  if (nullptr != ID) {
    memcpy(_client_id, ID, strict_min((uint32_t) strlen(ID), FIELD_SIZE-1));
  }
}


bool MQTTBrokerDefPaho::set(MQTTBrokerDef* n_broker) {
  // Copies the shared broker fields only. client_id is left unchanged.
  return MQTTBrokerDef::set(n_broker);
}


bool MQTTBrokerDefPaho::set(MQTTBrokerDefPaho* n_broker) {
  if (nullptr != n_broker) {
    if (MQTTBrokerDef::set((MQTTBrokerDef*) n_broker)) {
      clientId(n_broker->clientId());
      return true;
    }
  }
  return false;
}


/*******************************************************************************
* MQTTClientPaho
*******************************************************************************/

MQTTClientPaho::MQTTClientPaho() :
  StateMachine<MQTTCliState>("MQTTClientPaho-FSM", &_FSM_STATES, MQTTCliState::UNINIT, 8)
{
  memset(&_mutex, 0, sizeof(_mutex));
  memset(_runtime_client_id, 0, sizeof(_runtime_client_id));

  _flags.set(MQTT_FLAG_AUTOCONNECT, true);
  _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);

  _reconnect_backoff_ms     = 5000;
  _reconnect_backoff_ms_max = 60000;

  _mqtt_connected_latched    = false;
  _mqtt_disconnected_latched = false;
  _connect_attempt_active    = false;
  _connect_api_rc_last       = MQTTASYNC_SUCCESS;
}


MQTTClientPaho::~MQTTClientPaho() {
  if (nullptr != _client_handle) {
    MQTTAsync_destroy(&_client_handle);
    _client_handle = nullptr;
  }

  MQTTMessage* msg = nullptr;
  while (nullptr != (msg = _dequeue_msg())) {
    _free_mqtt_message(msg);
  }

  MQTTOpType op_type;
  int32_t op_token;
  while (_dequeue_op(&op_type, &op_token)) {}

  _purge_subs();

  if (_flags.value(MQTT_FLAG_MUTEX_INIT)) {
    pthread_mutex_destroy(&_mutex);
    _flags.clear(MQTT_FLAG_MUTEX_INIT);
  }
}


void MQTTClientPaho::_mutex_lock() {
  if (_flags.value(MQTT_FLAG_MUTEX_INIT)) {
    pthread_mutex_lock(&_mutex);
  }
}


void MQTTClientPaho::_mutex_unlock() {
  if (_flags.value(MQTT_FLAG_MUTEX_INIT)) {
    pthread_mutex_unlock(&_mutex);
  }
}


int8_t MQTTClientPaho::init() {
  int8_t ret = 0;

  if (!_flags.value(MQTT_FLAG_MUTEX_INIT)) {
    if (0 == pthread_mutex_init(&_mutex, nullptr)) {
      _flags.set(MQTT_FLAG_MUTEX_INIT);
    }
    else {
      return -1;
    }
  }

  _fsm_set_route(2, MQTTCliState::INIT, MQTTCliState::DISCONNECTED);
  return ret;
}


bool MQTTClientPaho::setBroker(MQTTBrokerDef* n_broker) {
  const bool ret = _current_broker.set(n_broker);
  if (ret) {
    _broker_changed_reinit_plan();
  }
  return ret;
}


bool MQTTClientPaho::setBroker(MQTTBrokerDefPaho* n_broker) {
  const bool ret = _current_broker.set(n_broker);
  if (ret) {
    _broker_changed_reinit_plan();
  }
  return ret;
}


bool MQTTClientPaho::_uri_supported_in_build(const char* URI) {
  if (nullptr == URI) {  return false;  }

  if (0 == strncmp(URI, "mqtt://", 7)) {  return true;  }
  if (0 == strncmp(URI, "tcp://",  6)) {  return true;  }
  if (0 == strncmp(URI, "ws://",   5)) {  return true;  }

  // TODO: Support mqtts:// / ssl:// / wss:// once linked against paho-mqtt3as
  //       and a TLS policy is defined in broker/client config.
  return false;
}


const char* MQTTClientPaho::_effective_client_id() {
  const char* CID = _current_broker.clientId();
  if ((nullptr != CID) && (0 < strlen(CID))) {
    return CID;
  }

  memset(_runtime_client_id, 0, sizeof(_runtime_client_id));
  StringBuilder tmp;
  const char* LABEL = _current_broker.label();
  tmp.concat(((nullptr != LABEL) && (0 < strlen(LABEL))) ? LABEL : "c3p-linux");
  tmp.concatf("-%u", (unsigned int) getpid());

  if (0 < tmp.length()) {
    const uint32_t COPY_LEN = strict_min((uint32_t) tmp.length(), (uint32_t) (sizeof(_runtime_client_id) - 1));
    memcpy(_runtime_client_id, tmp.string(), COPY_LEN);
  }
  return _runtime_client_id;
}


void MQTTClientPaho::_free_mqtt_message(MQTTMessage* msg) {
  if (nullptr != msg) {
    if (nullptr != msg->topic) {
      free(msg->topic);
      msg->topic = nullptr;
    }
    delete msg;
  }
}


int MQTTClientPaho::_enqueue_msg(MQTTMessage* msg) {
  if (nullptr == msg) {  return -1;  }

  MQTTMsgNode* node = new MQTTMsgNode(msg);
  if (nullptr == node) {  return -1;  }

  _mutex_lock();
  if (nullptr != _rx_queue_tail) {
    _rx_queue_tail->next = node;
    _rx_queue_tail = node;
  }
  else {
    _rx_queue_head = node;
    _rx_queue_tail = node;
  }
  _mutex_unlock();
  return 0;
}


MQTTMessage* MQTTClientPaho::_dequeue_msg() {
  MQTTMessage* ret = nullptr;
  MQTTMsgNode* node = nullptr;

  _mutex_lock();
  node = _rx_queue_head;
  if (nullptr != node) {
    _rx_queue_head = node->next;
    if (nullptr == _rx_queue_head) {
      _rx_queue_tail = nullptr;
    }
  }
  _mutex_unlock();

  if (nullptr != node) {
    ret = node->msg;
    delete node;
  }
  return ret;
}


int MQTTClientPaho::_enqueue_op(const MQTTOpType TYPE, const int32_t TOKEN) {
  MQTTOpNode* node = new MQTTOpNode(TYPE, TOKEN);
  if (nullptr == node) {  return -1;  }

  _mutex_lock();
  if (nullptr != _op_queue_tail) {
    _op_queue_tail->next = node;
    _op_queue_tail = node;
  }
  else {
    _op_queue_head = node;
    _op_queue_tail = node;
  }
  _mutex_unlock();
  return 0;
}


bool MQTTClientPaho::_dequeue_op(MQTTOpType* TYPE, int32_t* TOKEN) {
  bool ret = false;
  MQTTOpNode* node = nullptr;

  _mutex_lock();
  node = _op_queue_head;
  if (nullptr != node) {
    _op_queue_head = node->next;
    if (nullptr == _op_queue_head) {
      _op_queue_tail = nullptr;
    }
  }
  _mutex_unlock();

  if (nullptr != node) {
    if (nullptr != TYPE)   { *TYPE  = node->type;   }
    if (nullptr != TOKEN)  { *TOKEN = node->token;  }
    delete node;
    ret = true;
  }
  return ret;
}


int MQTTClientPaho::_add_sub(MQTTSub* n_sub) {
  int ret = 0;
  if (nullptr != n_sub) {
    if (nullptr != _client_subs) {
      MQTTSub* current = _client_subs;
      ret++;
      while (nullptr != current->next) {
        current = current->next;
        ret++;
      }
      current->next = n_sub;
    }
    else {
      _client_subs = n_sub;
    }
  }
  else {
    ret = -1;
  }
  return ret;
}


int MQTTClientPaho::_drop_sub(MQTTSub* n_sub) {
  int ret = -1;
  if (nullptr != n_sub) {
    MQTTSub* current = _client_subs;
    MQTTSub* prior   = nullptr;
    int idx = 0;

    while (nullptr != current) {
      if (current == n_sub) {
        if (nullptr != prior) {  prior->next = current->next;  }
        else                  {  _client_subs = current->next;  }
        current->next = nullptr;
        if (nullptr != current->topic) {
          free((void*) current->topic);
          current->topic = nullptr;
        }
        delete current;
        return idx;
      }
      prior = current;
      current = current->next;
      idx++;
    }
  }
  return ret;
}


void MQTTClientPaho::_purge_subs() {
  MQTTSub* current = _client_subs;
  _client_subs = nullptr;
  while (nullptr != current) {
    MQTTSub* nxt = current->next;
    current->next = nullptr;
    if (nullptr != current->topic) {
      free((void*) current->topic);
      current->topic = nullptr;
    }
    delete current;
    current = nxt;
  }
}


void MQTTClientPaho::_normalize_subs_for_disconnect() {
  MQTTSub* current = _client_subs;
  MQTTSub* prior   = nullptr;

  while (nullptr != current) {
    MQTTSub* nxt = current->next;
    current->pending_msg_id = -1;

    switch (current->sub_state) {
      case MQTTSubState::INFLIGHT_DOWN:
        if (nullptr != prior) {  prior->next = nxt;  }
        else                  {  _client_subs = nxt;  }
        current->next = nullptr;
        if (nullptr != current->topic) {
          free((void*) current->topic);
          current->topic = nullptr;
        }
        delete current;
        break;

      case MQTTSubState::NEG:
      case MQTTSubState::INFLIGHT_UP:
      case MQTTSubState::POS:
      default:
        current->sub_state = MQTTSubState::NEG;
        prior = current;
        break;
    }

    current = nxt;
  }

  _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);
}


void MQTTClientPaho::_apply_token_result(const int32_t TOKEN, const bool SUCCESS) {
  if (TOKEN < 0) {  return;  }

  _mutex_lock();
  MQTTSub* current = ((nullptr != _client_subs) ? _client_subs->havingAckID(TOKEN) : nullptr);

  if (nullptr != current) {
    switch (current->sub_state) {
      case MQTTSubState::INFLIGHT_UP:
        current->pending_msg_id = -1;
        current->sub_state = (SUCCESS ? MQTTSubState::POS : MQTTSubState::NEG);
        break;

      case MQTTSubState::INFLIGHT_DOWN:
        current->pending_msg_id = -1;
        if (SUCCESS) {
          _drop_sub(current);
        }
        else {
          current->sub_state = MQTTSubState::POS;
        }
        break;

      default:
        current->pending_msg_id = -1;
        break;
    }
  }
  _mutex_unlock();
}


MQTTTopicCallback MQTTClientPaho::_topic_callback_for(const char* TOPIC) {
  MQTTTopicCallback ret = nullptr;
  _mutex_lock();
  MQTTSub* sub = ((nullptr != _client_subs) ? _client_subs->havingTopic(TOPIC) : nullptr);
  if (nullptr != sub) {
    ret = sub->rx_callback;
  }
  _mutex_unlock();
  return ret;
}


void MQTTClientPaho::_mb_set_mqtt_connected(const bool v) {
  _mutex_lock();
  _mb_mqtt_connected = v;
  _mutex_unlock();
}


void MQTTClientPaho::_mb_set_mqtt_disconnected(const bool v) {
  _mutex_lock();
  _mb_mqtt_disconnected = v;
  _mutex_unlock();
}


void MQTTClientPaho::_broker_changed_reinit_plan() {
  _flags.clear(MQTT_FLAG_PAHO_INIT | MQTT_FLAG_CALLBACKS_SET);
  _connect_attempt_active    = false;
  _mqtt_connected_latched    = false;
  _mqtt_disconnected_latched = false;
  _connect_api_rc_last       = MQTTASYNC_SUCCESS;

  if (connected()) {
    _fsm_set_route(4,
      MQTTCliState::DISCONNECTING,
      MQTTCliState::DISCONNECTED,
      MQTTCliState::INIT,
      MQTTCliState::DISCONNECTED
    );
  }
  else {
    _fsm_prepend_state(MQTTCliState::INIT);
  }
}


/*******************************************************************************
* Paho callbacks
*******************************************************************************/

void MQTTClientPaho::_paho_connection_lost(void* context, char*) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(false);
    self->_mb_set_mqtt_disconnected(true);
  }
}


int MQTTClientPaho::_paho_message_arrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  MQTTMessage* n_msg = nullptr;

  if ((nullptr != self) && (nullptr != message)) {
    n_msg = new MQTTMessage();
    if (nullptr != n_msg) {
      const int TOPIC_LEN = ((0 < topicLen) ? topicLen : ((nullptr != topicName) ? (int) strlen(topicName) : 0));
      if (0 < TOPIC_LEN) {
        n_msg->topic = (char*) malloc(TOPIC_LEN + 1);
        if (nullptr != n_msg->topic) {
          memset(n_msg->topic, 0, TOPIC_LEN + 1);
          memcpy(n_msg->topic, topicName, TOPIC_LEN);
        }
      }

      n_msg->msg_id = message->msgid;
      n_msg->qos    = message->qos;
      n_msg->retain = (0 != message->retained);
      n_msg->dup    = (0 != message->dup);

      if ((nullptr != message->payload) && (0 < message->payloadlen)) {
        n_msg->data.concat((uint8_t*) message->payload, message->payloadlen);
      }

      if (0 != self->_enqueue_msg(n_msg)) {
        self->_free_mqtt_message(n_msg);
      }
    }
  }

  MQTTAsync_freeMessage(&message);
  MQTTAsync_free(topicName);
  return 1;
}


void MQTTClientPaho::_paho_delivery_complete(void*, MQTTAsync_token) {
  // Currently ignored. publish() reports acceptance into the client library,
  // not broker delivery completion.
}


void MQTTClientPaho::_paho_connected(void* context, char*) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(true);
    self->_mb_set_mqtt_disconnected(false);
  }
}


void MQTTClientPaho::_paho_disconnected(void* context, MQTTProperties*, enum MQTTReasonCodes) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(false);
    self->_mb_set_mqtt_disconnected(true);

    // TODO: Examine reasonCode and disable greedy reconnect for
    //       permanent authentication / protocol failures.
  }
}


void MQTTClientPaho::_paho_connect_success5(void* context, MQTTAsync_successData5*) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(true);
    self->_mb_set_mqtt_disconnected(false);
  }
}


void MQTTClientPaho::_paho_connect_failure5(void* context, MQTTAsync_failureData5*) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(false);
    self->_mb_set_mqtt_disconnected(true);
  }
}


void MQTTClientPaho::_paho_disconnect_success5(void* context, MQTTAsync_successData5*) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(false);
    self->_mb_set_mqtt_disconnected(true);
  }
}


void MQTTClientPaho::_paho_disconnect_failure5(void* context, MQTTAsync_failureData5*) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_mb_set_mqtt_connected(false);
    self->_mb_set_mqtt_disconnected(true);
  }
}


void MQTTClientPaho::_paho_subop_success5(void* context, MQTTAsync_successData5* response) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if ((nullptr != self) && (nullptr != response)) {
    self->_enqueue_op(MQTTOpType::TOKEN_SUCCESS, response->token);
  }
}


void MQTTClientPaho::_paho_subop_failure5(void* context, MQTTAsync_failureData5* response) {
  MQTTClientPaho* self = (MQTTClientPaho*) context;
  if (nullptr != self) {
    self->_enqueue_op(MQTTOpType::TOKEN_FAILURE, ((nullptr != response) ? response->token : -1));
  }
}


/*******************************************************************************
* Public API
*******************************************************************************/

bool MQTTClientPaho::connected() {
  switch (currentState()) {
    case MQTTCliState::CONNECTED:
    case MQTTCliState::SUBSCRIBING:
      return ((nullptr != _client_handle) ? (0 != MQTTAsync_isConnected(_client_handle)) : false);
    default:
      break;
  }
  return false;
}


int MQTTClientPaho::publish(MQTTMessage* msg) {
  if ((nullptr == msg) || (nullptr == msg->topic) || !connected()) {
    return -1;
  }

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  const void* PAYLOAD = ((0 < msg->data.length()) ? msg->data.string() : nullptr);

  const int rc = MQTTAsync_send(
    _client_handle,
    msg->topic,
    msg->data.length(),
    PAYLOAD,
    msg->qos,
    (msg->retain ? 1 : 0),
    &opts
  );

  if (MQTTASYNC_SUCCESS == rc) {
    msg->msg_id = opts.token;
    return 0;
  }
  return -1;
}


int MQTTClientPaho::subscribe(const char* TOPIC_STR, const uint8_t QOS, MQTTTopicCallback CB) {
  int ret = -1;
  if ((nullptr == TOPIC_STR) || (0 == strlen(TOPIC_STR))) {
    return ret;
  }

  _mutex_lock();
  MQTTSub* current = ((nullptr == _client_subs) ? nullptr : _client_subs->havingTopic(TOPIC_STR));

  if (nullptr == current) {
    const uint32_t TOPIC_LEN = strlen(TOPIC_STR);
    char* topic_copy = (char*) malloc(TOPIC_LEN + 1);
    if (nullptr != topic_copy) {
      memset(topic_copy, 0, TOPIC_LEN + 1);
      memcpy(topic_copy, TOPIC_STR, TOPIC_LEN);
      current = new MQTTSub(topic_copy, QOS, CB);
      if (nullptr != current) {
        current->sub_state = MQTTSubState::NEG;
        current->pending_msg_id = -1;
        ret = _add_sub(current);
      }
      else {
        free(topic_copy);
      }
    }
  }
  else {
    current->rx_callback = CB;
    current->qos         = QOS;
    if (MQTTSubState::INFLIGHT_DOWN == current->sub_state) {
      current->sub_state = MQTTSubState::NEG;
    }
    ret = 0;
  }
  _mutex_unlock();

  if ((0 <= ret) && (MQTTCliState::CONNECTED == currentState()) && _fsm_is_stable()) {
    ret = _fsm_prepend_state(MQTTCliState::SUBSCRIBING);
  }

  return ret;
}


int MQTTClientPaho::unsubscribe(const char* TOPIC_STR) {
  int ret = -1;
  if ((nullptr == TOPIC_STR) || (0 == strlen(TOPIC_STR))) {
    return ret;
  }

  _mutex_lock();
  MQTTSub* current = ((nullptr == _client_subs) ? nullptr : _client_subs->havingTopic(TOPIC_STR));
  if (nullptr == current) {
    _mutex_unlock();
    return -1;
  }

  if (!connected()) {
    ret = _drop_sub(current);
    _mutex_unlock();
    return ret;
  }

  current->sub_state = MQTTSubState::INFLIGHT_DOWN;
  current->pending_msg_id = -1;
  _mutex_unlock();

  if (_fsm_is_stable() && (MQTTCliState::CONNECTED == currentState())) {
    ret = _fsm_prepend_state(MQTTCliState::SUBSCRIBING);
  }
  else {
    ret = 0;
  }

  return ret;
}


FAST_FUNC PollResult MQTTClientPaho::poll() {
  bool did_work = false;

  _mutex_lock();
  if (_mb_mqtt_connected) {
    _mqtt_connected_latched = true;
    _mqtt_disconnected_latched = false;
    _mb_mqtt_connected = false;
    did_work = true;
  }
  if (_mb_mqtt_disconnected) {
    _mqtt_connected_latched = false;
    _mqtt_disconnected_latched = true;
    _mb_mqtt_disconnected = false;
    did_work = true;
  }
  _mutex_unlock();

  MQTTOpType op_type;
  int32_t op_token = -1;
  while (_dequeue_op(&op_type, &op_token)) {
    _apply_token_result(op_token, (MQTTOpType::TOKEN_SUCCESS == op_type));
    did_work = true;
  }

  if (0 != _fsm_poll()) {
    did_work = true;
  }

  return (did_work ? PollResult::ACTION : PollResult::NO_ACTION);
}


/*******************************************************************************
* FSM
*******************************************************************************/

FAST_FUNC int8_t MQTTClientPaho::_fsm_poll() {
  int8_t ret = 0;
  bool fsm_advance = false;

  switch (currentState()) {
    case MQTTCliState::UNINIT:
      fsm_advance = _fsm_is_next_pos(MQTTCliState::INIT);
      break;

    case MQTTCliState::INIT:
      fsm_advance = initialized();
      break;

    case MQTTCliState::CONNECTING:
      if (MQTTASYNC_SUCCESS != _connect_api_rc_last) {
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
        if (!_mqtt_connected_latched || _mqtt_disconnected_latched || !connected()) {
          fsm_advance = (0 == _fsm_append_state(MQTTCliState::DISCONNECTED));
        }
        else {
          StringBuilder* subs = _current_broker.subs();
          for (int i = 0; i < subs->count(); i++) {
            char* t = subs->position_trimmed(i);
            if (nullptr != t) {
              subscribe(t, 0, nullptr);
            }
          }

          MQTTMessage* msg = _dequeue_msg();
          if (nullptr != msg) {
            const char* reply_topic = nullptr;

            MQTTTopicCallback topic_cb = _topic_callback_for(msg->topic);
            if (nullptr != topic_cb) {
              reply_topic = topic_cb(msg);
            }
            if (nullptr == reply_topic) {
              reply_topic = _handle_mqtt_message(msg);
            }

            if (nullptr != reply_topic) {
              char* inbound_topic = msg->topic;
              msg->topic = (char*) reply_topic;
              publish(msg);
              msg->topic = inbound_topic;
            }

            _free_mqtt_message(msg);
            ret = 1;
          }
        }
      }

      if (!fsm_advance) {
        fsm_advance = !_fsm_is_stable();
      }
      break;

    case MQTTCliState::SUBSCRIBING:
      {
        fsm_advance = true;
        _mutex_lock();
        MQTTSub* current = _client_subs;
        while (fsm_advance && (nullptr != current)) {
          switch (current->sub_state) {
            case MQTTSubState::NEG:
              {
                MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
                opts.onSuccess5 = _paho_subop_success5;
                opts.onFailure5 = _paho_subop_failure5;
                opts.context    = this;
                opts.token      = 0;

                const int rc = MQTTAsync_subscribe(_client_handle, current->topic, current->qos, &opts);
                if (MQTTASYNC_SUCCESS == rc) {
                  current->pending_msg_id = opts.token;
                  current->sub_state = MQTTSubState::INFLIGHT_UP;
                }
                else {
                  current->pending_msg_id = -1;
                  current->sub_state = MQTTSubState::NEG;
                  _mqtt_connected_latched = false;
                  _mqtt_disconnected_latched = true;
                }
                fsm_advance = false;
              }
              break;

            case MQTTSubState::INFLIGHT_UP:
            case MQTTSubState::INFLIGHT_DOWN:
              fsm_advance = false;
              break;

            case MQTTSubState::POS:
              break;

            default:
              break;
          }

          if (fsm_advance && (MQTTSubState::INFLIGHT_DOWN == current->sub_state)) {
            MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
            opts.onSuccess5 = _paho_subop_success5;
            opts.onFailure5 = _paho_subop_failure5;
            opts.context    = this;
            opts.token      = 0;

            const int rc = MQTTAsync_unsubscribe(_client_handle, current->topic, &opts);
            if (MQTTASYNC_SUCCESS == rc) {
              current->pending_msg_id = opts.token;
            }
            else {
              current->pending_msg_id = -1;
              current->sub_state = MQTTSubState::POS;
              _mqtt_connected_latched = false;
              _mqtt_disconnected_latched = true;
            }
            fsm_advance = false;
          }

          current = current->next;
        }
        _mutex_unlock();
        _flags.set(MQTT_FLAG_SUBS_COMPLETE, fsm_advance);
      }
      break;

    case MQTTCliState::DISCONNECTING:
      fsm_advance = !_fsm_is_stable();
      break;

    case MQTTCliState::DISCONNECTED:
      if (_fsm_is_stable()) {
        const bool greedy = (autoconnect() && _current_broker.autoconnect() && _current_broker.isValid());
        if (greedy && _uri_supported_in_build(_current_broker.uri())) {
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

  if (fsm_advance && (-1 != ret)) {
    ret = (0 == _fsm_advance()) ? 1 : 0;
  }
  return ret;
}


FAST_FUNC int8_t MQTTClientPaho::_fsm_set_position(MQTTCliState new_state) {
  int8_t ret = -1;
  const MQTTCliState CURRENT_STATE = currentState();
  if (_fsm_is_waiting()) return ret;

  bool state_entry_success = false;

  switch (new_state) {
    case MQTTCliState::UNINIT:
      _set_fault("Tried to _fsm_set_position(UNINIT)");
      break;

    case MQTTCliState::INIT:
      if (nullptr != _client_handle) {
        MQTTAsync_destroy(&_client_handle);
        _client_handle = nullptr;
        _flags.clear(MQTT_FLAG_PAHO_INIT | MQTT_FLAG_CALLBACKS_SET);
      }

      if (!_current_broker.isValid()) {
        state_entry_success = false;
        break;
      }

      if (!_uri_supported_in_build(_current_broker.uri())) {
        _set_fault("Broker URI not supported by this non-TLS Paho build.");
        break;
      }

      {
        MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
        create_opts.MQTTVersion = MQTTVERSION_5;
        create_opts.sendWhileDisconnected = 0;
        create_opts.maxBufferedMessages   = 100;

        const int rc = MQTTAsync_createWithOptions(
          &_client_handle,
          _current_broker.uri(),
          _effective_client_id(),
          MQTTCLIENT_PERSISTENCE_NONE,
          nullptr,
          &create_opts
        );
        _flags.set(MQTT_FLAG_PAHO_INIT, (MQTTASYNC_SUCCESS == rc) && (nullptr != _client_handle));
      }

      if (_flags.value(MQTT_FLAG_PAHO_INIT)) {
        int local_rc = MQTTASYNC_SUCCESS;
        local_rc |= MQTTAsync_setCallbacks(
          _client_handle,
          this,
          _paho_connection_lost,
          _paho_message_arrived,
          _paho_delivery_complete
        );
        local_rc |= MQTTAsync_setConnected(_client_handle, this, _paho_connected);
        local_rc |= MQTTAsync_setDisconnected(_client_handle, this, _paho_disconnected);

        _flags.set(MQTT_FLAG_CALLBACKS_SET, (MQTTASYNC_SUCCESS == local_rc));
        if (!_flags.value(MQTT_FLAG_CALLBACKS_SET)) {
          _set_fault("Unable to register Paho callbacks.");
        }
      }

      state_entry_success = initialized();
      break;

    case MQTTCliState::CONNECTING:
      if (_current_broker.isValid() && initialized()) {
        MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer5;
        conn_opts.keepAliveInterval = 60;
        conn_opts.connectTimeout    = 30;
        conn_opts.username          = ((0 < strlen(_current_broker.user()))   ? _current_broker.user()   : nullptr);
        conn_opts.password          = ((0 < strlen(_current_broker.passwd())) ? _current_broker.passwd() : nullptr);
        conn_opts.automaticReconnect = 0;
        conn_opts.onSuccess5        = _paho_connect_success5;
        conn_opts.onFailure5        = _paho_connect_failure5;
        conn_opts.context           = this;
        conn_opts.MQTTVersion       = MQTTVERSION_5;
        conn_opts.cleanstart        = 1;

        // TODO: Add conn_opts.ssl for mqtts:// / ssl:// / wss:// when this build
        //       links against paho-mqtt3as and a TLS config surface exists.

        _connect_attempt_active = true;
        _mqtt_disconnected_latched = false;
        _mqtt_connected_latched    = false;

        _connect_api_rc_last = MQTTAsync_connect(_client_handle, &conn_opts);
        state_entry_success = true;
      }
      break;

    case MQTTCliState::CONNECTED:
      _connect_attempt_active    = false;
      _reconnect_backoff_ms      = 5000;
      _mqtt_disconnected_latched = false;
      _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);
      state_entry_success = true;
      break;

    case MQTTCliState::SUBSCRIBING:
      _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);
      state_entry_success = true;
      break;

    case MQTTCliState::DISCONNECTING:
      {
        _connect_attempt_active = false;
        _flags.set(MQTT_FLAG_SUBS_COMPLETE, false);

        MQTTAsync_disconnectOptions discon_opts = MQTTAsync_disconnectOptions_initializer;
        discon_opts.timeout    = 0;
        discon_opts.onSuccess5 = _paho_disconnect_success5;
        discon_opts.onFailure5 = _paho_disconnect_failure5;
        discon_opts.context    = this;
        discon_opts.reasonCode = MQTTREASONCODE_SUCCESS;

        if ((nullptr != _client_handle) && connected()) {
          const int rc = MQTTAsync_disconnect(_client_handle, &discon_opts);
          if (MQTTASYNC_SUCCESS != rc) {
            _mb_set_mqtt_connected(false);
            _mb_set_mqtt_disconnected(true);
          }
        }
        else {
          _mb_set_mqtt_connected(false);
          _mb_set_mqtt_disconnected(true);
        }

        state_entry_success = true;
      }
      break;

    case MQTTCliState::DISCONNECTED:
      state_entry_success = true;
      _normalize_subs_for_disconnect();

      if (_connect_attempt_active) {
        _connect_attempt_active = false;

        const bool greedy = (autoconnect() && _current_broker.autoconnect() && _current_broker.isValid());
        if (greedy) {
          _fsm_lockout(_reconnect_backoff_ms);

          if (_reconnect_backoff_ms < _reconnect_backoff_ms_max) {
            uint32_t next = (_reconnect_backoff_ms << 1);
            _reconnect_backoff_ms = ((next > _reconnect_backoff_ms_max) ? _reconnect_backoff_ms_max : next);
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
      c3p_log(
        LOG_LEV_NOTICE,
        "MQTTClientPaho::_fsm_set_position",
        "MQTTClientPaho State %s ---> %s",
        _FSM_STATES.enumStr(CURRENT_STATE),
        _FSM_STATES.enumStr(new_state)
      );
    }
    ret = 0;
  }

  return ret;
}


void MQTTClientPaho::_set_fault(const char* msg) {
  if (_log_verbosity >= LOG_LEV_WARN) {
    c3p_log(LOG_LEV_WARN, "MQTTClientPaho::_set_fault", "MQTTClientPaho fault: %s", msg);
  }
  _fsm_mark_current_state(MQTTCliState::FAULT);
}


/*******************************************************************************
* Debug
*******************************************************************************/

void MQTTClientPaho::printDebug(StringBuilder* output) {
  StringBuilder tmp;
  StringBuilder prod_str("MQTTClientPaho");
  prod_str.concatf(" [%sinitialized]", (initialized() ? "" : "un"));
  StringBuilder::styleHeader2(&tmp, (const char*) prod_str.string());
  printFSM(&tmp);

  tmp.concatf("\t Client autoconnect: %c\n", (autoconnect() ? 'y' : 'n'));
  tmp.concatf("\t Broker autoconnect: %c\n", (_current_broker.autoconnect() ? 'y' : 'n'));
  tmp.concatf("\t Broker valid:       %c\n", (_current_broker.isValid() ? 'y' : 'n'));
  tmp.concatf("\t Transport valid:    %c\n", (_uri_supported_in_build(_current_broker.uri()) ? 'y' : 'n'));
  tmp.concatf("\t Connected:          %c\n", (connected() ? 'y' : 'n'));
  tmp.concatf("\t Subs complete:      %c\n", (_flags.value(MQTT_FLAG_SUBS_COMPLETE) ? 'y' : 'n'));
  tmp.concatf("\t Backoff:            %u ms (max %u)\n", _reconnect_backoff_ms, _reconnect_backoff_ms_max);
  tmp.concatf("\t Client ID:          %s\n", _effective_client_id());
  tmp.concatf("\t MQTT URI:           %s\n", _current_broker.uri());

  if (nullptr != _client_subs) {
    _client_subs->printDebug(&tmp);
  }

  tmp.string();
  output->concatHandoff(&tmp);
}

#endif  // CONFIG_C3P_WITH_PAHO
