/*
File:   LinuxStorage.h
Author: J. Ian Lindsay
Date:   2016.08.28

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


Data-persistence layer for linux.
Implemented as a CBOR object within a single file. This feature therefore
  requires CONFIG_C3P_CBOR. In the future, it may be made to operate on some
  other encoding, be run through a cryptographic pipe, etc.
*/

#ifndef __MANUVR_LINUX_STORAGE_H__
#define __MANUVR_LINUX_STORAGE_H__

#include "Storage/Storage.h"

class C3PFile {
  public:
    C3PFile(char*);
    ~C3PFile();

    inline char* path() {             return _path;              };
    inline bool  exists() {           return _exists;            };
    inline bool  isDirectory() {      return _is_dir;            };
    inline bool  isFile() {           return _is_file;           };
    inline bool  isLink() {           return _is_link;           };
    inline bool  closelyExamined() {  return _closely_examined;  };

    int32_t read(StringBuilder* buf);
    int32_t write(StringBuilder* buf);


    void printDebug(StringBuilder*);


  private:
    char    _mode[12];
    char*   _path    = nullptr;
    ulong   _fsize   = 0;
    uid_t   _uid     = 0;
    gid_t   _gid     = 0;
    time_t  _ctime;
    time_t  _mtime;
    bool    _exists  = false;
    bool    _is_dir  = false;
    bool    _is_file = false;
    bool    _is_link = false;
    bool    _closely_examined = false;

    int _fill_from_stat();
};


//class LinuxFileStorage : public Storage {
//  public:
//    LinuxStorage(KeyValuePair*);
//    ~LinuxStorage();
//
//    /* Overrides from Storage. */
//    uint64_t   freeSpace();     // How many bytes are availible for use?
//    StorageErr wipe();          // Call to wipe the data store.
//    //StorageErr flush();             // Blocks until commit completes.
//
//    /* Raw buffer API. Might have more overhead on some platforms. */
//    StorageErr persistentWrite(const char*, uint8_t*, unsigned int, uint16_t);
//    StorageErr persistentRead(const char*, uint8_t*, unsigned int*, uint16_t);
//
//    /* StringBuilder API to avoid pedantic copying. */
//    StorageErr persistentWrite(const char*, StringBuilder*, uint16_t);
//    StorageErr persistentRead(const char*, StringBuilder*, uint16_t);
//
//    void printDebug(StringBuilder*);
//
//
//  protected:
//    int8_t attached();
//
//
//  private:
//    char*          _filename   = nullptr;
//    StringBuilder  _disk_buffer;
//
//    StorageErr _save_file(StringBuilder* b);
//    StorageErr _load_file(StringBuilder* b);
//};

#endif // __MANUVR_LINUX_STORAGE_H__
