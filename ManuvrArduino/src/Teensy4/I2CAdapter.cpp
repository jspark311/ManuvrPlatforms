/*
* Teensy4
*/
#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

#include <AbstractPlatform.h>
#include <I2CAdapter.h>
#include "ManuvrArduino.h"
#include "imx_rt1060/imx_rt1060_i2c_driver.h"


I2CMaster& master0 = Master;     // Pins 19 and 18; SCL0 and SDA0
I2CMaster& master1 = Master1;    // Pins 16 and 17; SCL1 and SDA1

#define ACK_CHECK_EN   0x01     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x00     /*!< I2C master will not check ack from slave */



/*******************************************************************************
* ___     _                                  This is a template class for
*  |   / / \ o    /\   _|  _. ._ _|_  _  ._  defining arbitrary I/O adapters.
* _|_ /  \_/ o   /--\ (_| (_| |_) |_ (/_ |   Adapters must be instanced with
*                             |              a BusOp as the template param.
*******************************************************************************/

/*
* Init the hardware for the bus.
* There is only one correct pin combination for each i2c bus (surprisingly).
*/
int8_t I2CAdapter::bus_init() {
  switch (ADAPTER_NUM) {
    case 0:
      if ((18 == _bus_opts.sda_pin) && (19 == _bus_opts.scl_pin)) {
        master0.begin(_bus_opts.freq);
        busOnline(true);
      }
      break;
    case 1:
      if ((17 == _bus_opts.sda_pin) && (16 == _bus_opts.scl_pin)) {
        master1.begin(_bus_opts.freq);
        busOnline(true);
      }
      break;
    default:
      break;
  }
  return (busOnline() ? 0:-1);
}


int8_t I2CAdapter::bus_deinit() {
  busOnline(false);
  switch (ADAPTER_NUM) {
    case 0:
      break;
    case 1:
      break;
    default:
      return -1;
  }
  return 0;
}



void I2CAdapter::printHardwareState(StringBuilder* output) {
  output->concatf("-- I2C%d (%sline)\n", adapterNumber(), (_adapter_flag(I2C_BUS_FLAG_BUS_ONLINE)?"on":"OFF"));
}


int8_t I2CAdapter::generateStart() {
  return busOnline() ? 0 : -1;
}


int8_t I2CAdapter::generateStop() {
  return busOnline() ? 0 : -1;
}



/*******************************************************************************
* ___     _                              These members are mandatory overrides
*  |   / / \ o     |  _  |_              from the BusOp class.
* _|_ /  \_/ o   \_| (_) |_)
*******************************************************************************/

XferFault I2CBusOp::begin() {
  I2CMaster* adptr = nullptr;
  uint8_t ord = 0;
  switch (device->adapterNumber()) {
    case 0:    adptr = &master0;             break;
    case 1:    adptr = &master1;             break;
    default:   abort(XferFault::BAD_PARAM);  return getFault();
  }
  set_state(XferState::INITIATE);  // Indicate that we now have bus control.

  switch (get_opcode()) {
    case BusOpcode::TX_CMD:
      set_state(XferState::TX_WAIT);
      adptr->write_async(dev_addr, _buf, 0, true);
      break;

    case BusOpcode::TX:
      if (need_to_send_subaddr()) {
        set_state(XferState::ADDR);
        adptr->write_async(dev_addr, (uint8_t*) &sub_addr, 1, false);
      }
      else {
        set_state(XferState::TX_WAIT);
        adptr->write_async(dev_addr, _buf, _buf_len, true);
      }
      break;

    case BusOpcode::RX:
      if (need_to_send_subaddr()) {
        set_state(XferState::ADDR);
        adptr->write_async(dev_addr, (uint8_t*) &sub_addr, 1, false);
      }
      else {
        set_state(XferState::RX_WAIT);
        adptr->read_async(dev_addr, _buf, _buf_len, true);
      }
      break;

    default:
      abort(XferFault::BAD_PARAM);
      break;
  }
  return getFault();
}




/*
* FreeRTOS doesn't have a concept of interrupt, but we might call this
*   from an I/O thread.
*/
XferFault I2CBusOp::advance(uint32_t status_reg) {
  XferFault ret = XferFault::NONE;
  I2CMaster* adptr = nullptr;
  switch (device->adapterNumber()) {
    case 0:    adptr = &master0;             break;
    case 1:    adptr = &master1;             break;
    default:   abort(XferFault::BAD_PARAM);  return getFault();
  }
  switch (get_state()) {
    case XferState::UNDEF:
    case XferState::IDLE:
    case XferState::QUEUED:
    case XferState::INITIATE:
      break;
    case XferState::ADDR:
      if (adptr->finished()) {
        switch (get_opcode()) {
          case BusOpcode::RX:
            if (adptr->get_bytes_transferred() == 1) {
              set_state(XferState::RX_WAIT);
              adptr->read_async(dev_addr, _buf, _buf_len, true);
            }
            else {
              abort(XferFault::DEV_NOT_FOUND);
            }
            break;
          case BusOpcode::TX:
            if (adptr->get_bytes_transferred() == 1) {
              set_state(XferState::TX_WAIT);
              adptr->write_async(dev_addr, _buf, _buf_len, true);
            }
            else {
              abort(XferFault::DEV_NOT_FOUND);
            }
            break;
          default:
            abort(XferFault::ILLEGAL_STATE);
            break;
        }
      }
      break;
    case XferState::TX_WAIT:
    case XferState::RX_WAIT:
      if (adptr->finished()) {
        switch (get_opcode()) {
          case BusOpcode::RX:
          case BusOpcode::TX:
            if (adptr->get_bytes_transferred() != _buf_len) {
              abort(XferFault::BUS_FAULT);
            }
            else {
              markComplete();
            }
            break;
          case BusOpcode::TX_CMD:
            if (adptr->get_bytes_transferred() > 1) {
              abort(XferFault::DEV_NOT_FOUND);
            }
            else {
              markComplete();
            }
            break;
          default:
            abort(XferFault::ILLEGAL_STATE);
            break;
        }
      }
      break;
    case XferState::STOP:
      markComplete();
      break;
    case XferState::COMPLETE:
    case XferState::FAULT:
    default:
      break;
  }
  return getFault();
}

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
