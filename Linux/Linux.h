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
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include <AbstractPlatform.h>

#if defined(CONFIG_MANUVR_STORAGE)
  #include "LinuxStorage.h"
#endif

#if defined(__MACH__) && defined(__APPLE__)
  typedef unsigned long pthread_t;
#endif


// If we were built with CryptoBurrito, we have some additional ratchet-straps.
#if defined(__HAS_CRYPT_WRAPPER)
  int8_t internal_integrity_check(uint8_t* test_buf, int test_len);
  int8_t hash_self();
#endif

void   init_rng();
void _close_open_threads();
int8_t _load_config();       // Called during boot to load configuration.



#endif  // __PLATFORM_VANILLA_LINUX_H__
