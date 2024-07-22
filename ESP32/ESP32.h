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

#include "AbstractPlatform.h"
#include "StringBuilder.h"
#include "BusQueue/UARTAdapter.h"

#if defined(CONFIG_C3P_STORAGE)
  #include "Storage/Storage.h"
#endif

/* These includes from ESF-IDF need to be under C linkage. */
extern "C" {
  #include "sdkconfig.h"
  #include "xtensa_api.h"
  #include "esp_idf_version.h"
  #include "esp_event.h"
  #include "freertos/task.h"
}


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


// Any source file that needs platform member functions should be able to access
//   them this way.
extern ESP32Platform platform;

#endif  // __PLATFORM_ESP32_H__
