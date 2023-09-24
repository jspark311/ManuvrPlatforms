/*
File:   LinuxSocketPipe.cpp
Author: J. Ian Lindsay
Date:   2015.03.17

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


This is a BufferPipe that abstracts a unix socket.
*/


#include <Linux.h>

#if defined(CONFIG_C3P_SOCKET_WRAPPER)

#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>


/*******************************************************************************
*      _______.___________.    ___   .___________. __    ______     _______.
*     /       |           |   /   \  |           ||  |  /      |   /       |
*    |   (----`---|  |----`  /  ^  \ `---|  |----`|  | |  ,----'  |   (----`
*     \   \       |  |      /  /_\  \    |  |     |  | |  |        \   \
* .----)   |      |  |     /  _____  \   |  |     |  | |  `----.----)   |
* |_______/       |__|    /__/     \__\  |__|     |__|  \______|_______/
*
* Static members and initializers should be located here.
*******************************************************************************/
static unsigned long _sock_polling_thread_id = 0;
static LinkedList<LinuxSockPipe*> sock_instances;


/**
* This is a thread to keep the sockets churning.
*/
static void* socket_polling_handler(void*) {
  bool keep_polling = true;
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Started socket polling thread.");
  while (keep_polling) {
    sleep_ms(20);
    for (int i = 0; i < sock_instances.size(); i++) {
      LinuxSockPipe* temp = sock_instances.get(i);
      if (nullptr != temp) {
        temp->poll();
      }
    }
    keep_polling = (0 < sock_instances.size());
  }
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Exiting socket polling thread...");
  _sock_polling_thread_id = 0;  // Allow the thread to be restarted later.
  return NULL;
}


/*******************************************************************************
* Socket wrapper class
*******************************************************************************/

/**
* Constructor will allocate memory for its lookup list item and the path string.
* Adds itself to the lookup list.
*
* NOTE: Because this constructor modifies static data, it can't be relied upon
*   if an instance of it is ever allocated statically. So don't do that.
*/
LinuxSockPipe::LinuxSockPipe(char* path, int sock_id) : LinuxSockPipe(path) {
  _sock_id = sock_id;
}

/**
* Constructor will allocate memory for its lookup list item and the path string.
* Adds itself to the lookup list.
*
* NOTE: Because this constructor modifies static data, it can't be relied upon
*   if an instance of it is ever allocated statically. So don't do that.
*/
LinuxSockPipe::LinuxSockPipe(char* path) {
  _set_sock_path(path);
  sock_instances.insert(this);
}


/**
* Destructor will de-init the hardware, remove itself from the lookup list,
*   and free any memory it used to store itself.
*/
LinuxSockPipe::~LinuxSockPipe() {
  close();
  sock_instances.remove(this);
  if (_sock_path) {
    free(_sock_path);
    _sock_path = nullptr;
  }
}


/*******************************************************************************
* Implementation of BufferAccepter
*******************************************************************************/


/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 or greater on success.
*/
int8_t LinuxSockPipe::poll() {
  int8_t ret = 0;
  if (true) {
    if (_sock_id > 0) {
      if (0 < _tx_buffer.length()) {
        // Refill the TX buffer...
        int tx_count = strict_min((int32_t) 64, (int32_t) _tx_buffer.length());
        if (0 < tx_count) {
          int bytes_written = (int) ::write(_sock_id, _tx_buffer.string(), tx_count);
          _tx_buffer.cull(bytes_written);
          _count_tx += bytes_written;
        }
      }
      uint8_t* buf = (uint8_t*) alloca(255);
      int n = ::read(_sock_id, buf, 255);
      if (n > 0) {
        _rx_buffer.concat(buf, n);
        _count_rx += n;
        _last_rx_ms = millis();
        ret = 1;
      }
      if (0 < _rx_buffer.length()) {
        if (nullptr != _read_cb_obj) {
          if (0 == _read_cb_obj->pushBuffer(&_rx_buffer)) {
            _rx_buffer.clear();
          }
        }
      }
    }
  }
  return ret;
}


int8_t LinuxSockPipe::_open() {
  int8_t ret = -1;
  if (true) {
    if (0 < _sock_id) {
      ::close(_sock_id);
      _sock_id = 0;
    }
    _sock_id = open(_sock_path, O_RDWR | O_NOCTTY | O_SYNC);
    if (_sock_id != -1) {
      c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Opened socket (%s)", _sock_path);
      if (0 == _sock_polling_thread_id) {
        platform.createThread(&_sock_polling_thread_id, nullptr, socket_polling_handler, nullptr, nullptr);
      }
      ret = 0;
    }
    else {
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unable to open port: (%s)", _sock_path);
    }
  }
  return ret;
}


int8_t LinuxSockPipe::close() {
  int8_t ret = -1;
  if (0 < _sock_id) {
    ::close(_sock_id);  // Close the socket.
    c3p_log(LOG_LEV_NOTICE, __PRETTY_FUNCTION__, "Closed socket %d (%s)", _sock_id, ((_sock_path) ? _sock_path : "no path"));
    _sock_id = -1;
    ret = 0;
  }
  _tx_buffer.clear();
  _rx_buffer.clear();
  return ret;
}


/*
* Write to the socket.
*/
uint LinuxSockPipe::write(uint8_t* buf, uint len) {
  _tx_buffer.concat(buf, len);
  return len;  // TODO: StringBuilder needs an API enhancement to make this safe.
}


/*
* Write to the socket.
*/
uint LinuxSockPipe::write(char c) {
  _tx_buffer.concat(c);
  return 1;  // TODO: StringBuilder needs an API enhancement to make this safe.
}


/*
* Read from the socket buffer.
*/
uint LinuxSockPipe::read(uint8_t* buf, uint len) {
  uint ret = strict_min((int32_t) len, (int32_t) _rx_buffer.length());
  if (0 < ret) {
    memcpy(buf, _rx_buffer.string(), ret);
    _rx_buffer.cull(ret);
  }
  return ret;
}


/*
* Read from the socket buffer.
*/
uint LinuxSockPipe::read(StringBuilder* buf) {
  uint ret = _rx_buffer.length();
  if (0 < ret) {
    buf->concatHandoff(&_rx_buffer);
  }
  return ret;
}



// Returns the number of connections, or -1 if not open.
int LinuxSockPipe::connected() {
  int ret = 0;
  if (_sock_id > 0) {
    ret++;
  }
  return ret;
}


int LinuxSockPipe::connect(char* path) {
  int ret = 0;

  if (!connected()) {
    char* conn_sock = _sock_path;
    if (nullptr != path) {
      if (0 != _set_sock_path(path)) {
        conn_sock = (char*) "";
      }
    }
    else if (nullptr == _sock_path) {
      conn_sock = (char*) "";
    }

    if (0 < strlen(conn_sock)) {   // We have something to work with.
      ret = _open();
    }
    else {  // No action is possible. We haven't been given a path.
      ret = -1;
    }
  }

  if (_sock_id > 0) {  ret++;    }
  return ret;
}



int8_t LinuxSockPipe::_set_sock_path(char* path) {
  int8_t ret = -1;
  if (!connected()) {
    ret--;
    if (path) {
      ret--;
      const int slen = strlen(path);
      if (slen > 0) {
        ret--;
        if (_sock_path) {
          if (0 != StringBuilder::strcasecmp(path, _sock_path)) {   // TODO: should be a case-sensitive test.
            free(_sock_path);
            _sock_path = nullptr;
          }
          else {
            // The given path is the same as what we already have. Do nothing.
            return 0;
          }
        }

        _sock_path = (char*) malloc(slen+1);
        if (_sock_path) {
          memcpy(_sock_path, path, slen);
          *(_sock_path + slen) = '\0';
          ret = 0;
        }
      }
    }
  }
  return ret;
}


void LinuxSockPipe::printDebug(StringBuilder* output) {
  StringBuilder temp("Socket");
  temp.concatf("%s (%sopen", _sock_path, ((0 < connected()) ? "":"not "));
  if (0 < connected()) {
    temp.concatf(": %d)", _sock_id);
  }
  else {
    temp.concat(")");
  }
  StringBuilder::styleHeader1(output, (char*) temp.string());
  output->concatf("\tBytes tx/rx:\t%u / %u\n",      _count_tx, _count_rx);
}


/*******************************************************************************
* Console callback
* These are built-in handlers for using this instance via a console.
*******************************************************************************/

/**
* @page console-handlers
* @section socket-tools Socket tools
*
* This is the console handler for debugging the operation of `LinuxSockPipe`'s.
*
*/
int8_t LinuxSockPipe::console_handler(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);

  if (0 == StringBuilder::strcasecmp(cmd, "connect")) {
    int ret_local = 0;
    if (args->count() > 1) {
      ret_local = connect(args->position_trimmed(1));
    }
    else {
      ret_local = connect();
    }
    text_return->concatf("connect(%s) returned %d\n", _sock_path, ret_local);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "close")) {
    text_return->concatf("close() returned %d\n", close());
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "poll")) {
    text_return->concatf("poll() returned %d\n", poll());
  }
  else {
    printDebug(text_return);
  }

  return ret;
}

#endif   // CONFIG_C3P_SOCKET_WRAPPER
