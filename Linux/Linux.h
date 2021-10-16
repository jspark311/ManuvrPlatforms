/*
File:   Linux.h
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


This file forms the catch-all for linux platforms that have no support.
*/


#ifndef __PLATFORM_VANILLA_LINUX_H__
#define __PLATFORM_VANILLA_LINUX_H__
#include <AbstractPlatform.h>
#include <UARTAdapter.h>
#include <I2CAdapter.h>

#include <pthread.h>
#include <signal.h>
#include <sys/time.h>


#if defined(CONFIG_MANUVR_STORAGE)
  #include "LinuxStorage.h"
#endif

#if defined(__MACH__) && defined(__APPLE__)
  typedef unsigned long pthread_t;
#endif


int8_t _load_config();       // Called during boot to load configuration.

/*
* The STDIO driver class.
*/
class LinuxStdIO : public BufferAccepter {
  public:
    LinuxStdIO();
    ~LinuxStdIO();

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


/* The UART wrapper class to allow taking a path as a device identifier. */
class LinuxUART : public UARTAdapter {
  public:
    LinuxUART(char* path);
    ~LinuxUART();
};


/* The UART wrapper class to allow taking a path as a device identifier. */
class LinuxI2C : public I2CAdapter {
  public:
    LinuxI2C(char* path);
    ~LinuxI2C();
};



class LinuxPlatform : public AbstractPlatform {
  public:
    LinuxPlatform() : AbstractPlatform("Generic") {};
    ~LinuxPlatform() {};

    /* Obligatory overrides from AbstrctPlatform. */
    void firmware_reset(uint8_t);
    void firmware_shutdown(uint8_t);
    int8_t init();
    void printDebug(StringBuilder* out);

    /* Threading */
    int createThread(unsigned long*, void*, ThreadFxnPtr, void*, PlatformThreadOpts*);
    int deleteThread(unsigned long*);
    int wakeThread(unsigned long);

    inline int  yieldThread() {    return pthread_yield();   };
    inline void suspendThread() {  sleep_ms(100);            };   // TODO


  private:
    void   _close_open_threads();
    void   _init_rng();
    #if defined(__HAS_CRYPT_WRAPPER)
      // Additional ratchet-straps (if we were built with CryptoBurrito).
      int8_t internal_integrity_check(uint8_t* test_buf, int test_len);
      int8_t _hash_self();
    #endif
};


// Any source file that needs platform member functions should be able to access
//   them this way.
extern LinuxPlatform platform;

#endif  // __PLATFORM_VANILLA_LINUX_H__
