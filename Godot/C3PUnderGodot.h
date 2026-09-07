/*
File:   C3PUnderGodot.h
Author: J. Ian Lindsay
Date:   2026.04.03

Godot is a cross-platform video game framework that C3P uses for
  UI on platforms large enough to support it. Given its
  cross-platform nature, Godot has its own platform agnosticism
  layer, for which this file acts as a shim.
*/


#ifndef __PLATFORM_GODOT_H__
#define __PLATFORM_GODOT_H__
#include "AbstractPlatform.h"
#include "LightLinkedList.h"
#include "TimerTools/C3PScheduler.h"

#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/socket.h>



/*******************************************************************************
* The STDIO driver class
*******************************************************************************/
class GodotStdIO : public BufferAccepter {
  public:
    GodotStdIO();
    ~GodotStdIO();

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


/*******************************************************************************
* Platform object
*******************************************************************************/

// TODO: Support versions other than v4.4.
#define GOTO_PF_STRING "Godot-4.4"

class GodotPlatform : public AbstractPlatform {
  public:
    GodotPlatform() : AbstractPlatform(GOTO_PF_STRING) {};
    ~GodotPlatform() {};

    /* Obligatory overrides from AbstrctPlatform. */
    void firmware_reset(uint8_t);
    void firmware_shutdown(uint8_t);
    int8_t init();
    void printDebug(StringBuilder* out);

    /* Threading */
    int createThread(unsigned long*, void*, ThreadFxnPtr, void*, PlatformThreadOpts*);
    int deleteThread(unsigned long*);
    int wakeThread(unsigned long);

    inline int  yieldThread() {    return sched_yield();     };
    inline void suspendThread() {  sleep_ms(100);            };   // TODO

    const char* selfPath();


  private:
    void   _close_open_threads();
    void   _init_rng();

    int8_t internal_integrity_check(uint8_t* test_buf, int test_len);
    int8_t _hash_self();
};


// Any source file that needs platform member functions should be able to access
//   them this way.
extern GodotPlatform platform;

#endif  // __PLATFORM_GODOT_H__
