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
* Since linux identifies UARTs by string ("/dev/ttyACMx", or some such), we need
*   a small wrapper class to allow instancing this way, and forming the
*   associated bridge to the CppPotpourri classes.
*******************************************************************************/

typedef struct {
  UARTAdapter*   instance; // Tracks the UARTAdapter representing it to the rest of CppPotpourri.
  char*          path;     // This tracks the device under Linux.
  int            sock;     // This tracks the open port under Linux.
  struct termios termAttr; // This tracks the port settings under Linux.
} LinuxUARTLookup;


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

unsigned long _uart_polling_thread_id = 0;
static LinkedList<LinuxUARTLookup*> uart_instances;


static LinuxUARTLookup* _uart_table_get_by_adapter_ref(UARTAdapter* adapter) {
  for (int i = 0; i < uart_instances.size(); i++) {
    LinuxUARTLookup* temp = uart_instances.get(i);
    if (temp->instance == adapter) return temp;
  }
  return nullptr;
}



/**
* This is a thread to keep the UARTs churning.
*/
static void* uart_polling_handler(void*) {
  bool keep_polling = true;
  printf("Started UART polling thread.\n");
  while (keep_polling) {
    for (int i = 0; i < uart_instances.size(); i++) {
      LinuxUARTLookup* temp = uart_instances.get(i);
      if (nullptr != temp) {
        if (temp->instance->initialized()) {
          temp->instance->poll();
        }
      }
    }
    keep_polling = (0 < uart_instances.size());
  }
  printf("Exiting UART polling thread...\n");
  _uart_polling_thread_id = 0;  // Allow the thread to be restarted later.
  return NULL;
}


/*******************************************************************************
* UART wrapper class
*******************************************************************************/

/**
* Constructor will allocate memory for its lookup list item and the path string.
* Adds itself to the lookup list.
*
* NOTE: Because this constructor modifies static data, it can't be relied upon
*   if an instance of it is ever allocated statically. So don't do that.
*/
LinuxUART::LinuxUART(char* path) : UARTAdapter(0, 0, 0, 0, 0, 256, 256) {
  const int slen = strlen(path);
  LinuxUARTLookup* lookup = (LinuxUARTLookup*) malloc(sizeof(LinuxUARTLookup));
  if (lookup) {
    bzero(lookup, sizeof(LinuxUARTLookup));
    lookup->instance = (UARTAdapter*) this;
    lookup->path     = (char*) malloc(slen+1);
    lookup->sock     = -1;
    if (lookup->path) {
      memcpy(lookup->path, path, slen);
      *(lookup->path + slen) = '\0';
      uart_instances.insert(lookup);
    }
    else {
      free(lookup);
    }
  }
}


/**
* Destructor will de-init the hardware, remove itself from the lookup list,
*   and free any memory it used to store itself.
*/
LinuxUART::~LinuxUART() {
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  _pf_deinit();
  if (nullptr != lookup) {
    uart_instances.remove(lookup);
    free(lookup->path);
    lookup->path = nullptr;
    free(lookup);
  }
}


/*******************************************************************************
* Implementation of UARTAdapter.
*******************************************************************************/

/*
* This doesn't get used on Linux.
*/
void UARTAdapter::irq_handler() {}


/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 or greater on success.
*/
int8_t UARTAdapter::poll() {
  int8_t return_value = 0;
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (nullptr != lookup) {
    if (lookup->sock != -1) {
      int bytes_written = 0;
      int bytes_received = 0;
      if (txCapable() && (0 < _tx_buffer.length())) {
        // Refill the TX buffer...
        int tx_count = strict_min((int32_t) 64, (int32_t) _tx_buffer.length());
        if (0 < tx_count) {
          bytes_written = (int) ::write(lookup->sock, _tx_buffer.string(), tx_count);
          _tx_buffer.cull(bytes_written);
          _adapter_set_flag(UART_FLAG_FLUSHED, _tx_buffer.isEmpty());
        }
      }
      if (rxCapable()) {
        uint8_t* buf = (uint8_t*) alloca(255);
        int n = ::read(lookup->sock, buf, 255);
        if (n > 0) {
          bytes_received += n;
          _rx_buffer.concat(buf, n);
          return_value = 1;
        }
        if (0 < _rx_buffer.length()) {
          if (nullptr != _read_cb_obj) {
            if (0 == _read_cb_obj->provideBuffer(&_rx_buffer)) {
              _rx_buffer.clear();
            }
          }
        }
      }
    }
  }
  return return_value;
}


int8_t UARTAdapter::_pf_init() {
  int8_t ret = -1;
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (nullptr != lookup) {
    _adapter_clear_flag(UART_FLAG_UART_READY);
    if (0 < lookup->sock) {
      close(lookup->sock);
    }
    lookup->sock = open(lookup->path, O_RDWR | O_NOCTTY | O_SYNC);
    if (lookup->sock != -1) {
      printf("Opened port (%s) at %dbps\n", lookup->path, _opts.bitrate);
      tcgetattr(lookup->sock, &(lookup->termAttr));
      cfsetspeed(&(lookup->termAttr), _opts.bitrate);
      // TODO: These choices should come from _options. Find a good API to emulate.
      //    ---J. Ian Lindsay   Thu Dec 03 03:43:12 MST 2015
      lookup->termAttr.c_cflag &= ~PARENB;          // No parity
      lookup->termAttr.c_cflag &= ~CSTOPB;          // 1 stop bit
      lookup->termAttr.c_cflag &= ~CSIZE;           // Enable char size mask
      lookup->termAttr.c_cflag |= CS8;              // 8-bit characters
      lookup->termAttr.c_cflag |= (CLOCAL | CREAD);
      lookup->termAttr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      lookup->termAttr.c_iflag &= ~(IXON | IXOFF | IXANY);
      lookup->termAttr.c_oflag &= ~OPOST;
      if (tcsetattr(lookup->sock, TCSANOW, &(lookup->termAttr)) == 0) {
        _adapter_set_flag(UART_FLAG_HAS_TX | UART_FLAG_HAS_RX);
        _adapter_set_flag(UART_FLAG_UART_READY | UART_FLAG_FLUSHED);
        _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
        if (0 == _uart_polling_thread_id) {
          platform.createThread(&_uart_polling_thread_id, nullptr, uart_polling_handler, nullptr, nullptr);
        }
        ret = 0;
      }
      else {
        printf("Failed to tcsetattr...\n");
      }
    }
    else {
      printf("Unable to open port: (%s)\n", lookup->path);
    }
  }
  return ret;
}


int8_t UARTAdapter::_pf_deinit() {
  int8_t ret = -2;
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (nullptr != lookup) {
    _adapter_clear_flag(UART_FLAG_UART_READY | UART_FLAG_PENDING_RESET | UART_FLAG_PENDING_CONF);
    if (txCapable()) {
      //uart_wait_tx_idle_polling((uart_port_t) ADAPTER_NUM);
      _adapter_set_flag(UART_FLAG_FLUSHED);
    }
    if (0 < lookup->sock) {
      close(lookup->sock);  // Close the socket.
      lookup->sock = -1;
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
