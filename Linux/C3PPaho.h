/*
* File:   C3PPaho.h
* Author: J. Ian Lindsay
* Date:   2026.04.18
*
* This is the root include file if you want your program to use Paho.
*/

#ifndef __C3P_PAHO_HEADER_H__
#define __C3P_PAHO_HEADER_H__


#include <pthread.h>
#include "MQTTAsync.h"

#include "CppPotpourri.h"
#include "C3PLinux.h"


#if !defined(MQTT_MAX_PACKET_SIZE)
  #define MQTT_MAX_PACKET_SIZE 2048
#endif

#define MQTT_FLAG_PAHO_INIT          0x00000001
#define MQTT_FLAG_CALLBACKS_SET      0x00000002
#define MQTT_FLAG_MUTEX_INIT         0x00000004
#define MQTT_FLAG_AUTOCONNECT        0x00000008
#define MQTT_FLAG_SUBS_COMPLETE      0x00000010

#define MQTT_CLI_FLAG_ALL_INIT_MASK  (MQTT_FLAG_PAHO_INIT | MQTT_FLAG_CALLBACKS_SET | MQTT_FLAG_MUTEX_INIT)

enum class MQTTCliState : uint8_t {
  UNINIT = 0,
  INIT,
  CONNECTING,
  CONNECTED,
  SUBSCRIBING,
  DISCONNECTING,
  DISCONNECTED,
  FAULT,
  INVALID = 255
};


class MQTTBrokerDefPaho : public MQTTBrokerDef {
  public:
    MQTTBrokerDefPaho(const char* LABEL = nullptr);
    MQTTBrokerDefPaho(const MQTTBrokerDefPaho&);
    ~MQTTBrokerDefPaho() {};

    bool set(MQTTBrokerDef*);
    bool set(MQTTBrokerDefPaho*);

    void clientId(const char*);
    inline const char* clientId() {  return _client_id;  };

  protected:
    char _client_id[48];
};


class MQTTClientPaho : public StateMachine<MQTTCliState>, public C3PMQTTClient {
  public:
    MQTTClientPaho();
    ~MQTTClientPaho();

    int8_t init();
    PollResult poll();

    bool setBroker(MQTTBrokerDef*);
    bool setBroker(MQTTBrokerDefPaho*);

    void printDebug(StringBuilder*);

    bool initialized() {         return (MQTT_CLI_FLAG_ALL_INIT_MASK == (MQTT_CLI_FLAG_ALL_INIT_MASK & _flags.raw));  };
    bool connected();
    void autoconnect(bool v) {   _flags.set(MQTT_FLAG_AUTOCONNECT, v); };
    bool autoconnect() {         return _flags.value(MQTT_FLAG_AUTOCONNECT); };
    int  publish(MQTTMessage*);
    int  subscribe(const char*, const uint8_t QOS, MQTTTopicCallback CB = nullptr);
    int  unsubscribe(const char*);

  private:
    enum class MQTTOpType : uint8_t {
      TOKEN_SUCCESS = 0,
      TOKEN_FAILURE
    };

    class MQTTMsgNode {
      public:
        MQTTMessage* msg;
        MQTTMsgNode* next;
        MQTTMsgNode(MQTTMessage* M) : msg(M), next(nullptr) {}
    };

    class MQTTOpNode {
      public:
        MQTTOpType type;
        int32_t    token;
        MQTTOpNode* next;
        MQTTOpNode(const MQTTOpType T, const int32_t TOK) : type(T), token(TOK), next(nullptr) {}
    };

    FlagContainer32  _flags;
    MQTTBrokerDefPaho _current_broker;
    MQTTAsync         _client_handle = nullptr;
    MQTTSub*          _client_subs   = nullptr;
    uint8_t           _log_verbosity = LOG_LEV_DEBUG;

    pthread_mutex_t   _mutex;
    volatile bool     _mb_mqtt_connected    = false;
    volatile bool     _mb_mqtt_disconnected = false;

    bool              _mqtt_connected_latched    = false;
    bool              _mqtt_disconnected_latched = false;
    bool              _connect_attempt_active    = false;
    int               _connect_api_rc_last       = 0;

    uint32_t          _reconnect_backoff_ms      = 5000;
    uint32_t          _reconnect_backoff_ms_max  = 60000;

    MQTTMsgNode*      _rx_queue_head = nullptr;
    MQTTMsgNode*      _rx_queue_tail = nullptr;
    MQTTOpNode*       _op_queue_head = nullptr;
    MQTTOpNode*       _op_queue_tail = nullptr;

    char              _runtime_client_id[48];

    int8_t   _fsm_poll();
    int8_t   _fsm_set_position(MQTTCliState);
    void     _set_fault(const char*);
    void     _broker_changed_reinit_plan();

    void     _mutex_lock();
    void     _mutex_unlock();

    bool     _uri_supported_in_build(const char*);
    const char* _effective_client_id();

    int      _enqueue_msg(MQTTMessage*);
    MQTTMessage* _dequeue_msg();
    int      _enqueue_op(const MQTTOpType, const int32_t);
    bool     _dequeue_op(MQTTOpType*, int32_t*);

    void     _free_mqtt_message(MQTTMessage*);
    void     _purge_subs();
    void     _normalize_subs_for_disconnect();
    int      _add_sub(MQTTSub*);
    int      _drop_sub(MQTTSub*);
    void     _apply_token_result(const int32_t, const bool);
    MQTTTopicCallback _topic_callback_for(const char*);

    void     _mb_set_mqtt_connected(const bool v);
    void     _mb_set_mqtt_disconnected(const bool v);

    static void _paho_connection_lost(void*, char*);
    static int  _paho_message_arrived(void*, char*, int, MQTTAsync_message*);
    static void _paho_delivery_complete(void*, MQTTAsync_token);
    static void _paho_connected(void*, char*);
    static void _paho_disconnected(void*, MQTTProperties*, enum MQTTReasonCodes);

    static void _paho_connect_success5(void*, MQTTAsync_successData5*);
    static void _paho_connect_failure5(void*, MQTTAsync_failureData5*);
    static void _paho_disconnect_success5(void*, MQTTAsync_successData5*);
    static void _paho_disconnect_failure5(void*, MQTTAsync_failureData5*);
    static void _paho_subop_success5(void*, MQTTAsync_successData5*);
    static void _paho_subop_failure5(void*, MQTTAsync_failureData5*);
};

#endif  // __C3P_PAHO_HEADER_H__
