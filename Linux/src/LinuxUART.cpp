/*
File:   ManuvrSerial.cpp
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


This driver fills out the platform-abstracted UARTAdapter class in
  CppPotpourri. On linux, that means /dev/tty<x>.

Platforms that require it should be able to extend this driver for specific
  kinds of hardware support.
*/


#include <Linux.h>

// Linux requires these libraries for serial port.
#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <fstream>
#include <termios.h>


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

int           _sock      = 0;
unsigned long _thread_id = 0;
char*         dev_path   = nullptr;
struct termios termAttr;


UARTAdapter* uart_instances[] = {nullptr, nullptr, nullptr};


/**
* This is a thread to keep the randomness pool flush.
*/
static void* uart_polling_handler(void*) {
  bool keep_polling = true;
  while (keep_polling) {
    uart_instances[0]->poll();
    keep_polling = (nullptr != uart_instances[0]);
  }
  printf("Exiting UART polling thread...\n");
  return NULL;
}


/*
* In-class ISR handler. Be careful about state mutation....
*/
void UARTAdapter::irq_handler() {
}


/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 on success.
*/
int8_t UARTAdapter::poll() {
  int8_t return_value = 0;
  int bytes_written = 0;
  int bytes_received = 0;

  if (txCapable() && (0 < _tx_buffer.length())) {
    // Refill the TX buffer...
    int tx_count = strict_min((int32_t) 64, (int32_t) _tx_buffer.length());
    if (0 < tx_count) {
      switch (ADAPTER_NUM) {
        case 0:
          if (_sock == -1) {
            #ifdef MANUVR_DEBUG
              if (getVerbosity() > 2) {
                local_log.concatf("Unable to write to transport: %s\n", dev_path);
                Kernel::log(&local_log);
              }
            #endif
            return false;
          }
          bytes_written = (int) ::write(_sock, _tx_buffer.string(), tx_count);
          _tx_buffer.cull(bytes_written);
          _adapter_set_flag(UART_FLAG_FLUSHED, _tx_buffer.isEmpty());
          break;
      }
    }
  }
  if (rxCapable()) {
    uint8_t* buf = (uint8_t*) alloca(255);
    switch (ADAPTER_NUM) {
      case 0:
        int n = ::read(_sock, buf, 255);
        //size_t n = fread(buf, 1, 255, _sock);
        if (n > 0) {
          bytes_received += n;
          _rx_buffer.concat(buf, n);
          return_value = 1;
        }
        break;
    }
    if (0 < _rx_buffer.length()) {
      if (nullptr != _read_cb_obj) {
        if (0 == _read_cb_obj->provideBuffer(&_rx_buffer)) {
          _rx_buffer.clear();
        }
      }
    }
  }
  return return_value;
}


int8_t UARTAdapter::_pf_init() {
  int8_t ret = -1;
  switch (ADAPTER_NUM) {
    case 0:
      uart_instances[ADAPTER_NUM] = this;
      if (_sock) {
        close(_sock);
      }
      _sock = open(dev_path, O_RDWR | O_NOCTTY | O_SYNC);
      if (_sock == -1) {
        printf("Unable to open port: (%s)\n", dev_path);
        return -1;
      }
      printf("Opened port (%s) at %d\n", dev_path, _opts.bitrate);
      tcgetattr(_sock, &termAttr);
      cfsetspeed(&termAttr, _opts.bitrate);
      // TODO: These choices should come from _options. Find a good API to emulate.
      //    ---J. Ian Lindsay   Thu Dec 03 03:43:12 MST 2015
      termAttr.c_cflag &= ~PARENB;          // No parity
      termAttr.c_cflag &= ~CSTOPB;          // 1 stop bit
      termAttr.c_cflag &= ~CSIZE;           // Enable char size mask
      termAttr.c_cflag |= CS8;              // 8-bit characters
      termAttr.c_cflag |= (CLOCAL | CREAD);
      termAttr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      termAttr.c_iflag &= ~(IXON | IXOFF | IXANY);
      termAttr.c_oflag &= ~OPOST;

      if (tcsetattr(_sock, TCSANOW, &termAttr) == 0) {
        _adapter_set_flag(UART_FLAG_HAS_TX | UART_FLAG_HAS_RX);
        _adapter_set_flag(UART_FLAG_UART_READY | UART_FLAG_FLUSHED);
        _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
        platform.createThread(&_thread_id, nullptr, uart_polling_handler, (void*) this, nullptr);
        ret = 0;
      }
      else {
        _adapter_clear_flag(UART_FLAG_UART_READY);
        printf("Failed to tcsetattr...\n");
      }
      break;
  }
  return ret;
}


int8_t UARTAdapter::_pf_deinit() {
  int8_t ret = -2;
  _adapter_clear_flag(UART_FLAG_UART_READY | UART_FLAG_PENDING_RESET | UART_FLAG_PENDING_CONF);
  if (ADAPTER_NUM < 3) {
    if (_sock) {
      close(_sock);  // Close the socket.
      _sock = 0;
    }
    if (dev_path) {
      free(dev_path);
      dev_path = nullptr;
    }
    if (txCapable()) {
      //uart_wait_tx_idle_polling((uart_port_t) ADAPTER_NUM);
      _adapter_set_flag(UART_FLAG_FLUSHED);
    }
    ret = 0;
  }
  return ret;
}


/*
* Write to the UART.
*/
uint UARTAdapter::write(uint8_t* buf, uint len) {
  if (txCapable()) {
    _tx_buffer.concat(buf, len);
    _adapter_clear_flag(UART_FLAG_FLUSHED);
  }
  return len;  // TODO: StringBuilder needs an API enhancement to make this safe.
}


/*
* Write to the UART.
*/
uint UARTAdapter::write(char c) {
  if (txCapable()) {
    _tx_buffer.concat(c);
    _adapter_clear_flag(UART_FLAG_FLUSHED);
  }
  return 1;  // TODO: StringBuilder needs an API enhancement to make this safe.
}


/*
* Read from the class buffer.
*/
uint UARTAdapter::read(uint8_t* buf, uint len) {
  uint ret = strict_min((int32_t) len, (int32_t) _rx_buffer.length());
  if (0 < ret) {
    memcpy(buf, _rx_buffer.string(), ret);
    _rx_buffer.cull(ret);
  }
  return ret;
}


/*
* Read from the class buffer.
*/
uint UARTAdapter::read(StringBuilder* buf) {
  uint ret = _rx_buffer.length();
  if (0 < ret) {
    buf->concatHandoff(&_rx_buffer);
  }
  return ret;
}


//
// /**
// * Debug support method. This fxn is only present in debug builds.
// *
// * @param   StringBuilder* The buffer into which this fxn should write its output.
// */
// void ManuvrSerial::printDebug(StringBuilder *temp) {
//   temp->concatf("-- dev_path        %s\n",     dev_path);
//   temp->concatf("-- _options        0x%08x\n", _options);
//   temp->concatf("-- _sock           0x%08x\n", _sock);
//   temp->concatf("-- Baud            %d\n",     _baud_rate);
//   temp->concatf("-- Class size      %d\n",     sizeof(ManuvrSerial));
// }


LinuxUART::LinuxUART(char* path) : UARTAdapter(0, 0, 0, 0, 0, 256, 256) {
  int slen = strlen(path);
  dev_path = (char*) malloc(slen+1);
  if (dev_path) {
    memcpy(dev_path, path, slen);
    *(dev_path + slen) = '\0';
  }
}


LinuxUART::~LinuxUART() {
  // if (dev_path) {
  //   free(dev_path);
  //   dev_path = nullptr;
  // }
}
