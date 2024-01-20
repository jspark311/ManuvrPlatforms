/*
File:   I2CAdapter.h
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

*/

#include "../ESP32.h"
#include <BusQueue/I2CAdapter.h>
#include "driver/i2c.h"


TaskHandle_t static_i2c_thread_id[2] = {0, 0};
//static const char* LOG_TAG = "I2CAdapter";


static void* IRAM_ATTR i2c_worker_thread(void* arg) {
  I2CAdapter* BUSPTR = (I2CAdapter*) arg;
  //uint8_t anum = BUSPTR->adapterNumber();
  while (1) {
    switch (BUSPTR->poll()) {
      case PollResult::NO_ACTION:
        //platform.suspendThread();
        platform.yieldThread();
      case PollResult::ACTION:
      default:
        break;
    }
  }
  return nullptr;
}



/*******************************************************************************
* ___     _                                  This is a template class for
*  |   / / \ o    /\   _|  _. ._ _|_  _  ._  defining arbitrary I/O adapters.
* _|_ /  \_/ o   /--\ (_| (_| |_) |_ (/_ |   Adapters must be instanced with
*                             |              a BusOp as the template param.
*******************************************************************************/

int8_t I2CAdapter::_bus_init() {
  int8_t ret = -1;
  i2c_config_t conf;
  conf.mode             = I2C_MODE_MASTER;  // TODO: We only support master mode right now.
  conf.sda_io_num       = (gpio_num_t) _bus_opts.sda_pin;
  conf.scl_io_num       = (gpio_num_t) _bus_opts.scl_pin;
  conf.sda_pullup_en    = _bus_opts.sdaPullup() ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  conf.scl_pullup_en    = _bus_opts.sclPullup() ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  conf.master.clk_speed = _bus_opts.freq;
  _pf_needs_op_advance(false);
  int a_id = adapterNumber();
  switch (a_id) {
    case 0:
    case 1:
      if (ESP_OK == i2c_param_config(((0 == a_id) ? I2C_NUM_0 : I2C_NUM_1), &conf)) {
        if (ESP_OK == i2c_driver_install(((0 == a_id) ? I2C_NUM_0 : I2C_NUM_1), conf.mode, 0, 0, 0)) {
          PlatformThreadOpts topts;
          topts.thread_name = (char*) "I2C";
          topts.stack_sz    = 2560;
          topts.priority    = 0;
          topts.core        = 1;   // TODO: Is this the best choice? Might use a preprocessor define.
          unsigned long _thread_id = 0;
          platform.createThread(&_thread_id, nullptr, i2c_worker_thread, (void*) this, &topts);
          static_i2c_thread_id[a_id] = (TaskHandle_t) _thread_id;
          c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Spawned i2c thread: %lu", _thread_id);
          ret = 0;
        }
      }
      break;

    default:
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Unsupported adapter: %d", a_id);
      break;
  }
  return ret;
}


int8_t I2CAdapter::_bus_deinit() {
  // TODO: This.
  _bus_online(false);
  return 0;
}


void I2CAdapter::printHardwareState(StringBuilder* output) {
  output->concatf("-- I2C%d (%sline)\n", adapterNumber(), (busOnline() ? "on":"OFF"));
}



/*******************************************************************************
* ___     _                              These members are mandatory overrides
*  |   / / \ o     |  _  |_              from the BusOp class.
* _|_ /  \_/ o   \_| (_) |_)
*******************************************************************************/

XferFault I2CBusOp::begin() {
  if (device) {
    switch (device->adapterNumber()) {
      case 0:
      case 1:
        if ((nullptr == callback) || (0 == callback->io_op_callahead(this))) {
          set_state(XferState::INITIATE);
          vTaskResume(static_i2c_thread_id[device->adapterNumber()]);
          return advance(0);
        }
        else {
          abort(XferFault::IO_RECALL);
        }
        break;
      default:
        abort(XferFault::BAD_PARAM);
        break;
    }
  }
  else {
    abort(XferFault::BUS_BUSY);
  }
  return getFault();
}


/*
* FreeRTOS doesn't have a concept of interrupt, but we might call this
*   from an I/O thread.
*/
XferFault I2CBusOp::advance(uint32_t status_reg) {
  if (XferState::INITIATE == get_state()) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (ESP_OK == i2c_master_start(cmd)) {
      switch (get_opcode()) {
        case BusOpcode::RX:
          if (need_to_send_subaddr()) {
            i2c_master_write_byte(cmd, ((uint8_t) (dev_addr & 0x00FF) << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK);
            i2c_master_write_byte(cmd, (uint8_t) (sub_addr & 0x00FF), I2C_MASTER_ACK);
            set_state(XferState::ADDR);
            i2c_master_start(cmd);
          }
          i2c_master_write_byte(cmd, ((uint8_t) (dev_addr & 0x00FF) << 1) | I2C_MASTER_READ, I2C_MASTER_ACK);
          i2c_master_read(cmd, _buf, (size_t) _buf_len, I2C_MASTER_LAST_NACK);
          set_state(XferState::RX_WAIT);
          break;
        case BusOpcode::TX:
          i2c_master_write_byte(cmd, ((uint8_t) (dev_addr & 0x00FF) << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK);
          if (need_to_send_subaddr()) {
            i2c_master_write_byte(cmd, (uint8_t) (sub_addr & 0x00FF), I2C_MASTER_ACK);
            set_state(XferState::ADDR);
          }
          i2c_master_write(cmd, _buf, (size_t) _buf_len, I2C_MASTER_NACK);
          set_state(XferState::TX_WAIT);
          break;
        case BusOpcode::TX_CMD:
          i2c_master_write_byte(cmd, ((uint8_t) (dev_addr & 0x00FF) << 1) | I2C_MASTER_WRITE, I2C_MASTER_NACK);
          set_state(XferState::TX_WAIT);
          break;
        default:
          abort(XferFault::BAD_PARAM);
          break;
      }

      if (ESP_OK == i2c_master_stop(cmd)) {
        set_state(XferState::STOP);
        int ret = i2c_master_cmd_begin((i2c_port_t) device->adapterNumber(), cmd, 800 / portTICK_RATE_MS);
        switch (ret) {
          case ESP_OK:                 markComplete();                   break;
          case ESP_ERR_INVALID_ARG:    abort(XferFault::BAD_PARAM);      break;
          case ESP_ERR_INVALID_STATE:  abort(XferFault::ILLEGAL_STATE);  break;
          case ESP_ERR_TIMEOUT:        abort(XferFault::TIMEOUT);        break;
          case ESP_FAIL:               abort(XferFault::DEV_FAULT);      break;
          default:                     abort();                          break;
        }
      }
      else {
        abort();
      }
    }
    else {
      abort(XferFault::BUS_FAULT);
    }
    i2c_cmd_link_delete(cmd);  // Cleanup.
  }
  else {
    abort(XferFault::ILLEGAL_STATE);
  }

  if (hasFault() & (BusOpcode::TX_CMD != get_opcode())) {
    c3p_log(LOG_LEV_WARN, __PRETTY_FUNCTION__, "BusOp to dev 0x%02x failed: %s", dev_addr, BusOp::getErrorString(getFault()));
  }
  return getFault();
}
