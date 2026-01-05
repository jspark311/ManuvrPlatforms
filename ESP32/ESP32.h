/*
File:   ESP32.h
Author: J. Ian Lindsay
Date:   2016.08.31

Copyright 2016 Manuvr, Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#ifndef __PLATFORM_ESP32_H__
#define __PLATFORM_ESP32_H__

#include <sys/time.h>
#include <time.h>

#include "AbstractPlatform.h"
#include "StringBuilder.h"
#include "FiniteStateMachine.h"
#include "FlagContainer.h"
#include "BusQueue/UARTAdapter.h"

#if defined(CONFIG_C3P_STORAGE)
  #include "Storage/Storage.h"
#endif

#if !defined(ESP32RADIO_SCAN_RESULTS)
  #define ESP32RADIO_SCAN_RESULTS        16
#endif



/* These includes from ESF-IDF need to be under C linkage. */
#ifdef __cplusplus
extern "C" {
#endif
  #include "esp_system.h"
  #include "nvs_flash.h"
  #include "sdkconfig.h"
  #include "xtensa_api.h"
  #include "esp_idf_version.h"
  #include "esp_event.h"
  #include "freertos/task.h"
  #include "freertos/event_groups.h"
  #include "esp_log.h"
  #include "esp_err.h"

  #include "lwip/err.h"
  #include "lwip/sockets.h"
  #include "lwip/dns.h"
  #include "lwip/netdb.h"
  #include "lwip/sys.h"
  #include "esp_wifi.h"
  #include "esp_http_client.h"

  #include "mqtt_client.h"
#ifdef __cplusplus
}
#endif


// This platform provides an on-die temperature sensor.
extern uint8_t temprature_sens_read();
int console_callback_esp_storage(StringBuilder*, StringBuilder*);


/*
* The STDIO driver class.
*/
class ESP32StdIO : public BufferAccepter {
  public:
    ESP32StdIO();
    ~ESP32StdIO();

    /* Implementation of BufferAccepter. */
    inline int8_t pushBuffer(StringBuilder* buf) {  _tx_buffer.concatHandoff(buf); return 1;   };
    inline int32_t bufferAvailable() {  return 1024;  };   // TODO: Use real value.

    inline void readCallback(BufferAccepter* cb) {   _read_cb_obj = cb;   };

    inline void write(const char* str) {  _tx_buffer.concat((uint8_t*) str, strlen(str));  };

    int8_t poll();


  private:
    BufferAccepter* _read_cb_obj = nullptr;
    StringBuilder   _tx_buffer;
    StringBuilder   _rx_buffer;
};


/*
* Platform-specific wrapper around UARTAdapter.
*/
class PlatformUART : public UARTAdapter {
  public:
    PlatformUART(
      const uint8_t adapter,
      const uint8_t txd_pin, const uint8_t rxd_pin,
      const uint8_t cts_pin, const uint8_t rts_pin,
      const uint16_t tx_buf_len, const uint16_t rx_buf_len) :
      UARTAdapter(adapter, txd_pin, rxd_pin, cts_pin, rts_pin, tx_buf_len, rx_buf_len) {};
    ~PlatformUART() {  _pf_deinit();  };

    void irq_handler();

  protected:
    /* Obligatory overrides from UARTAdapter */
    int8_t _pf_init();
    int8_t _pf_poll();
    int8_t _pf_deinit();
};


/*
* Data storage interface for the ESP32's on-board flash.
* NOTE: This is terrible. And I feel terrible for writing it. It shouldn't be
*   used by anyone for any reason. Only pain can result. It is being retained
*   for its value as a possible restricted interface later on (if at all).
*/
#if defined(CONFIG_C3P_STORAGE)
class ESP32Storage : public Storage {
  public:
    ESP32Storage(const esp_partition_t*);
    ~ESP32Storage();

    /* Overrides from Storage. */
    uint64_t   freeSpace();  // How many bytes are availible for use?
    StorageErr init();
    StorageErr wipe(uint32_t offset, uint32_t len);  // Wipe a range.
    uint8_t    blockAddrSize() {  return DEV_ADDR_SIZE_BYTES;  };
    int8_t     allocateBlocksForLength(uint32_t, DataRecord*);

    StorageErr flush();          // Blocks until commit completes.

    StorageErr persistentWrite(DataRecord*, StringBuilder* buf);
    //StorageErr persistentRead(DataRecord*, StringBuilder* buf);
    StorageErr persistentWrite(uint8_t* buf, unsigned int len, uint32_t offset);
    StorageErr persistentRead(uint8_t* buf,  unsigned int len, uint32_t offset);

    void printDebug(StringBuilder*);
    friend int console_callback_esp_storage(StringBuilder*, StringBuilder*);


  private:
    const esp_partition_t* _PART_PTR;

    int8_t _close();             // Blocks until commit completes.
};
#endif   // CONFIG_C3P_STORAGE


/*
* The Platform class.
*/
class ESP32Platform : public AbstractPlatform {
  public:
    ESP32Platform() : AbstractPlatform(esp_get_idf_version()) {};
    ~ESP32Platform() {};

    /* Obligatory overrides from AbstrctPlatform. */
    int8_t init();
    void   printDebug(StringBuilder*);
    void   firmware_reset(uint8_t);
    void   firmware_shutdown(uint8_t);

    /* Threading */
    int createThread(unsigned long*, void*, ThreadFxnPtr, void*, PlatformThreadOpts*);
    int deleteThread(unsigned long*);
    int wakeThread(unsigned long);

    inline int  yieldThread() {    taskYIELD();  return 0;   };
    inline void suspendThread() {  vTaskSuspend(xTaskGetCurrentTaskHandle()); };

    /* Storage, if applicable */
    #if defined(CONFIG_C3P_STORAGE)
      ESP32Storage* storage = nullptr;
    #endif


  private:
    void   _close_open_threads();
    void   _init_rng();
};



/*******************************************************************************
* Radio wrappers
* TODO: Once it is useful on another platform, some of these classes might be
*   moved to C3P.
*******************************************************************************/

/*
* In station mode, we keep a list of AP's which we know about.
*/
class WiFiAP {
  public:
    char* name;

    WiFiAP();
    ~WiFiAP();


  private:
};



/** Class flags */
#define ESP32RADIO_FLAG_NETIF_INIT           0x00000001  //
#define ESP32RADIO_FLAG_EVENT_LOOP_CREATED   0x00000002  //
#define ESP32RADIO_FLAG_WIFI_INIT            0x00000008  //
#define ESP32RADIO_FLAG_WIFI_STARTED         0x00000010  //
#define ESP32RADIO_FLAG_INIT_WIFI_AS_STATION 0x00000020  // --+ These bits are mutex,
#define ESP32RADIO_FLAG_INIT_WIFI_AS_AP      0x00000040  // --+ and exactly ONE must
#define ESP32RADIO_FLAG_INIT_WIFI_AS_MESH    0x00000080  // --+ be set to be initialized.


// Bits indicating basic init steps.
#define ESP32RADIO_FLAG_PREINIT_MASK ( \
  ESP32RADIO_FLAG_NETIF_INIT | ESP32RADIO_FLAG_EVENT_LOOP_CREATED)

// Bits indicating basic init steps.
#define ESP32RADIO_FLAG_ALL_INIT_MASK ( \
  ESP32RADIO_FLAG_PREINIT_MASK | ESP32RADIO_FLAG_WIFI_INIT | \
  ESP32RADIO_FLAG_WIFI_STARTED)

// Bits to preserve through radio reset.
#define ESP32RADIO_FLAG_RESET_MASK  (ESP32RADIO_FLAG_PREINIT_MASK)


/** Radio state machine positions */
enum class ESP32RadioState : uint8_t {
  UNINIT = 0,     // init() has never been called.
  PRE_INIT,       // Memory and resource allocation.
  RESETTING,      // Radio reset.
  INIT,           // Comm stack setup.
  SCANNING,
  PROMISCUOUS,    // Idle state under connectionless layer-2 stacks.
  CONNECTING,     // Establishing a counterparty connection.
  CONNECTED,      // Idle state with connection.
  DISCONNECTING,  // Graceful disconnection from counterparties.
  DISCONNECTED,   // Idle state without connection.
  SLEEPING,
  WAKING,
  FAULT,          // State machine encountered something it couldn't cope with.
  INVALID = 255   // FSM hygiene.
};


/*
* A class to handle the radio peripheral. WiFi, Bluetooth, and any other layer-2
*   protocols are handled as transparently as we can manage.
*/
class ESP32Radio : public StateMachine<ESP32RadioState>, public C3PPollable {
  public:
    ESP32Radio();
    ~ESP32Radio();

    int8_t init();
    PollResult poll();

    int8_t serialize_ap(const uint8_t AP_IDX, StringBuilder* output);
    int8_t wifi_scan();

    void printDebug(StringBuilder*);
    int console_handler_esp_radio(StringBuilder*, StringBuilder*);

    /* Semantic breakouts for flags and states */
    inline bool initialized() {        return (ESP32RADIO_FLAG_ALL_INIT_MASK == (ESP32RADIO_FLAG_ALL_INIT_MASK & _flags.raw));  };
    inline bool connected() {          return (ESP32RadioState::CONNECTED == currentState());  };

    /* Network status API for sibling FSMs (MQTT, etc). */
    bool linkUp();      // True if STA is associated.
    bool hasIP();       // True if STA has IPv4.
    uint32_t ip4();     // IPv4 address (event->ip_info.ip.addr order).

    /* Singleton/factory support (optional usage pattern). */
    static ESP32Radio* getInstance();


  protected:
    FlagContainer32  _flags;                   // Aggregate boolean class state.
    uint8_t _log_verbosity = LOG_LEV_DEBUG;
    esp_netif_t* _sta_netif = nullptr;
    wifi_ap_record_t _current_ap;

    // Event handler instance tokens (so we can unregister if needed).
    esp_event_handler_instance_t _handler_any_id;
    esp_event_handler_instance_t _handler_ip_any_id;

    // TODO? RingBuffer<WiFiAP> _ap_list;

    /* Semantic breakouts for flags and states */
    inline bool _pre_init_complete() {  return (ESP32RADIO_FLAG_PREINIT_MASK == (ESP32RADIO_FLAG_PREINIT_MASK & _flags.raw));  };


    /* State machine functions */
    int8_t   _fsm_poll();                         // Polling for state exit.
    int8_t   _fsm_set_position(ESP32RadioState);  // Attempt a state entry.
    int8_t   _reset_fxn();
    int8_t   _post_reset_fxn();
    void     _set_fault(const char*);

    void _print_ap_record(StringBuilder*, wifi_ap_record_t*);

    /* Scan utility */
    void _collect_scan_results();

    /* Mailbox helpers (called from event-loop context) */
    void _mb_set_wifi_started(const bool v);
    void _mb_set_sta_connected(const bool v);
    void _mb_set_ip4_valid(const bool v);
    void _mb_set_scan_done(const bool v);
    void _mb_set_ip4_addr(const uint32_t a);

    /* Mailboxes written by event handlers, consumed in poll(). */
    volatile bool     _mb_wifi_started   = false;
    volatile bool     _mb_sta_connected  = false;
    volatile bool     _mb_ip4_valid      = false;
    volatile bool     _mb_scan_done      = false;
    volatile uint32_t _mb_ip4_addr       = 0;

    /* Latched state owned by poll/FSM context. */
    bool     _wifi_started      = false;
    bool     _sta_connected     = false;
    bool     _ip4_valid         = false;
    uint32_t _ip4_addr          = 0;

    bool     _scan_done_latched = false;

    /* Scan results storage */
    static wifi_ap_record_t _scan_list_ap[ESP32RADIO_SCAN_RESULTS];
    static uint16_t _scan_list_count;
    static uint16_t _scan_total_count;

    friend void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    friend void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
};




/*******************************************************************************
* Messaging wrappers
* TODO: Once it is useful on another platform, some of these classes might be
*   moved to C3P.
*******************************************************************************/

#define MQTT_FLAG_ESP_MQTT_INIT        0x00000001  //
#define MQTT_FLAG_EVENT_LOOP_CREATED   0x00000002  //
#define MQTT_FLAG_EVENT_REGISTERED     0x00000004  //
#define MQTT_FLAG_AUTOCONNECT          0x00000008  // Client greedy-connect policy.

// Bits indicating basic init steps.
#define MQTT_CLI_FLAG_ALL_INIT_MASK (MQTT_FLAG_ESP_MQTT_INIT | MQTT_FLAG_EVENT_LOOP_CREATED | MQTT_FLAG_EVENT_REGISTERED)

/** Radio state machine positions */
enum class MQTTCliState : uint8_t {
  UNINIT = 0,     // init() has never been called.
  INIT,           // Memory and resource allocation.
  CONNECTING,     // Establishing a counterparty connection.
  CONNECTED,      // Idle state with connection.
  DISCONNECTING,  // Graceful disconnection from counterparties.
  DISCONNECTED,   // Idle state without connection.
  FAULT,          // State machine encountered something it couldn't cope with.
  INVALID = 255   // FSM hygiene.
};


// TODO: Explicit shared subscription support?
class MQTTBrokerDef {
  public:
    MQTTBrokerDef(const char* LABEL = nullptr);
    ~MQTTBrokerDef() {};  // We allocate nothing, so: featureless destructor.

    bool isValid();
    int  serialize(StringBuilder* out, const TCode FORMAT);
    void printDebug(StringBuilder*);

    /*
    * Accessors to wrap ESP-IDF's gnarly conf struct.
    * Side note: Version drift in this struct was the _worst_ thing about
    *   getting MQTT running following a migration from IDF v4 to v5.
    *   never-doing-that-again.png
    */
    void label(const char*);
    void uri(const char*);
    void user(const char*);
    void passwd(const char*);

    void autoconnect(bool v) {  _autoconnect = v;  };
    bool autoconnect() {        return _autoconnect;  };

    const char* label() {   return _label;                                         };
    const char* uri() {     return _cli_conf.broker.address.uri;                   };
    const char* user() {    return _cli_conf.credentials.username;                 };
    const char* passwd() {  return _cli_conf.credentials.authentication.password;  };

    esp_mqtt_client_config_t* config() {  return &_cli_conf;  };

  private:
    // Side note: Version drift in this struct was the _worst_ thing about
    //   getting MQTT running following a migration from IDF v4 to v5.
    //   never-doing-that-again.png
    esp_mqtt_client_config_t _cli_conf;
    char _label[16];
    char _uri[48];
    char _usr[16];
    char _pass[32];
    bool _autoconnect = true;
};


/*
* A class to handle the MQTT client.
* Network status is queried from ESP32Radio (MQTT does not register IP events).
*/
class MQTTClient : public StateMachine<MQTTCliState>, public C3PPollable {
  public:
    MQTTClient();
    ~MQTTClient();

    int8_t init();
    PollResult poll();

    void printDebug(StringBuilder*);
    int console_handler_mqtt_client(StringBuilder*, StringBuilder*);

    /* Semantic breakouts for flags and states */
    inline bool initialized() {         return (MQTT_CLI_FLAG_ALL_INIT_MASK == (MQTT_CLI_FLAG_ALL_INIT_MASK & _flags.raw));  };
    inline bool connected() {           return (MQTTCliState::CONNECTED == currentState());  };
    inline void autoconnect(bool v) {   _flags.set(MQTT_FLAG_AUTOCONNECT, v); };
    inline bool autoconnect() {         return _flags.value(MQTT_FLAG_AUTOCONNECT); };

    /* Dependency injection */
    void setRadio(ESP32Radio* r) { _radio = r; };


  protected:
    FlagContainer32  _flags;                   // Aggregate boolean class state.
    uint8_t _log_verbosity = LOG_LEV_DEBUG;
    MQTTBrokerDef   _current_broker;
    ESP32Radio*     _radio = nullptr;
    esp_mqtt_client_handle_t  _client_handle = nullptr;

    /* State machine functions */
    int8_t   _fsm_poll();                    // Polling for state exit.
    int8_t   _fsm_set_position(MQTTCliState);// Attempt a state entry.
    void     _set_fault(const char*);

    void _print_broker_record(StringBuilder*, MQTTBrokerDef*);

    /* Mailboxes written from ESP-IDF event loop, consumed in poll() */
    void _mb_set_mqtt_connected(const bool v);
    void _mb_set_mqtt_disconnected(const bool v);

    volatile bool _mb_mqtt_connected    = false;
    volatile bool _mb_mqtt_disconnected = false;

    bool _mqtt_connected_latched        = false;
    bool _mqtt_disconnected_latched     = false;

    /* Backoff control (applied in DISCONNECTED entry, on failed attempt) */
    uint32_t _reconnect_backoff_ms      = 5000;
    uint32_t _reconnect_backoff_ms_max  = 60000;
    bool     _connect_attempt_active    = false;

    void _broker_changed_reinit_plan();

    friend void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
};


// Any source file that needs platform member functions should be able to access
//   them this way.
extern ESP32Platform platform;

#endif  // __PLATFORM_ESP32_H__
