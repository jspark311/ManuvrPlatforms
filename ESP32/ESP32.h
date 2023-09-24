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

#include <AbstractPlatform.h>
#include <StringBuilder.h>

#if defined(CONFIG_C3P_STORAGE)
  #include <Storage.h>
#endif

/* These includes from ESF-IDF need to be under C linkage. */
extern "C" {
  #include "driver/adc.h"
  #include "driver/gpio.h"
  #include "driver/ledc.h"
  #include "driver/periph_ctrl.h"

  #include "esp_attr.h"
  #include "esp_err.h"
  #include "esp_event.h"
  #include "esp_heap_caps.h"
  #include "esp_intr_alloc.h"
  #include "esp_log.h"
  #include "esp_netif.h"
  #include "esp_partition.h"
  #include "esp_sleep.h"
  #include "esp_system.h"
  #include "esp_task_wdt.h"
  #include "esp_types.h"
  #include "esp_wifi.h"

  #include "esp32/rom/ets_sys.h"
  #include "esp32/rom/lldesc.h"
  #include "nvs_flash.h"
  #include "nvs.h"

  #include "soc/dport_reg.h"
  #include "soc/efuse_reg.h"
  #include "soc/gpio_reg.h"
  #include "soc/gpio_sig_map.h"
  #include "soc/io_mux_reg.h"
  #include "soc/rtc_cntl_reg.h"

  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/event_groups.h"
  #include "freertos/semphr.h"
  #include "freertos/ringbuf.h"
  #include "freertos/queue.h"
  #include "freertos/xtensa_api.h"
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



/*
* Data storage interface for the ESP32's on-board flash.
*/
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
