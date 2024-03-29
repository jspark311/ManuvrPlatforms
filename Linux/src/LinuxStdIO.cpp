/*
File:   LinuxStdIO.cpp
Author: J. Ian Lindsay
Date:   2016.07.23

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


LinuxStdIO is the driver for wrapping STDIN/STDOUT/STDERR into a BufferAccepter.
*/

#include "Linux.h"

/*******************************************************************************
*   ___ _              ___      _ _              _      _
*  / __| |__ _ ______ | _ ) ___(_) |___ _ _ _ __| |__ _| |_ ___
* | (__| / _` (_-<_-< | _ \/ _ \ | / -_) '_| '_ \ / _` |  _/ -_)
*  \___|_\__,_/__/__/ |___/\___/_|_\___|_| | .__/_\__,_|\__\___|
*                                          |_|
* Constructors/destructors, class initialization functions and so-forth...
*******************************************************************************/
/**
* Constructor.
*/
LinuxStdIO::LinuxStdIO() {
}

/**
* Destructor
*/
LinuxStdIO::~LinuxStdIO() {
}


/**
* Write output to STDOUT, and read input from STDIN.
*/
int8_t LinuxStdIO::poll() {
  int read_len = 0;

  while (_tx_buffer.count()) {
    const char* working_chunk = (const char*) _tx_buffer.position(0);
    printf("%s", working_chunk);
    _tx_buffer.drop_position(0);
  }
  fflush(stdout);

  // If there is an object to feed the
  char input_text[256];    // Buffer to hold user-input.
  bzero(input_text, sizeof(input_text));
  if (nullptr != fgets(input_text, (sizeof(input_text)-1), stdin)) {
    read_len = strlen(input_text);
    // NOTE: This should suffice to be binary-safe.
    //read_len = fread(input_text, 1, getMTU(), stdin);
    if (nullptr != _read_cb_obj) {
      if (read_len > 0) {
        _rx_buffer.concat((uint8_t*) input_text, read_len);
      }
      if (0 < _rx_buffer.length()) {
        if (0 == _read_cb_obj->pushBuffer(&_rx_buffer)) {
          _rx_buffer.clear();
        }
      }
    }
  }
  return read_len;
}
