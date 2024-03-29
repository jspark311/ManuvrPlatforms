/*
File:   SPIAdapter.cpp
Author: J. Ian Lindsay
Date:   2016.12.17

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


This is a peripheral wraspper around the Linux SPI driver.

On Linux, we don't deal with chip-select in the same manner as other platforms,
  since it is not under our direct control.

For instance: On a RasPi v1 with the kernel driver loaded we have...
    /dev/spidev0.0
    /dev/spidev0.1
  ...for CS0 and CS1.
*/

#include "../Linux.h"
#include <AbstractPlatform.h>
#include <BusQueue/SPIAdapter.h>

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/*******************************************************************************
* ___     _                                  This is a template class for
*  |   / / \ o    /\   _|  _. ._ _|_  _  ._  defining arbitrary I/O adapters.
* _|_ /  \_/ o   /--\ (_| (_| |_) |_ (/_ |   Adapters must be instanced with
*                             |              a BusOp as the template param.
*******************************************************************************/
void SPIAdapter::printDebug(StringBuilder* output) {
  printAdapter(output);
  printHardwareState(output);
}

void SPIAdapter::printHardwareState(StringBuilder* output) {
}


FAST_FUNC int8_t SPIAdapter::frequency(const uint32_t f) {
  int8_t ret = -1;
  if (0 < f) {
    ret--;
    if (2 > ADAPTER_NUM) {
      _current_freq = f;
      ret = 0;
    }
  }
  return ret;
}


FAST_FUNC int8_t SPIAdapter::setMode(const uint8_t m) {
  int8_t   ret = -2;
  if (2 > ADAPTER_NUM) {
    ret = 0;
    switch (m) {
      case 0:   clock_mode = SPI_MODE0;   break;
      case 1:   clock_mode = SPI_MODE1;   break;
      case 2:   clock_mode = SPI_MODE2;   break;
      case 3:   clock_mode = SPI_MODE3;   break;
      default:  ret = -3;   break;
    }
  }
  return ret;
}


int8_t SPIAdapter::_bus_init() {
  return -1;
}


int8_t SPIAdapter::_bus_deinit() {
  return 0;
}


/**
* This is called when the kernel attaches the module.
* This is the first time the class can be expected to have kernel access.
*
* @return 0 on no action, 1 on action, -1 on failure.
*/
int8_t SPIAdapter::attached() {
  if (EventReceiver::attached()) {
    // We should init the SPI library...
    //SPI.begin();
    //SPI.setDataMode(SPI_MODE0);
    //SPI.setClockDivider(SPI_CLOCK_DIV32);
    return 1;
  }
  return 0;
}


/**
* Calling this member will cause the bus operation to be started.
*
* @return 0 on success, or non-zero on failure.
*/
XferFault SPIBusOp::begin() {
  //time_began    = micros();
  //if (0 == _param_len) {
  //  // Obvious invalidity. We must have at least one transfer parameter.
  //  abort(XferFault::BAD_PARAM);
  //  return XferFault::BAD_PARAM;
  //}

  //if (SPI1->SR & SPI_FLAG_BSY) {
  //  Kernel::log("SPI op aborted before taking bus control.\n");
  //  abort(XferFault::BUS_BUSY);
  //  return XferFault::BUS_BUSY;
  //}

  set_state(XferState::INITIATE);  // Indicate that we now have bus control.

  // NOTE: The linux SPI driver abstracts chip-select pins away from us. If the
  //  CS pin number is set to 255, this fxn call loses its hardware implications
  //  and degrades into a state-tracking marker.
  _assert_cs(true);

  if (_param_len) {
    set_state(XferState::ADDR);
    for (int i = 0; i < _param_len; i++) {
      SPI.transfer(xfer_params[i]);
    }
  }

  if (buf_len) {
    set_state((opcode == BusOpcode::TX) ? XferState::TX_WAIT : XferState::RX_WAIT);
    for (int i = 0; i < buf_len; i++) {
      SPI.transfer(*(buf + i));
    }
  }

  markComplete();
  return XferFault::NONE;
}


/**
* Called from the ISR to advance this operation on the bus.
* Stay brief. We are in an ISR.
*
* @return 0 on success. Non-zero on failure.
*/
int8_t SPIBusOp::advance_operation(uint32_t status_reg, uint8_t data_reg) {
  //debug_log.concatf("advance_op(0x%08x, 0x%02x)\n\t %s\n\t status: 0x%08x\n", status_reg, data_reg, getStateString(), (unsigned long) hspi1.State);
  //Kernel::log(&debug_log);

  /* These are our transfer-size-invariant cases. */
  switch (xfer_state) {
    case XferState::COMPLETE:
      abort(XferFault::HUNG_IRQ);
      return 0;

    case XferState::TX_WAIT:
    case XferState::RX_WAIT:
      markComplete();
      return 0;

    case XferState::FAULT:
      return 0;

    case XferState::QUEUED:
    case XferState::ADDR:
    case XferState::STOP:
    case XferState::UNDEF:

    /* Below are the states that we shouldn't be in at this point... */
    case XferState::INITIATE:
    case XferState::IDLE:
      abort(XferFault::ILLEGAL_STATE);
      return 0;
  }

  return -1;
}
