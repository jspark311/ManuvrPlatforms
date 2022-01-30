/*
File:   ESP32Storage.h
Author: J. Ian Lindsay
Date:   2016.09.05

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


Data-persistence layer for Teensy.
*/

#ifndef __MANUVR_ESP32_STORAGE_H__
#define __MANUVR_ESP32_STORAGE_H__

#include <AbstractPlatform.h>
#include <Storage.h>
#include "nvs_flash.h"
#include "nvs.h"


class ESP32Storage : public Storage {
  public:
    ESP32Storage(const esp_partition_t*);
    ~ESP32Storage();

    /* Overrides from Storage. */
    uint64_t   freeSpace();  // How many bytes are availible for use?
    StorageErr init();
    StorageErr wipe(uint32_t offset, uint32_t len);  // Wipe a range.
    uint8_t    blockAddrSize() {  return DEV_ADDR_SIZE_BYTES;  };
    int8_t     allocateBlocksForLength(uint32_t, DataRecord*);

    StorageErr flush();          // Blocks until commit completes.

    StorageErr persistentWrite(DataRecord*, StringBuilder* buf);
    //StorageErr persistentRead(DataRecord*, StringBuilder* buf);
    StorageErr persistentWrite(uint8_t* buf, unsigned int len, uint32_t offset);
    StorageErr persistentRead(uint8_t* buf,  unsigned int len, uint32_t offset);

    void printDebug(StringBuilder*);


  private:
    const esp_partition_t* _PART_PTR;
    nvs_handle store_handle;

    int8_t close();             // Blocks until commit completes.
};

#endif // __MANUVR_ESP32_STORAGE_H__
