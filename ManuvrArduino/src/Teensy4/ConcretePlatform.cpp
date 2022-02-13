/*
* This is the platform abstraction for the Teensyduino environment.
*/

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

#define ANALOG_WRITE_RES_BITS   12

#include "../ManuvrArduino.h"
#include <StringBuilder.h>

/*******************************************************************************
* Global platform singleton.                                                   *
*******************************************************************************/
ArduinoPlatform platform;
AbstractPlatform* platformObj() {   return (AbstractPlatform*) &platform;   }


/*******************************************************************************
* Watchdog                                                                     *
*******************************************************************************/


/*******************************************************************************
*     ______      __
*    / ____/___  / /__________  ____  __  __
*   / __/ / __ \/ __/ ___/ __ \/ __ \/ / / /
*  / /___/ / / / /_/ /  / /_/ / /_/ / /_/ /
* /_____/_/ /_/\__/_/   \____/ .___/\__, /
*                           /_/    /____/
*******************************************************************************/
/**
* Dead-simple interface to the RNG.
*
* @return   A 32-bit unsigned random number. This can be cast as needed.
*/
uint32_t randomUInt32() {
  return ((uint32_t) random(2147483647)) ^ (((uint32_t) random(2147483647)) << 1);
}

/**
* Fills the given buffer with random bytes.
* Blocks if there is nothing random available.
*
* @param uint8_t* The buffer to fill.
* @param size_t The number of bytes to write to the buffer.
* @return 0, always.
*/
int8_t random_fill(uint8_t* buf, size_t len) {
  int written_len = 0;
  while (4 <= (len - written_len)) {
    // If we have slots for them, just up-cast and write 4-at-a-time.
    *((uint32_t*) (buf + written_len)) = randomUInt32();
    written_len += 4;
  }
  uint32_t slack = randomUInt32();
  while (0 < (len - written_len)) {
    *(buf + written_len) = (uint8_t) 0xFF & slack;
    slack = slack >> 8;
    written_len++;
  }
  return 0;
}


/**
* Init the RNG. Short and sweet.
*/
void ArduinoPlatform::_init_rng() {
  // TODO: Seed the PRNG...
  _alter_flags(true, ABSTRACT_PF_FLAG_RNG_READY);
}


/*******************************************************************************
*  ___   _           _      ___
* (  _`\(_ )        ( )_  /'___)
* | |_) )| |    _ _ | ,_)| (__   _    _ __   ___ ___
* | ,__/'| |  /'_` )| |  | ,__)/'_`\ ( '__)/' _ ` _ `\
* | |    | | ( (_| || |_ | |  ( (_) )| |   | ( ) ( ) |
* (_)   (___)`\__,_)`\__)(_)  `\___/'(_)   (_) (_) (_)
* These are overrides and additions to the platform class.
*******************************************************************************/

void ArduinoPlatform::printDebug(StringBuilder* output) {
  output->concatf(
    "==< Arduino [%s] >=============================\n",
    _board_name
  );
  _print_abstract_debug(output);
}


/*******************************************************************************
* Time and date                                                                *
*******************************************************************************/
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
*     ____  _          ______            __             __
*    / __ \(_)___     / ____/___  ____  / /__________  / /
*   / /_/ / / __ \   / /   / __ \/ __ \/ __/ ___/ __ \/ /
*  / ____/ / / / /  / /___/ /_/ / / / / /_/ /  / /_/ / /
* /_/   /_/_/ /_/   \____/\____/_/ /_/\__/_/   \____/_/
*
* Teensyduino already provides some functions without us wrapping them.
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


  /**
  * Externally-facing function to set the carrier frequency for the PWM driver.
  * Enables PWM clocks on first call.
  *
  * @param pin The desired pin. Arduino Platform disregards this argument.
  * @param freq The desired carrier frequency.
  * @return 0 on success.
  */
  int8_t analogWriteFrequency(uint8_t pin, uint32_t freq) {
    //analogWriteFrequency(freq);   // Shunt to Teensyduino library.
    return 0;
  }


  /**
  * The ratio parameter will be clamped to the valid range.
  *
  * @param pin The desired pin.
  * @param ratio The desired duty ratio within the range [0.0, 1.0].
  * @return
  *   0  on success.
  *   -1 on failure.
  */
  int8_t analogWrite(uint8_t pin, float ratio) {
    analogWrite(pin, (int) (((1UL << ANALOG_WRITE_RES_BITS)-1) * ratio));
    return 0;
  }



/*******************************************************************************
* Process control                                                              *
*******************************************************************************/

/**
* Cause the CPU to reboot.
* Does not return.
*
* @param An app-specific byte to be stored in NVRAM as the restart reason.
*/
void ArduinoPlatform::firmware_reset(uint8_t reason) {
  (*(uint32_t*)0xE000ED0C) = 0x5FA0004;
  while(true);
}

/**
* This doesn't make sense on a platform that has a hardware PMIC. This will
*   simply reboot the MCU.
* Does not return.
*
* @param An app-specific byte to be stored in NVRAM as the restart reason.
*/
void ArduinoPlatform::firmware_shutdown(uint8_t reason) {
  firmware_reset(0);
}


/*******************************************************************************
*   _______                                   __   ____        __
*  /_  __(_)___ ___  ___     ____ _____  ____/ /  / __ \____ _/ /____
*   / / / / __ `__ \/ _ \   / __ `/ __ \/ __  /  / / / / __ `/ __/ _ \
*  / / / / / / / / /  __/  / /_/ / / / / /_/ /  / /_/ / /_/ / /_/  __/
* /_/ /_/_/ /_/ /_/\___/   \__,_/_/ /_/\__,_/  /_____/\__,_/\__/\___/
*
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
*     ____  __      __  ____                        ____  ____      __
*    / __ \/ /___ _/ /_/ __/___  _________ ___     / __ \/ __ )    / /
*   / /_/ / / __ `/ __/ /_/ __ \/ ___/ __ `__ \   / / / / __  |_  / /
*  / ____/ / /_/ / /_/ __/ /_/ / /  / / / / / /  / /_/ / /_/ / /_/ /
* /_/   /_/\__,_/\__/_/  \____/_/  /_/ /_/ /_/   \____/_____/\____/
*******************************************************************************/
#define  DEFAULT_PLATFORM_FLAGS   0

/**
* Init that needs to happen prior to kernel bootstrap().
* This is the final function called by the kernel constructor.
*/
int8_t ArduinoPlatform::init() {
  _discover_alu_params();
  analogWriteResolution(ANALOG_WRITE_RES_BITS);

  uint32_t default_flags = DEFAULT_PLATFORM_FLAGS;
  _alter_flags(true, default_flags);

  _init_rng();

  #if defined(__HAS_CRYPT_WRAPPER)
  #endif

  return 0;
}

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
