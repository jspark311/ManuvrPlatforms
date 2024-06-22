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
  //StringBuilder  unpushed_rx;  // This eases the conversion to a reliable BufferAccepter.
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
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Started UART polling thread.\n");
  while (keep_polling) {
    sleep_ms(20);
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
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Exiting UART polling thread...\n");
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
LinuxUART::LinuxUART(char* path) : UARTAdapter(0, 0, 0, 0, 0, 1024, 1024) {
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


char* LinuxUART::path() {
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (lookup) {
    return lookup->path;
  }
  return "";
}

/*******************************************************************************
* Implementation of UARTAdapter.
*******************************************************************************/

/*
* This doesn't get used on Linux.
*/
void LinuxUART::irq_handler() {}



/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 on no action, 1 on successful action, -1 on error.
*/
int8_t LinuxUART::_pf_poll() {
  int8_t return_value = 0;
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (nullptr != lookup) {
    if (lookup->sock > 0) {
      if (txCapable() & (0 < _tx_buffer.count())) {
        // Refill the TX buffer...
        const uint32_t TX_COUNT = strict_min((uint32_t) 64, _tx_buffer.count());
        if (0 < TX_COUNT) {
          uint8_t side_buffer[TX_COUNT] = {0, };
          const int32_t PEEK_COUNT = _tx_buffer.peek(side_buffer, TX_COUNT);
          const int BYTES_WRITTEN = (int) ::write(lookup->sock, side_buffer, TX_COUNT);

          //StringBuilder tmp_log;
          //tmp_log.concatf("\n\n__________Bytes written (%d)________\n", bytes_written);
          //_tx_buffer.printDebug(&tmp_log);
          //printf("%s\n\n", tmp_log.string());
          //_tx_buffer.cull(bytes_written);
          _flushed = _tx_buffer.isEmpty();
          if (BYTES_WRITTEN > 0) {
            _tx_buffer.cull(BYTES_WRITTEN);
            return_value |= 1;
          }
        }
      }
      if (rxCapable()) {
        const uint32_t RX_COUNT = strict_min((uint32_t) _rx_buffer.vacancy(), (uint32_t) 255);
        uint8_t buf[RX_COUNT] = {0, };
        int n = ::read(lookup->sock, buf, RX_COUNT);
        if (n > 0) {
          _rx_buffer.insert(buf, n);
          last_byte_rx_time = millis();
          //StringBuilder tmp_log;
          //tmp_log.concatf("\n\n__________Bytes read (%d)________\n", n);
          //_rx_buffer.printDebug(&tmp_log);
          //printf("%s\n\n", tmp_log.string());
        }
        if (0 < _handle_rx_push()) {
          return_value |= 1;
        }
      }
    }
  }
  return return_value;
}


int8_t LinuxUART::_pf_init() {
  int8_t ret = -1;
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (nullptr != lookup) {
    _adapter_clear_flag(UART_FLAG_UART_READY);
    if (0 < lookup->sock) {
      close(lookup->sock);
    }
    lookup->sock = open(lookup->path, O_RDWR | O_NOCTTY | O_SYNC);
    if (lookup->sock != -1) {
      tcgetattr(lookup->sock, &(lookup->termAttr));
      cfsetspeed(&(lookup->termAttr), _opts.bitrate);
      lookup->termAttr.c_cflag &= ~CSIZE;           // Enable char size mask
      switch (_opts.bit_per_word) {
        case 5:  lookup->termAttr.c_cflag |= CS5;  break;
        case 6:  lookup->termAttr.c_cflag |= CS6;  break;
        case 7:  lookup->termAttr.c_cflag |= CS7;  break;
        case 8:  lookup->termAttr.c_cflag |= CS8;  break;
        default:
          c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "%d bis-per-word is invalid for %s.\n", _opts.bit_per_word, lookup->path);
          return ret;
      }
      switch (_opts.parity) {
        case UARTParityBit::NONE:  lookup->termAttr.c_cflag &= ~PARENB;            break;
        case UARTParityBit::EVEN:  lookup->termAttr.c_cflag |= PARENB;             break;
        case UARTParityBit::ODD:   lookup->termAttr.c_cflag |= (PARENB | PARODD);  break;
        default:
          c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Invalid parity selection for %s.\n", lookup->path);
          return ret;
      }
      switch (_opts.stop_bits) {
        case UARTStopBit::STOP_1:  lookup->termAttr.c_cflag &= ~CSTOPB;  break;
        case UARTStopBit::STOP_2:  lookup->termAttr.c_cflag |= CSTOPB;   break;
        default:
          c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unsupported stop-bit selection for %s.\n", lookup->path);
          return ret;
      }
      switch (_opts.flow_control) {
        case UARTFlowControl::NONE:     lookup->termAttr.c_cflag |= CLOCAL;   break;
        case UARTFlowControl::RTS_CTS:  lookup->termAttr.c_cflag |= CRTSCTS;  break;
        default:
          c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unsupported flow control selection for %s.\n", lookup->path);
          return ret;
      }
      // If an input buffer was desired, we turn on RX.
      if (0 < _rx_buffer.capacity()) {
        lookup->termAttr.c_cflag |= CREAD;
        _adapter_set_flag(UART_FLAG_HAS_RX);
      }

      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Opened UART (%s) at %dbps\n", lookup->path, _opts.bitrate);
      lookup->termAttr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      lookup->termAttr.c_iflag &= ~(IXON | IXOFF | IXANY);
      lookup->termAttr.c_oflag &= ~OPOST;
      lookup->termAttr.c_cc[VMIN]  = 0;
      lookup->termAttr.c_cc[VTIME] = 0;
      if (tcsetattr(lookup->sock, TCSANOW, &(lookup->termAttr)) == 0) {
        _adapter_set_flag(UART_FLAG_UART_READY | UART_FLAG_HAS_TX);
        _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
        _flushed = true;
        if (0 == _uart_polling_thread_id) {
          platform.createThread(&_uart_polling_thread_id, nullptr, uart_polling_handler, nullptr, nullptr);
        }
        ret = 0;
      }
      else {
        c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to tcsetattr...\n");
      }
    }
    else {
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unable to open port: (%s)\n", lookup->path);
    }
  }
  return ret;
}


int8_t LinuxUART::_pf_deinit() {
  int8_t ret = -2;
  LinuxUARTLookup* lookup = _uart_table_get_by_adapter_ref(this);
  if (nullptr != lookup) {
    _adapter_clear_flag(UART_FLAG_UART_READY | UART_FLAG_PENDING_RESET | UART_FLAG_PENDING_CONF);
    if (txCapable()) {
      //uart_wait_tx_idle_polling((uart_port_t) ADAPTER_NUM);
      _flushed = true;
    }
    if (0 < lookup->sock) {
      close(lookup->sock);  // Close the socket.
      lookup->sock = -1;
      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Closed UART (%s)\n", lookup->path);
    }
    _tx_buffer.clear();
    _rx_buffer.clear();
    ret = 0;
  }
  return ret;
}
