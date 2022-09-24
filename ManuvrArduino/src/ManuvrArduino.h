/*
* ManuvrPlatform.h should be included from within the sketch, and this header
*   will sort out which specific APIs to use.
*
*/

#include <AbstractPlatform.h>
#include <StringBuilder.h>

#ifndef __PLATFORM_ARDUINO_H__
#define __PLATFORM_ARDUINO_H__

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
  /* Teensy4 */
  #define __MAPF_ID_STRING  "Teensy4"
  // Teensy4 support uses a library capable of async operation.
  // https://github.com/Richard-Gemmell/teensy4_i2c
  #include <i2c_driver.h>
  #include "imx_rt1060/imx_rt1060_i2c_driver.h"
#elif defined(__MK20DX256__) || defined(__MK20DX128__)
  /* Teensy3 */
  #define __MAPF_ID_STRING  "Teensy3"
  #include <Wire.h>
#else
  /* Vanilla Arduino */
  #define _MANUVR_VANILLA_ARDUINO
  #define __MAPF_ID_STRING  "Generic"
#endif


#if defined(CONFIG_C3P_STORAGE)
  #if defined(__IMXRT1052__) || defined(__IMXRT1062__)
    // TODO: support this.
  #elif defined(__MK20DX256__) || defined(__MK20DX128__)
    #include "Teensy3/TeensyStorage.h"
  #else
    #error CONFIG_C3P_STORAGE was set, but have no support in Vanilla Arduino.
  #endif
#endif   // CONFIG_C3P_STORAGE


class ArduinoPlatform : public AbstractPlatform {
  public:
    ArduinoPlatform() : AbstractPlatform(__MAPF_ID_STRING) {};
    ~ArduinoPlatform() {};

    /* Obligatory overrides from AbstrctPlatform. */
    int8_t init();
    void   printDebug(StringBuilder*);
    void   firmware_reset(uint8_t);
    void   firmware_shutdown(uint8_t);


  private:
    void   _init_rng();
};


// Any source file that needs platform member functions should be able to access
//   them this way.
extern ArduinoPlatform platform;

#endif  // __PLATFORM_ARDUINO_H__
