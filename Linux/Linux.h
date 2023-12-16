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
#include "AbstractPlatform.h"
#include "BusQueue/UARTAdapter.h"
#include "BusQueue/I2CAdapter.h"
#include "CryptoBurrito/CryptoBurrito.h"
#include "TimerTools/C3PScheduler.h"

#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/socket.h>

#if defined(CONFIG_C3P_STORAGE)
  #include "LinuxStorage.h"
#endif

#if defined(__MACH__) && defined(__APPLE__)
  typedef unsigned long pthread_t;
#endif


int8_t _load_config();       // Called during boot to load configuration.

/*******************************************************************************
* The STDIO driver class
*******************************************************************************/
class LinuxStdIO : public BufferAccepter {
  public:
    LinuxStdIO();
    ~LinuxStdIO();

    /* Implementation of BufferAccepter. */
    inline int8_t pushBuffer(StringBuilder* buf) {  _tx_buffer.concatHandoff(buf); return 1;   };
    inline int32_t bufferAvailable() {  return 0xFFFF;   };   // TODO: Use real value.

    inline void readCallback(BufferAccepter* cb) {   _read_cb_obj = cb;   };
    inline void write(const char* str) {  _tx_buffer.concat((uint8_t*) str, strlen(str));  };
    int8_t poll();

  private:
    BufferAccepter* _read_cb_obj = nullptr;
    StringBuilder   _tx_buffer;
    StringBuilder   _rx_buffer;
};


/*******************************************************************************
* Socket driver class
*******************************************************************************/
class LinuxSockPipe;
class LinuxSockListener;
typedef int8_t (*NewSocketCallback)(LinuxSockListener*, LinuxSockPipe*);


class LinuxSockListener {
  public:
    LinuxSockListener(char* path);
    LinuxSockListener() {};
    ~LinuxSockListener();

    inline void newConnectionCallback(NewSocketCallback cb) {   _new_cb = cb;   };
    int8_t poll();
    void   printDebug(StringBuilder* out);

    int    listening();  // Returns the number of connections, or -1 if not listening.
    int    listen(char* path = nullptr);  // Open a listening socket.
    int8_t close();   // Close the listener, if it is open.

    /* Built-in per-instance console handler. */
    int8_t console_handler(StringBuilder* text_return, StringBuilder* args);


  private:
    int             _sock_id     = 0;
    char*           _sock_path   = nullptr;
    unsigned long   _thread_id   = 0;
    NewSocketCallback _new_cb   = nullptr;

    int8_t _set_sock_path(char*);
};


class LinuxSockPipe : public BufferAccepter {
  public:
    LinuxSockPipe(char* path, int sock_id);
    LinuxSockPipe(char* path);
    LinuxSockPipe() : LinuxSockPipe(nullptr) {};
    virtual ~LinuxSockPipe();

    /* Implementation of BufferAccepter. */
    inline int8_t pushBuffer(StringBuilder* buf) {  _tx_buffer.concatHandoff(buf); return 1;   };
    inline int32_t bufferAvailable() {  return 0xFFFF;   };   // TODO: Use real value.

    inline void readCallback(BufferAccepter* cb) {   _read_cb_obj = cb;   };
    int8_t poll();
    void   printDebug(StringBuilder* out);

    inline void write(const char* str) {  _tx_buffer.concat((uint8_t*) str, strlen(str));  };
    uint32_t read(StringBuilder* buf);
    uint32_t read(uint8_t* buf, uint32_t len);
    uint32_t write(char c);
    uint32_t write(uint8_t* buf, uint32_t len);

    int    connected();  // Returns the number of connections, or -1 if not connected.
    int    connect(char* path = nullptr);  // Open an existing socket.
    int8_t close();   // Close the socket, if it is open.
    inline bool   flushed() {   return _tx_buffer.isEmpty();   };

    /* Built-in per-instance console handler. */
    int8_t console_handler(StringBuilder* text_return, StringBuilder* args);


  private:
    BufferAccepter* _read_cb_obj = nullptr;
    uint32_t        _flags       = 0;
    uint32_t        _last_rx_ms  = 0;
    uint32_t        _count_tx    = 0;
    uint32_t        _count_rx    = 0;
    int             _sock_id     = 0;
    char*           _sock_path   = nullptr;
    StringBuilder   _tx_buffer;
    StringBuilder   _rx_buffer;

    int8_t _open();
    int8_t _set_sock_path(char*);
};


/*******************************************************************************
* Wrapper classes to allow taking a path as a device identifier.
*******************************************************************************/
class LinuxUART : public UARTAdapter {
  public:
    LinuxUART(char* path);
    ~LinuxUART();
};

class LinuxI2C : public I2CAdapter {
  public:
    LinuxI2C(char* path, const I2CAdapterOptions*);
    ~LinuxI2C();
};


/*******************************************************************************
* Platform object
*******************************************************************************/

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
