/*
* This is the platform abstraction for the Teensyduino environment.
*/

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

#include <AbstractPlatform.h>
#include <StringBuilder.h>


static volatile uint32_t tick_count = 0;   // The timebase for the rest of the program.


/*******************************************************************************
* Watchdog                                                                     *
*******************************************************************************/


/*******************************************************************************
* Platform interrupts that we handle in a uniform manner.
* These are not exposed in the header file.
*******************************************************************************/

uint32_t randomUInt32() {
  return ((uint32_t) random(2147483647)) ^ (((uint32_t) random(2147483647)) << 1);
}

/*******************************************************************************
* Time and date                                                                *
*******************************************************************************/
  // Arduino has these.
  //uint32_t millis();
  //uint32_t micros();

  /* Delay functions */
  // TODO: Install handler and sleep instead of burn the clock?
  void sleep_ms(uint32_t ms) {
    uint32_t stop_ms  = millis() + ms;
    while (millis() <= stop_ms) {}
  }

  void sleep_us(uint32_t us) {
    uint32_t stop_us = micros() + us;
    while (micros() <= stop_us) {}
  }


/*******************************************************************************
* GPIO and change-notice                                                       *
*******************************************************************************/
  /*
  * Sets the GPIO pin direction, and configure pullups.
  * Does not yet use the SCU to set pull resistors.
  * Assumes 8mA pin drive for outputs.
  *
  * Returns -1 if pin is out of range.
  *         -2 if GPIO control of pin isn't possible.
  *         -3 if pin mode is unsupported.
  */
  int8_t pinMode(uint8_t pin, GPIOMode m) {
    int8_t ret = -1;
    switch (m) {
      case GPIOMode::INPUT:
      case GPIOMode::OUTPUT:
      case GPIOMode::INPUT_PULLUP:
      case GPIOMode::INPUT_PULLDOWN:
      case GPIOMode::OUTPUT_OD:
        pinMode(pin, (int) m);
        ret = 0;
        break;
      case GPIOMode::ANALOG_IN:
      case GPIOMode::BIDIR_OD:
      case GPIOMode::BIDIR_OD_PULLUP:
      case GPIOMode::ANALOG_OUT:
      case GPIOMode::UNINIT:
        ret = -3;   // TODO: Unsupported for now.
        break;
    }
    return ret;
  }


  int8_t setPin(uint8_t pin, bool val) {
    digitalWrite(pin, val);
    return 0;
  }


  int8_t readPin(uint8_t pin) {
    return digitalRead(pin);
  }


  void unsetPinFxn(uint8_t pin) {
    detachInterrupt(digitalPinToInterrupt(pin));
  }

  int8_t setPinFxn(uint8_t pin, IRQCondition condition, FxnPointer fxn) {
    attachInterrupt(digitalPinToInterrupt(pin), fxn, (int) condition);
    return 0;
  }



/*******************************************************************************
* Clock handling
*******************************************************************************/


/*******************************************************************************
* Time, date, and RTC abstraction
* TODO: This might be migrated into a separate abstraction.
*
* NOTE: Datetime string interchange always uses ISO-8601 format unless otherwise
*         provided.
* NOTE: 1972 was the first leap-year of the epoch.
* NOTE: Epoch time does not account for leap seconds.
* NOTE: We don't handle dates prior to the Unix epoch (since we return an epoch timestamp).
* NOTE: Leap years that are divisible by 100 are NOT leap years unless they are also
*         divisible by 400. I have no idea why. Year 2000 meets this criteria, but 2100
*         does not. 2100 is not a leap year. In any event, this code will give bad results
*         past the year 2100. So fix it before then.
* NOTE: We can't handle dates prior to 1583 because some king ripped 10 days out of the
*         calandar in October of the year prior.
*
* Format: 2016-11-16T21:44:07Z
*******************************************************************************/

bool rtcInitilized() {
  bool ret = false;
  // TODO
  return ret;
}


/**
* If the RTC is set to a time other than default, and at least equal to the
*   year the firmware was built, we assume it to be accurate.
* TODO: would be better to use a set bit in RTC memory, maybe...
*
* @return true if the RTC is reliable.
*/
bool rtcAccurate() {
  bool ret = false;
  // TODO
  return ret;
}


bool setTimeAndDateStr(char*) {
  bool ret = false;
  // TODO
  return ret;
}


/**
*
* @param y   Year
* @param m   Month [1, 12]
* @param d   Day-of-month [1, 31]
* @param h   Hours [0, 23]
* @param mi  Minutes [0, 59]
* @param s   Seconds [0, 59]
* @return true on success.
*/
bool setTimeAndDate(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi, uint8_t s) {
  // TODO
  return false;
}


/**
* Get the date and time from the RTC with pointers to discrete value bins.
* Passing nullptr for any of the arguments is safe.
*
* @param y   Year
* @param m   Month [1, 12]
* @param d   Day-of-month [1, 31]
* @param h   Hours [0, 23]
* @param mi  Minutes [0, 59]
* @param s   Seconds [0, 59]
* @return true on success.
*/
bool getTimeAndDate(uint16_t* y, uint8_t* m, uint8_t* d, uint8_t* h, uint8_t* mi, uint8_t* s) {
  bool ret = false;
  // TODO
  ret = rtcAccurate();
  return ret;
}


/*
* Returns an integer representing the current datetime.
*/
uint64_t epochTime() {
  uint64_t ret = 0;
  // TODO
  return ret;
}


/*
* Writes a ISO-8601 datatime string in Zulu time to the argument.
* Format: 2016-11-16T21:44:07Z
*/
void currentDateTime(StringBuilder* output) {
}



/*******************************************************************************
* Platform initialization.                                                     *
*******************************************************************************/

/*
* Do the boilerplate setup of the MCU that all applications will require.
*/
int8_t platform_init() {
  return 0;
}


#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
