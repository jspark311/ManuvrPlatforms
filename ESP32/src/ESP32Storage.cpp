/*
File:   ESP32Storage.cpp
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


Implemented as a CBOR object within NVS. This feature therefore
  requires CONFIG_C3P_CBOR. In the future, it may be made to operate on some
  other encoding, be run through a cryptographic pipe, etc.

CBOR data begins at offset 4. The first uint32 is broken up this way:
  Offset  |
  --------|----------------
  0       | Magic number (0x7A)
  1       | Magic number (0xB7)
  2       | Bits[7..4] Zero
          | Bits[3..0] LSB of free-space (max, 2044)
  3       | LSB of free-space (max, 2044)


Noteworth snippit from the ESP-IDF doc:
> if an NVS partition is truncated (for example, when the partition table layout
> is changed), its contents should be erased. ESP-IDF build system provides a
> idf.py erase_flash target to erase all contents of the flash chip.

*/
#include "../ESP32.h"

#if defined(CONFIG_C3P_STORAGE)

#if !defined(CONFIG_C3P_CBOR)
  #error Enabling the storage abstraction requires CONFIG_C3P_CBOR.
#endif

// We want this definition isolated to the compilation unit.
#define STORAGE_PROPS (PL_FLAG_BLOCK_ACCESS | PL_FLAG_MEDIUM_READABLE | PL_FLAG_MEDIUM_WRITABLE)

#if !defined(CONFIG_C3P_STORAGE_BLK_SIZE)
  #define CONFIG_C3P_STORAGE_BLK_SIZE  256
#endif

/*******************************************************************************
*   ___ _              ___      _ _              _      _
*  / __| |__ _ ______ | _ ) ___(_) |___ _ _ _ __| |__ _| |_ ___
* | (__| / _` (_-<_-< | _ \/ _ \ | / -_) '_| '_ \ / _` |  _/ -_)
*  \___|_\__,_/__/__/ |___/\___/_|_\___|_| | .__/_\__,_|\__\___|
*                                          |_|
* Constructors/destructors, class initialization functions and so-forth...
*******************************************************************************/

static ESP32Storage* INSTANCE = nullptr;

ESP32Storage::ESP32Storage(const esp_partition_t* NVS_PART_PTR)
  : Storage(NVS_PART_PTR->size, CONFIG_C3P_STORAGE_BLK_SIZE), _PART_PTR(NVS_PART_PTR) {
  INSTANCE = this;
}


ESP32Storage::~ESP32Storage() {
  _close();
}


StorageErr ESP32Storage::init() {
  StorageErr ret = StorageErr::UNSPECIFIED;
  esp_err_t err = nvs_flash_init();   // Initialize NVS.
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // OTA app partition table has a smaller NVS partition size than the non-OTA
    // partition table. This size mismatch may cause NVS initialization to fail.
    // If this happens, we erase NVS partition and initialize NVS again.
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  if (err == ESP_OK) {
    _pl_set_flag(STORAGE_PROPS | PL_FLAG_MEDIUM_MOUNTED);
    ret = StorageErr::NONE;
  }
  return ret;
}


/*******************************************************************************
*  __  ______   ___   ____   ___    ___   ____
* (( \ | || |  // \\  || \\ // \\  // \\ ||
*  \\    ||   ((   )) ||_// ||=|| (( ___ ||==
* \_))   ||    \\_//  || \\ || ||  \\_|| ||___
*
* Storage interface.
********************************************************************************/
uint64_t ESP32Storage::freeSpace() {
  return _free_space;
}

StorageErr ESP32Storage::wipe(uint32_t offset, uint32_t len) {
  StorageErr ret = StorageErr::NOT_MOUNTED;
  if (isMounted()) {
    ret = StorageErr::NOT_WRITABLE;
    if (isWritable()) {
      ret = StorageErr::HW_FAULT;
      if (ESP_OK == esp_partition_erase_range(_PART_PTR, offset, len)) {
        ret = StorageErr::NONE;
      }
    }
  }
  return ret;
}


StorageErr ESP32Storage::flush() {
  StorageErr ret = StorageErr::NOT_MOUNTED;
  if (isMounted()) {
    ret = StorageErr::NOT_WRITABLE;
    if (isWritable()) {
      ret = StorageErr::HW_FAULT;
      //if (ESP_OK == nvs_commit(store_handle)) {
        ret = StorageErr::NONE;
      //}
    }
  }
  return ret;
}


StorageErr ESP32Storage::persistentWrite(uint8_t* buf, unsigned int len, uint32_t offset) {
  StorageErr ret = StorageErr::NOT_MOUNTED;
  if (isMounted()) {
    ret = StorageErr::NOT_WRITABLE;
    if (isWritable()) {
      ret = StorageErr::NO_FREE_SPACE;
      if (freeSpace() >= len) {
        ret = StorageErr::HW_FAULT;
        //if (ESP_OK == nvs_set_blob(store_handle, key, (const void*) buf, len)) {
          ret = StorageErr::NONE;
        //}
      }
    }
  }
  return ret;
}


StorageErr ESP32Storage::persistentRead(uint8_t* buf, unsigned int len, uint32_t offset) {
  StorageErr ret = StorageErr::NOT_MOUNTED;
  if (isMounted()) {
    ret = StorageErr::NOT_READABLE;
    if (isReadable()) {
      ret = StorageErr::BAD_PARAM;
      if (0 < len) {
        size_t nvs_len = 0;
        ret = StorageErr::KEY_NOT_FOUND;
        //if (ESP_OK == nvs_get_blob(store_handle, key, (void*) buf, &nvs_len)) {
        //  if (nvs_len <= *len) {
        //    ret = StorageErr::HW_FAULT;
        //    if (ESP_OK == nvs_get_blob(store_handle, key, (void*) buf, &nvs_len)) {
        //      *len = nvs_len;
        //      ret = StorageErr::NONE;
        //    }
        //  }
        //}
      }
    }
  }
  return ret;
}


StorageErr ESP32Storage::persistentWrite(DataRecord*, StringBuilder* buf) {
  StorageErr ret = StorageErr::NOT_MOUNTED;
  if (isMounted()) {
    ret = StorageErr::NOT_WRITABLE;
    if (isWritable()) {
      ret = StorageErr::NO_FREE_SPACE;
      if (freeSpace() >= buf->length()) {
        ret = StorageErr::HW_FAULT;
        //if (ESP_OK == nvs_set_blob(store_handle, key, (const void*) out->string(), out->length())) {
          ret = StorageErr::NONE;
        //}
      }
    }
  }
  return ret;
}



/**
* Debug support method. This fxn is only present in debug builds.
*
* @param   StringBuilder* The buffer into which this fxn should write its output.
*/
void ESP32Storage::printDebug(StringBuilder* output) {
  _print_storage(output);
}


/**
*
*
* @return
*/
int8_t ESP32Storage::_close() {
  if (isMounted()) {
    if (StorageErr::NONE == flush()) {
      //nvs_close(store_handle);
      _pl_set_flag(PL_FLAG_MEDIUM_MOUNTED);
      return 0;
    }
  }
  return -1;
}


int8_t ESP32Storage::allocateBlocksForLength(uint32_t len, DataRecord* rec) {
  int8_t ret = -1;
  //if (0 == _busy_check()) {
  //  const uint BLOCKS_NEEDED = (len / PAYLOAD_SIZE_BYTES) + ((0 == (len % PAYLOAD_SIZE_BYTES)) ? 0:1);
  //  LinkedList<StorageBlock*>* blocks = rec->getBlockList();
  //  uint blocks_found = blocks->size();
  //  ret--;

  //  if (BLOCKS_NEEDED > blocks_found) {    // Present allocation is too small.
  //    ret--;
  //    if (_free_space >= ((BLOCKS_NEEDED - blocks_found) * DEV_BLOCK_SIZE)) {
  //      // If we have enough free space to allocate the extra needed space, do so.
  //      ret--;
  //      uint i = 1;
  //      uint32_t blk_addr  = 0;
  //      bool loop_continue = true;
  //      while (loop_continue) {
  //        // There is no block holding the root record. Find one from low-to-high.
  //        const uint32_t TEST_BLK_ADDR = i * DEV_BLOCK_SIZE;
  //        if (!_is_block_allocated(i)) {
  //          _mark_block_allocated(i, true);
  //          blocks_found++;
  //          if (0 == blk_addr) {
  //            blk_addr = TEST_BLK_ADDR;
  //          }
  //          else {
  //            blocks->insert(new StorageBlock(blk_addr, TEST_BLK_ADDR));
  //            blk_addr = TEST_BLK_ADDR;
  //          }
  //        }
  //        i++;
  //        loop_continue = (blocks_found < BLOCKS_NEEDED) & (i != 0) & (i != DEV_TOTAL_BLOCKS);
  //      }
  //      if (0 != blk_addr) {
  //        blocks->insert(new StorageBlock(blk_addr, 0));
  //        ret = 0;
  //      }
  //    }
  //  }
  //  else if (BLOCKS_NEEDED < blocks_found) {    // Present allocation is too large.
  //    // TODO: add extra blocks to trim list.
  //    ret = 0;
  //  }
  //  else {  // Allocation is ok as it is.
  //    ret = 0;
  //  }
  //}
  return ret;
}



int console_callback_esp_storage(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd    = args->position_trimmed(0);
  char* subcmd = args->position_trimmed(1);

  if (0 == StringBuilder::strcasecmp(cmd, "info")) {
    INSTANCE->printDebug(text_return);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "init")) {
    text_return->concatf("Storage init returns %d.\n", (int8_t) INSTANCE->init());
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "wipe")) {
    bool print_fail = true;
    if (2 == args->count()) {
      if (0 == StringBuilder::strcasecmp(subcmd, "yes")) {
        print_fail = false;
        text_return->concatf("Storage wipe returns %d.\n", (int8_t) ((Storage*) INSTANCE)->wipe());
      }
    }
    if (print_fail) {
      text_return->concat("You must issue \"wipe yes\" to confirm.\n");
    }
  }
  else {
    ret = -1;
  }
  return ret;
}

#endif   // CONFIG_C3P_STORAGE
