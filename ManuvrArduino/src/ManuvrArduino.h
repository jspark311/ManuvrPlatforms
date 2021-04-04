/*
* ManuvrPlatform.h should be included from within the sketch, and this header
*   will sort out which specific APIs to use.
*
*/

#include <AbstractPlatform.h>
#include <StringBuilder.h>


#if defined(__IMXRT1052__) || defined(__IMXRT1062__)
  /*
  * Teensy4
  */
#elif defined(__MK20DX256__) || defined(__MK20DX128__)
  /*
  * Teensy3
  */
  #include "Teensy3/TeensyStorage.h"
#else
  /*
  * Vanilla Arduino
  * TODO: This is easy to add. But it hasn't been done.
  */
#endif
