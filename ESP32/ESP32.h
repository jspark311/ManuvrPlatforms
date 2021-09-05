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

extern "C" {
  #include "driver/gpio.h"
  #include "esp_system.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
}


#if defined(CONFIG_MANUVR_STORAGE)
  #include "ESP32Storage.h"
#endif

extern uint8_t temprature_sens_read();


/*
* The STDIO driver class.
*/
class ESP32StdIO : public BufferAccepter {
  public:
    ESP32StdIO();
    ~ESP32StdIO();

    /* Implementation of BufferAccepter. */
    inline int8_t provideBuffer(StringBuilder* buf) {  _tx_buffer.concatHandoff(buf); return 1;   };
    inline void readCallback(BufferAccepter* cb) {   _read_cb_obj = cb;   };

    inline void write(const char* str) {  _tx_buffer.concat((uint8_t*) str, strlen(str));  };

    int8_t poll();


  private:
    BufferAccepter* _read_cb_obj = nullptr;
    StringBuilder   _tx_buffer;
    StringBuilder   _rx_buffer;
};



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


  private:
    void   _close_open_threads();
    void   _init_rng();
};


// Any source file that needs platform member functions should be able to access
//   them this way.
extern ESP32Platform platform;

#endif  // __PLATFORM_ESP32_H__
