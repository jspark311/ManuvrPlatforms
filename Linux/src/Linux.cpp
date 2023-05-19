/*
File:   Linux.cpp
Author: J. Ian Lindsay
Date:   2015.11.01

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


This file forms the catch-all for linux platforms that have no specific support.
*/

#include "../Linux.h"

#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>
#if defined(CONFIG_C3P_STORAGE)
  #include <fcntl.h>      // Needed for integrity checks.
  #include <sys/stat.h>   // Needed for integrity checks.
#endif


#ifndef CONFIG_C3P_INTERVAL_PERIOD_MS
  // Unless otherwise specified, the interval timer will fire at 100Hz.
  #define CONFIG_C3P_INTERVAL_PERIOD_MS  10
#endif

#ifndef CONFIG_C3P_CRYPTO_QUEUE_MAX_DEPTH
  // Unless otherwise specified, the cryptographic processing queue depth is 32.
  #define CONFIG_C3P_CRYPTO_QUEUE_MAX_DEPTH  32
#endif

#ifndef PLATFORM_RNG_CARRY_CAPACITY
  #define PLATFORM_RNG_CARRY_CAPACITY  1024
#endif



#define MANUVR_INIT_STATE_UNINITIALIZED   0
#define MANUVR_INIT_STATE_RESERVED_0      1
#define MANUVR_INIT_STATE_PREINIT         2
#define MANUVR_INIT_STATE_KERNEL_BOOTING  3
#define MANUVR_INIT_STATE_POST_INIT       4
#define MANUVR_INIT_STATE_NOMINAL         5
#define MANUVR_INIT_STATE_SHUTDOWN        6
#define MANUVR_INIT_STATE_HALTED          7


/*******************************************************************************
* Global platform singleton.                                                   *
*******************************************************************************/
LinuxPlatform platform;
AbstractPlatform* platformObj() {   return (AbstractPlatform*) &platform;   }


/*******************************************************************************
* The code under this block is special on this platform,
*   and will not be available elsewhere.
*******************************************************************************/

#if defined(__HAS_CRYPT_WRAPPER)
  uint8_t _binary_hash[32];
  long unsigned int crypto_thread_id = 0;
#endif

struct itimerval _interval              = {0};
struct sigaction _signal_action_SIGALRM = {0};

char* _binary_name = nullptr;
static int   _main_pid    = 0;



/*******************************************************************************
* Signal catching code.                                                        *
*******************************************************************************/
/*
* This function is the signal handler for the basal platform. All other signals
*   are left for optional application usage.
*/
void _platform_sig_handler(int signo) {
  switch (signo) {
    case SIGKILL:
      printf("Received a SIGKILL signal. Something bad must have happened.\n");
    case SIGINT:    // CTRL+C will cause an immediate shutdown.
      platform.firmware_shutdown(1);   // TODO: Would be nice to give this to the application.
      break;
    case SIGTERM:
      printf("Received a SIGTERM signal.\n");
      platform.firmware_shutdown(1);   // TODO: Would be nice to give this to the application.
      break;
    case SIGVTALRM:
      printf("Received a SIGVTALRM signal.\n");
      // NOTE: No break;
    case SIGALRM:
      // Any periodic platform actions should be done here.
      C3PScheduler::getInstance()->advanceScheduler();
      break;
    default:
      printf("Unhandled signal: %d\n", signo);
      break;
  }
}


/*
* Used to setup the periodic alarm.
* Uses a real timer, rather than the PID's execution time.
*/
bool set_linux_interval_timer() {
  _signal_action_SIGALRM.sa_handler = &_platform_sig_handler;
  //sigaction(SIGVTALRM, &_signal_action_SIGALRM, NULL);
  sigaction(SIGALRM, &_signal_action_SIGALRM, NULL);

  _interval.it_value.tv_sec      = 0;
  _interval.it_value.tv_usec     = CONFIG_C3P_INTERVAL_PERIOD_MS * 1000;
  _interval.it_interval.tv_sec   = 0;
  _interval.it_interval.tv_usec  = CONFIG_C3P_INTERVAL_PERIOD_MS * 1000;

  int err = setitimer(ITIMER_REAL, &_interval, nullptr);
  if (err) {
    printf("Failed to enable interval timer.\n");
  }
  return (0 == err);
}


/*
* Used to tear down the periodic alarm.
*/
bool unset_linux_interval_timer() {
  _signal_action_SIGALRM.sa_handler = SIG_IGN;
  sigaction(SIGALRM, &_signal_action_SIGALRM, NULL);

  _interval.it_value.tv_sec      = 0;
  _interval.it_value.tv_usec     = 0;
  _interval.it_interval.tv_sec   = 0;
  _interval.it_interval.tv_usec  = 0;
  return true;
}


// /*
// * Parse through all the command line arguments and flags.
// * Return an Argument object.
// */
// Argument* parseFromArgCV(int argc, const char* argv[]) {
//   Argument*   _args    = new Argument(argv[0]);
//   _args->setKey("binary_name");
//   const char* last_key = nullptr;
//   for (int i = 1; i < argc; i++) {
//     if ((strcasestr(argv[i], "--version")) || ((argv[i][0] == '-') && (argv[i][1] == 'v'))) {
//       // Print the version and quit.
//       printf("%s v%s\n\n", argv[0], VERSION_STRING);
//       exit(0);
//     }
//
//     else if (((argv[i][0] == '-') && (argv[i][1] == '-'))) {
//       if (last_key) {
//         _args->link(new Argument((uint8_t) 1))->setKey(last_key);
//       }
//       last_key = (const char*) &argv[i][2];
//     }
//     else {
//       Argument* nu = new Argument(argv[i]);
//       if (last_key) {
//         nu->setKey(last_key);
//         last_key = nullptr;
//       }
//       _args->link(nu);
//     }
//   }
//
//   if (last_key) {
//     _args->link(new Argument((uint8_t) 1))->setKey(last_key);
//   }
//   return _args;
// }


/*******************************************************************************
* Watchdog                                                                     *
*******************************************************************************/
volatile uint32_t millis_since_reset = 1;   // Start at one because WWDG.
volatile uint8_t  watchdog_mark      = 42;


/*******************************************************************************
*     ______      __
*    / ____/___  / /__________  ____  __  __
*   / __/ / __ \/ __/ ___/ __ \/ __ \/ / / /
*  / /___/ / / / /_/ /  / /_/ / /_/ / /_/ /
* /_____/_/ /_/\__/_/   \____/ .___/\__, /
*                           /_/    /____/
*******************************************************************************/
volatile uint32_t randomness_pool[PLATFORM_RNG_CARRY_CAPACITY];
volatile unsigned int _random_pool_r_ptr = 0;
volatile unsigned int _random_pool_w_ptr = 0;

long unsigned int rng_thread_id = 0;

/**
* This is a thread to keep the randomness pool flush.
*/
static void* dev_urandom_reader(void*) {
  FILE* ur_file = fopen("/dev/urandom", "rb");
  unsigned int rng_level    = 0;
  unsigned int needed_count = 0;

  if (ur_file) {
    while (platform.platformState() <= MANUVR_INIT_STATE_NOMINAL) {
      rng_level = _random_pool_w_ptr - _random_pool_r_ptr;
      if (rng_level == PLATFORM_RNG_CARRY_CAPACITY) {
        // We have filled our entropy pool. Sleep.
        // TODO: Implement wakeThread() and this can go way higher.
        sleep_ms(10);
      }
      else {
        // We continue feeding the entropy pool as demanded until the platform
        //   leaves its nominal state. Don't write past the wrap-point.
        unsigned int _w_ptr = _random_pool_w_ptr % PLATFORM_RNG_CARRY_CAPACITY;
        unsigned int _delta_to_wrap = PLATFORM_RNG_CARRY_CAPACITY - _w_ptr;
        unsigned int _delta_to_fill = PLATFORM_RNG_CARRY_CAPACITY - rng_level;

        needed_count = ((_delta_to_fill < _delta_to_wrap) ? _delta_to_fill : _delta_to_wrap);
        size_t ret = fread((void*)&randomness_pool[_w_ptr], sizeof(uint32_t), needed_count, ur_file);
        //printf("Read %d uint32's from /dev/urandom.\n", ret);
        if((ret > 0) && !ferror(ur_file)) {
          _random_pool_w_ptr += ret;
        }
        else {
          fclose(ur_file);
          printf("Failed to read /dev/urandom.\n");
          return NULL;
        }
      }
    }
    fclose(ur_file);
  }
  else {
    printf("Failed to open /dev/urandom.\n");
  }
  printf("Exiting random thread.....\n");
  return NULL;
}


/**
* Dead-simple interface to the RNG. Despite the fact that it is interrupt-driven, we may resort
*   to polling if random demand exceeds random supply. So this may block until a random number
*   is actually availible (next_random_int != 0).
*
* @return   A 32-bit unsigned random number. This can be cast as needed.
*/
uint32_t randomUInt32() {
  // Preferably, we'd shunt to a PRNG at this point. For now we block.
  while (_random_pool_w_ptr <= _random_pool_r_ptr) {
  }
  return randomness_pool[_random_pool_r_ptr++ % PLATFORM_RNG_CARRY_CAPACITY];
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
void LinuxPlatform::_init_rng() {
  srand(time(nullptr));          // Seed the PRNG...
  if (createThread(&rng_thread_id, nullptr, dev_urandom_reader, nullptr, nullptr)) {
    printf("Failed to create RNG thread.\n");
    exit(-1);
  }
  int t_out = 30;
  while ((_random_pool_w_ptr < PLATFORM_RNG_CARRY_CAPACITY) && (t_out > 0)) {
    sleep_ms(20);
    t_out--;
  }
  if (0 == t_out) {
    printf("Failed to fill the RNG pool.\n");
    exit(-1);
  }
  _alter_flags(true, ABSTRACT_PF_FLAG_RNG_READY);
}


/*******************************************************************************
*     __                      _
*    / /   ____  ____ _____ _(_)___  ____ _
*   / /   / __ \/ __ `/ __ `/ / __ \/ __ `/
*  / /___/ /_/ / /_/ / /_/ / / / / / /_/ /
* /_____/\____/\__, /\__, /_/_/ /_/\__, /
*             /____//____/        /____/
*
* On vanilla linux, we will defer to the platform object's configuration and
*   either write to syslog, or STDIO via text-transform.
*******************************************************************************/

// // Returns 1 if we ought to be logging to the fp_log.
// //    Since this is our default logging target, we will response 'yes' even if the DB isn't loaded.
// int shouldLogToSyslog() {
//   int return_value    = conf.getConfigIntByKey("log-to-syslog");
//   if (return_value == -1) {
//     return_value    = 1;
//   }
//   return return_value;
// }

// int shouldLogToStdout() {
//   int return_value    = conf.getConfigIntByKey("log-to-stdout");
//   if (return_value == -1) {
//     return_value    = 0;
//   }
//   return return_value;
// }

bool shouldLogToSyslog() {  return false; }    // TODO: This
bool shouldLogToStdout() {  return true;  }    // TODO: This

/**
* This function is declared in CppPotpourri (AbstractPlatform.h).
* Log the given message.
*
* @param severity is the syslog-style importance of the message.
* @param tag is the free-form source of the message.
* @param msg contains the log content.
*/
void c3p_log(uint8_t severity, const char* tag, StringBuilder* msg) {
  bool log_disseminated = false;
  if (shouldLogToSyslog()) {
    syslog(severity, "%s", (char*) msg->string());
    log_disseminated    = true;
  }

  if (!log_disseminated || shouldLogToStdout()){
    printf("%s\n", msg->string());
  }
}



/*******************************************************************************
*     ____  __      __  ____                        ____  ____      __
*    / __ \/ /___ _/ /_/ __/___  _________ ___     / __ \/ __ )    / /
*   / /_/ / / __ `/ __/ /_/ __ \/ ___/ __ `__ \   / / / / __  |_  / /
*  / ____/ / /_/ / /_/ __/ /_/ / /  / / / / / /  / /_/ / /_/ / /_/ /
* /_/   /_/\__,_/\__/_/  \____/_/  /_/ /_/ /_/   \____/_____/\____/
*
* These are overrides and additions to the platform class.
*******************************************************************************/

void LinuxPlatform::printDebug(StringBuilder* output) {
  output->concatf(
    "==< Linux [%s] >=============================\n",
    _board_name
  );
  _print_abstract_debug(output);

  struct utsname sname;
  if (uname(&sname) != -1) {
    output->concatf("\t%s %s (%s)\n", sname.sysname, sname.release, sname.machine);
    output->concatf("\t%s\n", sname.version);
  }
  else {
    output->concat("\tFailed to get detailed kernel info.\n");
  }

  #if defined(__HAS_CRYPT_WRAPPER)
    output->concatf("-- Binary hash         %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      _binary_hash[0],  _binary_hash[1],  _binary_hash[2],  _binary_hash[3],
      _binary_hash[4],  _binary_hash[5],  _binary_hash[6],  _binary_hash[7],
      _binary_hash[8],  _binary_hash[9],  _binary_hash[10], _binary_hash[11],
      _binary_hash[12], _binary_hash[13], _binary_hash[14], _binary_hash[15]
    );
    output->concatf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
      _binary_hash[16], _binary_hash[17], _binary_hash[18], _binary_hash[19],
      _binary_hash[20], _binary_hash[21], _binary_hash[22], _binary_hash[23],
      _binary_hash[24], _binary_hash[25], _binary_hash[26], _binary_hash[27],
      _binary_hash[28], _binary_hash[29], _binary_hash[30], _binary_hash[31]
    );
    randomArt(_binary_hash, 32, "SHA256", output);
  #endif
}



/*******************************************************************************
*   _______                                   __   ____        __
*  /_  __(_)___ ___  ___     ____ _____  ____/ /  / __ \____ _/ /____
*   / / / / __ `__ \/ _ \   / __ `/ __ \/ __  /  / / / / __ `/ __/ _ \
*  / / / / / / / / /  __/  / /_/ / / / / /_/ /  / /_/ / /_/ / /_/  __/
* /_/ /_/_/ /_/ /_/\___/   \__,_/_/ /_/\__,_/  /_____/\__,_/\__/\___/
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

/**
* If the RTC is set to a time other than default, and at least equal to the
*   year the firmware was built, we assume it to be accurate.
* TODO: would be better to use a set bit in RTC memory, maybe...
*
* @param y   Year
* @param m   Month [1, 12]
* @param d   Day-of-month [1, 31]
* @param h   Hours [0, 23]
* @param mi  Minutes [0, 59]
* @param s   Seconds [0, 59]
* @return true on success.
*/
bool setTimeAndDate(uint8_t y, uint8_t m, uint8_t d, uint8_t wd, uint8_t h, uint8_t mi, uint8_t s) {
  return false;
}


/*
* Given an RFC2822 datetime string, decompose it and set the time and date.
* We would prefer RFC2822, but we should try and cope with things like missing
*   time or timezone.
*
* @return true on success.
*/
bool setTimeAndDateStr(char* nu_date_time) {
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
  ret = platform.rtcAccurate();
  return ret;
}


/*
* Returns an integer representing the current datetime.
*/
uint64_t epochTime() {
  struct timeval tv;
  return (0 == gettimeofday(&tv, nullptr)) ? 0 : tv.tv_sec;
}


/*
* Writes a human-readable datetime to the argument.
* Returns ISO 8601 datetime string.
* 2004-02-12T15:19:21+00:00
*/
void currentDateTime(StringBuilder* target) {
  if (target != nullptr) {
    time_t t = time(0);
    struct tm  tstruct;
    char       buf[64];
    tstruct = *localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    target->concat(buf);
  }
}


/*
* Not provided elsewhere on a linux platform.
*/
long unsigned millis() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000L);
}

/*
* Not provided elsewhere on a linux platform.
*/
long unsigned micros() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000000L + ts.tv_nsec / 1000L);
}


/* Delay functions */
void sleep_ms(uint32_t ms) {
  struct timespec t = {(long) (ms / 1000), (long) ((ms % 1000) * 1000000UL)};
  struct timespec r;
  if (0 != nanosleep(&t, &r)) {
    while (0 != nanosleep(&r, &r)) {}
  }
}

void sleep_us(uint32_t us) {
  struct timespec t = {(long) (us / 1000000), (long) ((us % 1000000) * 1000000UL)};
  struct timespec r;
  if (0 != nanosleep(&t, &r)) {
    while (0 != nanosleep(&r, &r)) {}
  }
}


/*******************************************************************************
* Threading                                                                    *
*******************************************************************************/
/**
* On linux, we support pthreads. On microcontrollers, we support FreeRTOS.
* This is the wrapper to create a new thread.
*
* @return The thread's return value.
*/
int LinuxPlatform::createThread(unsigned long* _thread_id, void* _something, ThreadFxnPtr _fxn, void* _args, PlatformThreadOpts* _thread_opts) {
  return pthread_create(_thread_id, (const pthread_attr_t*) _something, _fxn, _args);
}

int LinuxPlatform::deleteThread(unsigned long* _thread_id) {
  return pthread_cancel(*_thread_id);
}

int LinuxPlatform::wakeThread(unsigned long _thread_id) {
  return 0;
}


/*******************************************************************************
* Persistent configuration                                                     *
*******************************************************************************/
#if defined(CONFIG_C3P_STORAGE)
  // Called during boot to load configuration.
  int8_t LinuxPlatform::_load_config() {
    if (_storage_device) {
      if (_storage_device->isMounted()) {
        uint len = 2048;
        uint8_t raw[len];
        StorageErr err = _storage_device->persistentRead(NULL, raw, &len, 0);
        if (err == StorageErr::NONE) {
          _config = Argument::decodeFromCBOR(raw, len);
          if (_config) {
            return 0;
          }
        }
      }
    }
    return -1;
  }
#endif



/*******************************************************************************
*     ____  _          ______            __             __
*    / __ \(_)___     / ____/___  ____  / /__________  / /
*   / /_/ / / __ \   / /   / __ \/ __ \/ __/ ___/ __ \/ /
*  / ____/ / / / /  / /___/ /_/ / / / / /_/ /  / /_/ / /
* /_/   /_/_/ /_/   \____/\____/_/ /_/\__/_/   \____/_/
*******************************************************************************/
/*
* TODO: Support this via sysfs.
* Weak reference to allow override by board-specific support that would be
*   higher-reliability than reading the sysfs interface.
*/


/*******************************************************************************
* Process control                                                              *
*******************************************************************************/
void LinuxPlatform::_close_open_threads() {
  unset_linux_interval_timer();   // Stop the periodic alarm.
  //_set_init_state(MANUVR_INIT_STATE_HALTED);
  if (rng_thread_id) {
    if (0 == deleteThread(&rng_thread_id)) {
    }
  }

  #if defined(__HAS_CRYPT_WRAPPER)
  if (crypto_thread_id) {
    if (0 == crypto->deinit()) {
      // Orderly thread termination is preferable.
      while (0 != crypto_thread_id) {
        sleep_ms(10);
      }
    }
    else if (0 == deleteThread(&crypto_thread_id)) {
    }
  }
  #endif  // __HAS_CRYPT_WRAPPER

  sleep_ms(10);
}


// TODO: This
uint8_t last_restart_reason() {  return 0;  }


/*
* Terminate this running process, along with any children it may have forked() off.
*/
void LinuxPlatform::firmware_reset(uint8_t reason) {
  // TODO: If reason is update, pull binary from a location of firmware's choice,
  //   install firmware after validation, and schedule a program restart.
  #if defined(CONFIG_C3P_STORAGE)
    if (_self && _self->isDirty()) {
      // Save the dirty identity.
      // TODO: int8_t persistentWrite(const char*, uint8_t*, int, uint16_t);
    }
  #endif
  // Whatever the kernel cared to clean up, it better have done so by this point,
  //   as no other platforms return from this function.
  printf("\nfirmware_reset(%d): About to exit().\n\n", reason);
  _close_open_threads();
  exit(0);
}

/*
* On linux, we take this to mean: scheule a program restart with the OS,
*   and then terminate this one.
*/
void LinuxPlatform::firmware_shutdown(uint8_t reason) {
  printf("\nfirmware_shutdown(%d): About to exit().\n\n", reason);
  _close_open_threads();
  exit(0);
}


/*******************************************************************************
* Cryptographic                                                                *
*******************************************************************************/
#if defined(__HAS_CRYPT_WRAPPER)

/**
* This is a thread to keep the randomness pool flush.
*/
static void* crypto_processor_thread(void*) {
  printf("Starting CryptoProcessor thread...\n");
  CryptoProcessor* cproc = platformObj()->crypto;
  while (cproc->initialized()) {
    cproc->poll();
  }
  printf("Exiting CryptoProcessor thread...\n");
  crypto_thread_id = 0;
  return nullptr;
}


int8_t LinuxPlatform::internal_integrity_check(uint8_t* test_buf, int test_len) {
  if ((nullptr != test_buf) && (0 < test_len)) {
    for (int i = 0; i < test_len; i++) {
      if (*(test_buf+i) != _binary_hash[i]) {
        printf("Hashing %s yields a different value than expected. Exiting...\n", _binary_name);
        return -1;
      }
    }
    return 0;
  }
  else {
    // We have no idea what to expect. First boot?
  }
  return -1;
}


/**
* Look in the mirror and find our executable's full path.
* Then, read the full contents and hash them. Store the result in
*   local private member _binary_hash.
*
* @return 0 on success.
*/
int8_t LinuxPlatform::_hash_self() {
  char *exe_path = (char *) alloca(300);   // 300 bytes ought to be enough for our path info...
  memset(exe_path, 0x00, 300);
  int exe_path_len = readlink("/proc/self/exe", exe_path, 300);
  if (!(exe_path_len > 0)) {
    printf("%s was unable to read its own path from /proc/self/exe. You may be running it on an unsupported operating system, or be running an old kernel. Please discover the cause and retry. Exiting...\n", _binary_name);
    return -1;
  }
  printf("This binary's path is %s\n", exe_path);
  memset(_binary_hash, 0x00, 32);
  int ret = 1; //hashFileByPath(exe_path, _binary_hash);
  if (0 < ret) {
    return 0;
  }
  else {
    printf("Failed to hash file: %s\n", exe_path);
  }
  return -1;
}


#endif  // __HAS_CRYPT_WRAPPER




/******************************************************************************
* Platform initialization.                                                    *
******************************************************************************/
#define  DEFAULT_PLATFORM_FLAGS ( \
              ABSTRACT_PF_FLAG_INNATE_DATETIME | \
              ABSTRACT_PF_FLAG_HAS_IDENTITY)

/**
* Init that needs to happen prior to kernel bootstrap().
* This is the final function called by the kernel constructor.
*/
int8_t LinuxPlatform::init() {
  //LinuxPlatform::platformPreInit(root_config);
  // Used for timer and signal callbacks.
  _discover_alu_params();

  // Bind to the signals that the platform handles.
  if (signal(SIGINT, _platform_sig_handler) == SIG_ERR) {
    printf("Failed to bind SIGINT to the signal system. Failing...");
    return -1;
  }
  if (signal(SIGTERM, _platform_sig_handler) == SIG_ERR) {
    printf("Failed to bind SIGTERM to the signal system. Failing...");
    return -1;
  }

  uint32_t default_flags = DEFAULT_PLATFORM_FLAGS;
  _main_pid = getpid();  // Our PID.
  _alter_flags(true, default_flags);

  _init_rng();
  _alter_flags(true, ABSTRACT_PF_FLAG_RTC_READY);

  #if defined(CONFIG_C3P_STORAGE)
    LinuxStorage* sd = new LinuxStorage(root_config);
    _storage_device = (Storage*) sd;
  #endif

  set_linux_interval_timer();

  #if defined(__HAS_CRYPT_WRAPPER)
    crypto = new CryptoProcessor(CONFIG_C3P_CRYPTO_QUEUE_MAX_DEPTH);
    if (nullptr != crypto) {
      if (0 != crypto->init()) {
        return -3;
      }
      if (createThread(&crypto_thread_id, nullptr, crypto_processor_thread, nullptr, nullptr)) {
        printf("Failed to create crypto thread.\n");
        return -4;
      }
      _hash_self();
      //internal_integrity_check(nullptr, 0);
    }
    else {
      return -2;
    }
  #endif // __HAS_CRYPT_WRAPPER

  return 0;
}
